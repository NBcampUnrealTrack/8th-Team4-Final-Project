"""Read-only verification; intentionally fails before MutableLevel is created."""
import unreal

ROOT='/Game/FPS/Customizable/MutableLevel'
assert unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/MutableLevel'), 'MutableLevel has not been created'
widget=unreal.load_asset(ROOT+'/UI/WBP_MutableLevel')
assert widget, 'Missing dedicated widget'
assert unreal.get_default_object(widget.generated_class()).get_editor_property('use_mutable_level_layout')
controller=unreal.load_asset(ROOT+'/BP_MutableLevelController')
mode=unreal.load_asset(ROOT+'/BP_MutableLevelGameMode')
assert unreal.get_default_object(controller.generated_class()).get_editor_property('widget_class')==widget.generated_class()
assert unreal.get_default_object(mode.generated_class()).get_editor_property('player_controller_class')==controller.generated_class()
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.load_level('/Game/Maps/MutableLevel')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
previews=[a for a in actors if isinstance(a,unreal.CustomizationStudioActor)]
assert len(previews)==1
config=previews[0].get_editor_property('config')
assert config.get_path_name()==ROOT+'/DA_MutableLevelStudio.DA_MutableLevelStudio'
for body in ['Seo','Custom','Fab']:
    hairs=[h for h in config.get_editor_property('grooms') if h.get_editor_property('body_type')==body and str(h.get_editor_property('component'))=='Hair']
    assert len(hairs)==3
    assert {h.get_editor_property('groom').get_name() for h in hairs}=={'Hair_S_UpdoBuns','Hair_M_BobStraight','Hair_M_SlickPonytail'}
assert previews[0].get_editor_property('use_mutable_level_framing')
assert previews[0].get_editor_property('use_reference_pose_grooms')
assert previews[0].get_editor_property('preview_face_lod')==1
previews[0].set_editor_property('use_mutable_level_framing',False)
previews[0].set_face_view(True)
assert abs(previews[0].camera.get_editor_property('relative_location').y+100)<.1, 'Legacy face framing must remain unchanged'
previews[0].set_editor_property('use_mutable_level_framing',True)
previews[0].set_face_view(False)
for component in previews[0].get_components_by_class(unreal.SceneComponent):
    if component.get_name()=='ModelRoot':
        rotation=component.get_editor_property('relative_rotation')
        assert abs(rotation.pitch)<.1 and abs(rotation.roll)<.1 and abs(abs(rotation.yaw)-180)<.1, 'Preview must face camera upright'
assert previews[0].get_editor_property('config').get_editor_property('customizable_object').get_path_name()=='/Game/FPS/Customizable/CO_Ranger.CO_Ranger'
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property('default_game_mode')==mode.generated_class()
for bp in (widget,controller,mode):unreal.BlueprintEditorLibrary.compile_blueprint(bp)
old=unreal.load_asset('/Game/FPS/Customizable/UI/WBP_CustomizationStudio')
assert not unreal.get_default_object(old.generated_class()).get_editor_property('use_mutable_level_layout')
unreal.log('MUTABLE_LEVEL_VERIFIED')
