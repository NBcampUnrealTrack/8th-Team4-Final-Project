"""Complete MutableLevel's private hair config; preserve shared preview assets."""
import unreal

ROOT='/Game/FPS/Customizable/MutableLevel'
DEST=ROOT+'/DA_MutableLevelStudio'
SOURCE='/Game/FPS/Customizable/Character/Preview/DA_CustomizationStudio'
expected={'Custom':'SKM_MHC_Custom01_FaceMesh','Fab':'SKM_Skotukeda_FaceMesh'}
bindings=[('Custom','GB_Ponytail_Custom'),('Fab','GB_PonytailBinding_Mara')]
validated=[]
for body,name in bindings:
    binding=unreal.load_asset('/Game/Fab/MetaHuman/Hair_M_SlickPonytail/'+name)
    assert isinstance(binding,unreal.GroomBindingAsset),name
    groom=binding.get_editor_property('groom')
    target=binding.get_editor_property('target_skeletal_mesh')
    assert groom and groom.get_name()=='Hair_M_SlickPonytail',name
    assert target and target.get_name()==expected[body],name
    validated.append((body,name,binding,groom))
config=unreal.load_asset(DEST) if unreal.EditorAssetLibrary.does_asset_exist(DEST) else unreal.EditorAssetLibrary.duplicate_asset(SOURCE,DEST)
assert config
choices=list(config.get_editor_property('grooms'))
for body,name,binding,groom in validated:
    if any(c.get_editor_property('body_type')==body and str(c.get_editor_property('id'))==name for c in choices):continue
    c=unreal.StudioGroomChoice()
    for prop,value in [('body_type',body),('id',name),('label','Ponytail'),('component','Hair'),('groom',groom),('binding',binding)]:c.set_editor_property(prop,value)
    choices.append(c)
for body in ['Seo','Custom','Fab']:
    hairs=[c for c in choices if c.get_editor_property('body_type')==body and str(c.get_editor_property('component'))=='Hair']
    assert len(hairs)==3,body
    assert {c.get_editor_property('groom').get_name() for c in hairs}=={'Hair_S_UpdoBuns','Hair_M_BobStraight','Hair_M_SlickPonytail'},body
config.set_editor_property('grooms',choices)
assert unreal.EditorAssetLibrary.save_loaded_asset(config,False)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Maps/MutableLevel')
previews=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.CustomizationStudioActor)]
assert len(previews)==1
previews[0].set_editor_property('config',config)
assert levels.save_current_level()
unreal.log('MUTABLE_HAIR_CONFIG_COMPLETE 3 bodies x 3 hairstyles; shared config unchanged')
