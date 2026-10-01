import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('client_build',Path(__file__).with_name('client_build.py'))
build=importlib.util.module_from_spec(spec);spec.loader.exec_module(build)

class MemoryFile:
    def __init__(self, content, fail=False):self.content=content;self.fail=fail
    def read(self):return self.content
    def write(self, content):
        if self.fail:self.fail=False;raise OSError('simulated second-file failure')
        self.content=content

class BuildTests(unittest.TestCase):
    def test_rollback_if_second_file_write_fails(self):
        a,b=MemoryFile(b'old DLL'),MemoryFile(b'old JAR',True)
        with self.assertRaises(OSError):build.commit_pair([a,b],[b'new DLL',b'new JAR'])
        self.assertEqual((a.read(),b.read()),(b'old DLL',b'old JAR'))

    def prepare(self, root):
        project=root/'project';output=root/'output';(project/'bin').mkdir(parents=True);output.mkdir()
        for name in build.NAMES:
            (project/'bin'/name).write_bytes(('old '+name).encode());(output/name).write_bytes(('new '+name).encode())
        (output/'manifest.json').write_text(json.dumps({'artifacts':{name:build.digest(output/name) for name in build.NAMES}}))
        return project,output

    def test_corrupt_artifact_changes_no_installed_binary(self):
        with tempfile.TemporaryDirectory() as directory:
            project,output=self.prepare(Path(directory));(output/build.NAMES[1]).write_bytes(b'corrupt')
            with self.assertRaises(RuntimeError):build.install(project,output)
            self.assertTrue((project/'bin'/build.NAMES[0]).read_bytes().startswith(b'old'))
            self.assertTrue((project/'bin'/build.NAMES[1]).read_bytes().startswith(b'old'))

    def test_locked_second_binary_changes_neither_target(self):
        with tempfile.TemporaryDirectory() as directory:
            project,output=self.prepare(Path(directory));held=build.WindowsBinary(project/'bin'/build.NAMES[1])
            try:
                with self.assertRaises(OSError):build.install(project,output)
                self.assertTrue((project/'bin'/build.NAMES[0]).read_bytes().startswith(b'old'))
            finally:held.close()
            self.assertTrue((project/'bin'/build.NAMES[1]).read_bytes().startswith(b'old'))

    def test_real_two_file_install_and_backup(self):
        with tempfile.TemporaryDirectory() as directory:
            project,output=self.prepare(Path(directory));build.install(project,output,dry_run=True)
            self.assertFalse((output/'installed.json').exists())
            build.install(project,output)
            result=json.loads((output/'installed.json').read_text())
            for name in build.NAMES:
                self.assertEqual(build.digest(project/'bin'/name),build.digest(output/name))
                self.assertTrue((Path(result['backup'])/name).read_bytes().startswith(b'old'))

if __name__=='__main__':unittest.main()
