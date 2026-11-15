"""Compile a version-isolated 26.3 overlay using locally installed game/API jars."""
from pathlib import Path
import json, os, re, subprocess, zipfile

def build(java_root, base_cp, output, bridge_jar):
    meta=Path(os.environ.get('APPDATA',''))/'ModrinthApp/meta'
    games=sorted((meta/'versions').glob('26.3-*/26.3-*.jar'))
    if not games: raise RuntimeError('26.3 game JAR is required for the compatibility build.')
    game=games[-1]; version=json.loads(game.with_suffix('.json').read_text())
    folder=output/'compat263'; sources=folder/'sources'; classes=folder/'classes'
    sources.mkdir(parents=True,exist_ok=True); classes.mkdir(parents=True,exist_ok=True)
    cp=[game]
    for lib in version['libraries']:
        path=lib.get('downloads',{}).get('artifact',{}).get('path')
        if not path:
            part=lib['name'].split(':');path=part[0].replace('.','/')+'/'+part[1]+'/'+part[2]+'/'+part[1]+'-'+part[2]+'.jar'
        path=meta/'libraries'/path
        if path.is_file():cp.append(path)
    profiles=meta.parent/'profiles'
    api=[]
    for jar in profiles.glob('*26.3*/mods/*.jar'):
        try:
            with zipfile.ZipFile(jar) as file:
                info=json.loads(file.read('fabric.mod.json'))
                if info.get('id')=='fabric-api' and '26.3' in info.get('version',''):
                    libdir=folder/'api';libdir.mkdir(exist_ok=True)
                    for member in file.namelist():
                        if member.startswith('META-INF/jars/') and member.endswith('.jar'):
                            target=libdir/Path(member).name;target.write_bytes(file.read(member));api.append(target)
                    api.append(jar);break
        except (KeyError,ValueError,zipfile.BadZipFile):pass
    if not api:raise RuntimeError('Local Fabric API for 26.3 not found.')
    cp+=api+[Path(p) for p in base_cp if 'minecraft-merged' not in str(p)]
    transformations={
        'kaptainwutax/seedcrackerX/finder/structure/JunglePyramidFinder.java': [('RedStoneWireBlock','RedstoneWireBlock')],
        'kaptainwutax/seedcrackerX/command/DatabaseCommand.java':[('Util.getPlatform().openUri(DATABASE_URL)','com.mojang.blaze3d.Blaze3D.openUri(DATABASE_URL)')],
        'kaptainwutax/seedcrackerX/mixin/ClientPacketListenerMixin.java':[('packet.getX()','packet.x()'),('packet.getZ()','packet.z()')],
        'kaptainwutax/seedcrackerX/util/BiomeFixer.java':[('VanillaRegistries.createLookup()','VanillaRegistries.createWorldLookup()')],
        'phantomui/esp/PlayerSkinBridge.java':[
            ('var renderer=mc.getEntityRenderDispatcher().getPlayerRenderer(player);','var renderer=(net.minecraft.client.renderer.entity.player.AvatarRenderer)mc.getEntityRenderDispatcher().getRenderer(player);'),
            ('AvatarRenderState state=renderer.createRenderState(player,partial);','AvatarRenderState state=(AvatarRenderState)renderer.createRenderState(player,partial);'),
            ('public VertexConsumer setUv2(int u,int v) {return this;}','public VertexConsumer setUv2(int u,int v) {return this;}\n        public VertexConsumer setUv3(float u,float v) {return this;}')],
        'phantomui/oresim/Ore.java':[
            ('import net.minecraft.world.level.levelgen.feature.configurations.FeatureConfiguration;',''),
            ('import net.minecraft.world.level.levelgen.feature.configurations.OreConfiguration;','import net.minecraft.world.level.levelgen.feature.AbstractOreFeature;'),
            ('VanillaRegistries.createLookup()','VanillaRegistries.createWorldLookup()'),
            ('FeatureConfiguration featureConfig = feature.feature().value().config();','var featureConfig = feature.feature().value();'),
            ('featureConfig instanceof OreConfiguration oreFeatureConfig','featureConfig instanceof AbstractOreFeature oreFeatureConfig'),
            ('oreFeatureConfig.discardChanceOnAirExposure','oreFeatureConfig.discardChanceOnAirExposure()'),
            ('oreFeatureConfig.size;','oreFeatureConfig.size();'),
            ('feature.feature().value().feature() instanceof ScatteredOreFeature','feature.feature().value() instanceof ScatteredOreFeature')]
    }
    no_depth=Path(__file__).with_name('compat263_NoDepthLayer.java').read_text(encoding='utf8')
    for file in (java_root/'src/main/java').rglob('*.java'):
        relative=file.relative_to(java_root/'src/main/java');text=file.read_text(encoding='utf8');name=relative.as_posix()
        for old,new in transformations.get(name,[]):
            if old not in text:raise RuntimeError('Stale 26.3 adaptation: '+name+' / '+old)
            text=text.replace(old,new)
        if name=='kaptainwutax/seedcrackerX/render/NoDepthLayer.java':text=no_depth
        if name=='kaptainwutax/seedcrackerX/finder/FinderQueue.java':
            text=text.replace('private static final RenderStateDataKey<Set<Cuboid>> CUBOID_SET_KEY = RenderStateDataKey.create(() -> "SeedCrackerX cuboid set");','private static final java.util.Map<LevelRenderState, Set<Cuboid>> cuboidSets = java.util.Collections.synchronizedMap(new java.util.WeakHashMap<>());')
            text=text.replace('state.setData(CUBOID_SET_KEY, Collections.emptySet())','cuboidSets.put(state, Collections.emptySet())').replace('state.setData(CUBOID_SET_KEY, cuboids)','cuboidSets.put(state, cuboids)').replace('state.getData(CUBOID_SET_KEY)','cuboidSets.get(state)')
        target=sources/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_text(text,encoding='utf8')
    args=folder/'javac.args';files=sorted(sources.rglob('*.java'))
    args.write_text('--release 25\n-encoding UTF-8\n-d "'+classes.as_posix()+'"\n-classpath "'+';'.join(p.as_posix() for p in cp)+'"\n'+'\n'.join('"'+p.as_posix()+'"' for p in files),encoding='utf8')
    result=subprocess.run(['javac','@'+str(args)],capture_output=True,text=True,encoding='utf-8',errors='replace')
    (folder/'compile.log').write_text(result.stdout+result.stderr,encoding='utf8')
    if result.returncode:raise RuntimeError('26.3 compile failed; see '+str(folder/'compile.log'))
    rebuilt=folder/'bridge.jar'
    with zipfile.ZipFile(bridge_jar) as old,zipfile.ZipFile(rebuilt,'w',zipfile.ZIP_DEFLATED) as new:
        for entry in old.infolist():
            if not entry.filename.startswith('compat/26.3/'):new.writestr(entry,old.read(entry.filename))
        for file in sorted(classes.rglob('*.class')):new.write(file,'compat/26.3/'+file.relative_to(classes).as_posix())
    bridge_jar.write_bytes(rebuilt.read_bytes())
    return {'minecraft':'26.3','game':str(game),'classpath':[str(p) for p in cp]}
