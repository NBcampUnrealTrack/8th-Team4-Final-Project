"""Create the opt-in MutableLevel. Never overwrite existing assets."""
import unreal

ROOT='/Game/FPS/Customizable/MutableLevel'
MAP='/Game/Maps/MutableLevel'
paths=[MAP,ROOT+'/UI/WBP_MutableLevel',ROOT+'/BP_MutableLevelController',ROOT+'/BP_MutableLevelGameMode',ROOT+'/M_MutableStage']
for path in paths:
    assert not unreal.EditorAssetLibrary.does_asset_exist(path), 'Refusing overwrite: '+path
assert not unreal.EditorAssetLibrary.does_asset_exist(ROOT+'/DA_MutableLevelStudio'), 'Refusing overwrite: private studio config'
tools=unreal.AssetToolsHelpers.get_asset_tools()

def asset(path,kind,factory):
    folder,name=path.rsplit('/',1)
    value=tools.create_asset(name,folder,kind,factory)
    assert value,path
    return value

def blueprint(path,parent):
    f=unreal.BlueprintFactory();f.set_editor_property('parent_class',parent)
    return asset(path,unreal.Blueprint,f)

wf=unreal.WidgetBlueprintFactory();wf.set_editor_property('parent_class',unreal.CustomizationStudioWidget)
widget=asset(paths[1],unreal.WidgetBlueprint,wf)
defaults=unreal.get_default_object(widget.generated_class())
defaults.set_editor_property('use_mutable_level_layout',True)
defaults.set_editor_property('is_focusable',True)
defaults.set_editor_property('body_font',unreal.load_asset('/Game/FPS/UI/Core/Fonts/WantedSans/WantedSans'))
defaults.set_editor_property('heading_font',unreal.load_asset('/Game/FPS/UI/Core/Fonts/ChakraPetch/ChakraPetch'))
# Reuse the actual game session button artwork without modifying its source style.
source=unreal.load_asset('/Game/FPS/UI/OutGame/Session/Buttons/Secondary/ButtonStyle_Session_Secondary')
style_cdo=unreal.get_default_object(source.generated_class())
style=unreal.ButtonStyle()
for dest,src in [('normal','normal_base'),('hovered','normal_hovered'),('pressed','normal_pressed'),('disabled','disabled')]:
    brush=style_cdo.get_editor_property(src)
    size=brush.get_editor_property('image_size')
    size.set_editor_property('x',160);size.set_editor_property('y',36)
    brush.set_editor_property('image_size',size)
    style.set_editor_property(dest,brush)
defaults.set_editor_property('action_style',style)
pc=blueprint(paths[2],unreal.CustomizationStudioController)
unreal.get_default_object(pc.generated_class()).set_editor_property('widget_class',widget.generated_class())
gm=blueprint(paths[3],unreal.CustomizationStudioGameMode)
unreal.get_default_object(gm.generated_class()).set_editor_property('player_controller_class',pc.generated_class())
for bp in (widget,pc,gm):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert unreal.EditorAssetLibrary.save_loaded_asset(bp,False)

levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert levels.new_level(MAP)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode',gm.generated_class())
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def spawn(cls,label,pos=(0,0,0),rot=(0,0,0)):
    a=actors.spawn_actor_from_class(cls,unreal.Vector(*pos),unreal.Rotator(pitch=rot[0],yaw=rot[1],roll=rot[2]))
    assert a,label
    a.set_actor_label(label)
    return a

preview=unreal.load_asset('/Game/FPS/Customizable/Character/Preview/BP_CustomizationStudio')
preview_actor=spawn(preview.generated_class(),'Mutable / Ranger Preview')
preview_actor.set_editor_property('use_mutable_level_framing',True)
preview_actor.set_editor_property('use_reference_pose_grooms',True)
preview_actor.set_editor_property('preview_face_lod',1)
preview_actor.set_face_view(False)
preview_actor.rotate_preview(180)
for groom in preview_actor.get_components_by_class(unreal.GroomComponent):
    groom.set_editor_property('use_cards',True)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
dark=asset(paths[4],unreal.Material,unreal.MaterialFactoryNew())
color=unreal.MaterialEditingLibrary.create_material_expression(dark,unreal.MaterialExpressionConstant3Vector)
color.set_editor_property('constant',unreal.LinearColor(.018,.025,.036,1))
unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
roughness=unreal.MaterialEditingLibrary.create_material_expression(dark,unreal.MaterialExpressionConstant)
roughness.set_editor_property('r',.8)
unreal.MaterialEditingLibrary.connect_material_property(roughness,'',unreal.MaterialProperty.MP_ROUGHNESS)
unreal.MaterialEditingLibrary.recompile_material(dark)
assert unreal.EditorAssetLibrary.save_loaded_asset(dark,False)
def block(label,pos,scale):
    a=spawn(unreal.StaticMeshActor,label,pos)
    a.static_mesh_component.set_static_mesh(cube)
    a.static_mesh_component.set_material(0,dark)
    a.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    a.set_actor_scale3d(unreal.Vector(*scale))
    return a

block('Stage / Floor',(0,0,-12),(18,18,.2))
block('Stage / Back Wall',(0,280,190),(18,.3,4))
for x in [-500,-340,-180,180,340,500]:
    block('Stage / Equipment Panel '+str(x),(x,250,140),(1.4,.25,2.8))
block('Stage / Pedestal',(0,0,-3),(2.5,2.5,.08))

for label,pos,rot,power,color in [
    ('Key',(-180,-220,270),(-30,55,0),3500,(1,.94,.86)),
    ('Fill',(190,-160,210),(-20,135,0),1800,(.7,.85,1)),
    ('Rim',(0,160,230),(-20,-90,0),2400,(.2,.55,1))]:
    a=spawn(unreal.RectLight,'Lighting / '+label,pos,rot)
    c=a.get_component_by_class(unreal.RectLightComponent)
    c.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
    c.set_editor_property('intensity_units',unreal.LightUnits.CANDELAS)
    c.set_intensity(power);c.set_light_color(unreal.LinearColor(*color,1))
    c.set_editor_property('source_width',160);c.set_editor_property('source_height',180)
sky=spawn(unreal.SkyLight,'Lighting / Ambient',(0,0,250))
sky.light_component.set_editor_property('mobility',unreal.ComponentMobility.MOVABLE)
sky.light_component.set_intensity(.35)
pp=spawn(unreal.PostProcessVolume,'Lighting / Exposure')
pp.set_editor_property('unbound',True)
settings=pp.get_editor_property('settings')
settings.set_editor_property('override_auto_exposure_method',True)
settings.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
settings.set_editor_property('override_auto_exposure_apply_physical_camera_exposure',True)
settings.set_editor_property('auto_exposure_apply_physical_camera_exposure',False)
settings.set_editor_property('override_auto_exposure_bias',True)
settings.set_editor_property('auto_exposure_bias',-6)
pp.set_editor_property('settings',settings)
assert levels.save_current_level()
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name('configure_mutable_hair.py')))
unreal.log('MUTABLE_LEVEL_CREATED')
