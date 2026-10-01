"""Offline local client build. DLL and bridge JAR remain separate artifacts."""
import argparse
import ctypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET
import zipfile

PROJECT = Path(__file__).resolve().parent.parent
NAMES = ('fusion-plus.dll', 'SeedCrackerBridge.jar')

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def properties(path):
    return dict(re.findall(r'^([\w.]+)\s*=\s*(.*)$', Path(path).read_text(), re.M))

def check_protocol(project, java_root):
    native = (project / 'src/input/assist_options.hpp').read_text()
    java = (java_root / 'src/main/java/phantomui/assist/AssistOptions.java').read_text()
    cpp = {k: int(v) for k, v in re.findall(r'^\s*(\w+)\s*=\s*(\d+),?$', native, re.M)}
    jvm = {k: int(v) for k, v in re.findall(r'static final int (\w+)=(\d+);', java)}
    if cpp != jvm or sorted(v for k, v in cpp.items() if k != 'COUNT') != list(range(cpp['COUNT'])):
        raise RuntimeError('C++ and Java JNI option layouts differ.')
    assignments = re.findall(r'values\[Protocol::Index\(Protocol::Option::(\w+)\)\]',
                            (project / 'src/input/utility_suite.hpp').read_text())
    if set(assignments) != set(cpp) - {'COUNT'} or len(assignments) != cpp['COUNT']:
        raise RuntimeError('Missing or duplicate native option assignment.')
    print('JNI layout verified: %d fields.' % cpp['COUNT'], flush=True)

def source_hashes(project, java_root):
    files = [p for p in (project / 'src').rglob('*') if p.suffix in ('.cpp', '.hpp', '.h')]
    files += [project / 'UniversalHookX.vcxproj', project / 'UniversalHookX.vcxproj.filters']
    files += list((java_root / 'src/main/java').rglob('*.java'))
    files += [java_root / 'gradle.properties']
    return {str(p.resolve()): digest(p) for p in files if p.is_file()}

def run(command, log, **kwargs):
    with open(log, 'wb') as output:
        result = subprocess.run(command, stdout=output, stderr=subprocess.STDOUT, **kwargs)
    if result.returncode:
        raise RuntimeError('Build command failed (%d); see %s' % (result.returncode, log))

def cache_jars(cache, group, artifact, version):
    found = sorted(p for p in (cache / group / artifact / version).rglob('*.jar')
                   if not p.name.endswith(('-sources.jar', '-javadoc.jar')))
    if not found:
        poms = list((cache / group / artifact / version).rglob('*.pom'))
        if poms and ET.parse(poms[0]).findtext('{http://maven.apache.org/POM/4.0.0}packaging') == 'pom':
            return []
        raise RuntimeError('Dependency not cached: %s:%s:%s. Run Gradle setup first.' % (group, artifact, version))
    return found

def compile_java(java_root, base_jar, cache, output):
    props = properties(java_root / 'gradle.properties')
    version = props['minecraft_version']
    mc = sorted((java_root / '.gradle/loom-cache/minecraftMaven').rglob('minecraft-merged-*-%s.jar' % version))
    if len(mc) != 1:
        raise RuntimeError('Expected one Loom Minecraft %s compile JAR; run Gradle setup first.' % version)
    api = cache / 'net.fabricmc.fabric-api/fabric-api' / props['fabric_api_version']
    poms = sorted(api.rglob('*.pom'))
    if len(poms) != 1:
        raise RuntimeError('Fabric API POM not cached for this project version.')
    ns = {'m': 'http://maven.apache.org/POM/4.0.0'}
    priority = []
    for dep in ET.parse(poms[0]).findall('.//m:dependency', ns):
        priority += cache_jars(cache, *(dep.find('m:' + key, ns).text for key in ('groupId', 'artifactId', 'version')))
    for artifact, prop in [('mc_math','math'), ('mc_seed','seed'), ('mc_core','core'), ('mc_noise','noise'),
                           ('mc_biome','biome'), ('mc_terrain','terrain'), ('mc_feature','feature'), ('mc_reversal','reversal')]:
        priority += cache_jars(cache, 'com.seedfinding', artifact, props['seedfinding_%s_version' % prop])
    priority += cache_jars(cache, 'com.seedfinding', 'latticg', props['latticg_version'])
    fallback = sorted(p for p in cache.rglob('*.jar') if not p.name.endswith(('-sources.jar', '-javadoc.jar')))
    fallback.sort(key=lambda p: 0 if ('/authlib/9.0.75/' in p.as_posix() or '/4.2.15.Final/' in p.as_posix()) else 1)
    cp = list(dict.fromkeys(mc + priority + fallback))
    classes = output / 'classes'
    classes.mkdir()
    sources = sorted((java_root / 'src/main/java').rglob('*.java'))
    if not sources:
        raise RuntimeError('No Java sources found.')
    args = output / 'javac.args'
    args.write_text('--release 25\n-encoding UTF-8\n-d "%s"\n-classpath "%s"\n%s\n' %
                    (classes.as_posix(), ';'.join(p.as_posix() for p in cp),
                     '\n'.join('"%s"' % p.as_posix() for p in sources)), encoding='utf-8')
    run(['javac', '@' + str(args)], output / 'java-build.log')
    # Preserve the working bootstrap JAR's resources and vendored libraries. Rebuild
    # all local Java classes and remove stale classes from local source packages.
    prefixes = {'phantomui/', 'kaptainwutax/seedcrackerX/'}
    jar = output / 'SeedCrackerBridge.jar'
    with zipfile.ZipFile(base_jar) as old, zipfile.ZipFile(jar, 'w', zipfile.ZIP_DEFLATED) as new:
        for info in old.infolist():
            if not (info.filename.endswith('.class') and any(info.filename.startswith(p) for p in prefixes)):
                new.writestr(info, old.read(info.filename))
        for file in sorted(classes.rglob('*.class')):
            new.write(file, file.relative_to(classes).as_posix())
    with zipfile.ZipFile(jar) as check:
        if len(check.namelist()) != len(set(check.namelist())) or check.testzip():
            raise RuntimeError('Invalid bridge JAR.')
        for entry in ('phantomui/assist/UtilitySuiteBridge.class', 'phantomui/seedcracker/SeedCrackerBridge.class'):
            check.getinfo(entry)
    return [str(p) for p in cp]

def find_vcvars():
    roots = [Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) / 'Microsoft Visual Studio/Installer/vswhere.exe']
    if roots[0].exists():
        result = subprocess.check_output([str(roots[0]), '-latest', '-prerelease', '-products', '*',
            '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
        if result:
            return Path(result) / 'VC/Auxiliary/Build/vcvars64.bat'
    raise RuntimeError('Visual Studio C++ tools not found; specify --vcvars.')

def compile_native(project, output, vcvars, vulkan):
    for path in (project, output, vcvars, vulkan):
        if any(c in str(path) for c in '&|<>^%\r\n"'):
            raise RuntimeError('Unsupported shell metacharacters in build path.')
    if not vcvars.is_file() or not (vulkan / 'Include/vulkan/vulkan.h').is_file():
        raise RuntimeError('Visual Studio / Vulkan SDK installation not found.')
    batch = output / 'native-build.cmd'
    batch.write_text('@call "%s" >nul\n@if errorlevel 1 exit /b %%errorlevel%%\n@set "VULKAN_SDK=%s"\n'
        '@msbuild "%s" /p:Configuration=Release /p:Platform=x64 /m /nr:false /v:minimal '
        '"/p:OutDir=%s/" "/p:IntDir=%s/"\n@exit /b %%errorlevel%%\n' %
        (vcvars, vulkan, project / 'UniversalHookX.vcxproj', output.as_posix(), (output / 'native-obj').as_posix()), encoding='utf-8')
    run(['cmd', '/d', '/c', str(batch)], output / 'native-build.log', cwd=project,
        env={k.upper(): v for k, v in os.environ.items()})
    dll = output / 'fusion-plus.dll'
    data = dll.read_bytes()
    offset = int.from_bytes(data[60:64], 'little')
    if data[:2] != b'MZ' or data[offset:offset+4] != b'PE\0\0' or data[offset+4:offset+6] != b'\x64\x86':
        raise RuntimeError('Native build did not produce an x64 DLL.')

def commit_pair(handles, payloads):
    backups = [handle.read() for handle in handles]
    try:
        for handle, content in zip(handles, payloads):
            handle.write(content)
        if any(handle.read() != content for handle, content in zip(handles, payloads)):
            raise RuntimeError('Installed data differs from the prepared artifact pair.')
    except Exception:
        for handle, original in zip(handles, backups):
            handle.write(original)
        raise

class WindowsBinary:
    """Hold both binary targets against concurrent writes for the entire transaction."""
    def __init__(self, path):
        self.path = path
        self.api = ctypes.WinDLL('kernel32', use_last_error=True)
        self.api.CreateFileW.restype = ctypes.c_void_p
        self.api.CreateFileW.argtypes = [ctypes.c_wchar_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p,ctypes.c_uint32,ctypes.c_uint32,ctypes.c_void_p]
        for name in ('CloseHandle','FlushFileBuffers','SetEndOfFile'):
            getattr(self.api,name).argtypes = [ctypes.c_void_p]
        self.api.SetFilePointerEx.argtypes = [ctypes.c_void_p,ctypes.c_longlong,ctypes.c_void_p,ctypes.c_uint32]
        self.api.WriteFile.argtypes = [ctypes.c_void_p,ctypes.c_void_p,ctypes.c_uint32,ctypes.POINTER(ctypes.c_uint32),ctypes.c_void_p]
        self.handle = self.api.CreateFileW(str(path), 0xC0000000, 1, None, 3, 0, None)
        if self.handle == ctypes.c_void_p(-1).value:
            raise OSError(ctypes.get_last_error(), 'Binary locked or inaccessible; close Minecraft before installing', str(path))
    def read(self):
        return self.path.read_bytes()
    def write(self, data):
        if not self.api.SetFilePointerEx(self.handle, 0, None, 0):
            raise ctypes.WinError(ctypes.get_last_error())
        written = ctypes.c_uint32()
        buffer = ctypes.create_string_buffer(data)
        if not self.api.WriteFile(self.handle, buffer, len(data), ctypes.byref(written), None) or written.value != len(data):
            raise ctypes.WinError(ctypes.get_last_error())
        if not self.api.SetEndOfFile(self.handle) or not self.api.FlushFileBuffers(self.handle):
            raise ctypes.WinError(ctypes.get_last_error())
    def close(self):
        self.api.CloseHandle(self.handle)

def install(project, output, dry_run=False):
    manifest = json.loads((output / 'manifest.json').read_text())
    for name in NAMES:
        if digest(output / name) != manifest['artifacts'][name]:
            raise RuntimeError('Artifact changed after build: ' + name)
    handles = []
    try:
        for name in NAMES:
            handles.append(WindowsBinary(project / 'bin' / name))
        print('Both binary targets writable; artifact hashes verified.', flush=True)
        if dry_run:
            return
        backup = output / ('backup-' + datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f'))
        backup.mkdir()
        for name, handle in zip(NAMES, handles):
            (backup / name).write_bytes(handle.read())
        commit_pair(handles, [(output / name).read_bytes() for name in NAMES])
        for name in NAMES:
            if digest(project / 'bin' / name) != manifest['artifacts'][name]:
                raise RuntimeError('Installed checksum mismatch: ' + name)
        (output / 'installed.json').write_text(json.dumps({'installed_at':datetime.datetime.now().isoformat(),
            'target':str(project / 'bin'), 'artifacts':manifest['artifacts'], 'backup':str(backup)}, indent=2))
        print('Installed matching DLL + JAR. Restart Minecraft to load the new bridge.', flush=True)
    finally:
        for handle in handles:
            handle.close()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('action', choices=['build','install'])
    parser.add_argument('--project', type=Path, default=PROJECT)
    parser.add_argument('--java-root', type=Path, default=PROJECT.parent.parent / 'SeedcrackerX-master')
    parser.add_argument('--cache', type=Path, default=Path.home() / '.gradle/caches/modules-2/files-2.1')
    parser.add_argument('--base-jar', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--vcvars', type=Path)
    parser.add_argument('--vulkan', type=Path)
    parser.add_argument('--dry-run', action='store_true')
    args = parser.parse_args();project=args.project.resolve();java_root=args.java_root.resolve()
    output = (args.output or project / 'obj/client-build').resolve()
    latest = project / 'obj/client-build/latest.json'
    if args.action == 'install':
        if args.output is None:
            output = Path(json.loads(latest.read_text())['output'])
        install(project, output, args.dry_run)
        return
    if args.dry_run:
        parser.error('--dry-run is for install only')
    check_protocol(project, java_root)
    hashes = source_hashes(project, java_root)
    output = output / datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f');output.mkdir(parents=True)
    base_jar = (args.base_jar or project / 'bin/SeedCrackerBridge.jar').resolve()
    base_hash = digest(base_jar)
    print('Building both artifacts in ' + str(output), flush=True)
    cp = compile_java(java_root, base_jar, args.cache.resolve(), output)
    vulkan = args.vulkan or (Path(os.environ['VULKAN_SDK']) if 'VULKAN_SDK' in os.environ else next(iter(sorted(Path('C:/VulkanSDK').glob('*'), reverse=True)), Path('missing')))
    compile_native(project, output, (args.vcvars or find_vcvars()).resolve(), vulkan.resolve())
    if hashes != source_hashes(project, java_root) or base_hash != digest(base_jar):
        raise RuntimeError('Inputs changed during build; no ready manifest generated.')
    manifest = {'built_at':datetime.datetime.now().isoformat(),'configuration':'Release|x64',
        'java_root':str(java_root), 'minecraft':properties(java_root / 'gradle.properties')['minecraft_version'],
        'bootstrap_jar':{'path':str(base_jar),'sha256':base_hash}, 'sources':hashes, 'classpath':cp,
        'artifacts':{name:digest(output / name) for name in NAMES}}
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2))
    latest.parent.mkdir(parents=True,exist_ok=True);latest.write_text(json.dumps({'output':str(output)},indent=2))
    print('READY: two separate artifacts. Install with Build-Client.ps1 -Action install.', flush=True)

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        print('ERROR: ' + str(error), file=sys.stderr)
        sys.exit(1)
