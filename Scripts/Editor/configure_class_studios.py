"""Configure approved class-local previews; never save assembled source BPs."""
import sys
from pathlib import Path
import unreal
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
from verify_class_studios import CLASSES, assembled_grooms, verify

configs = {}
for cls, names in CLASSES.items():
    path = '/Game/FPS/Customizable/Character/Preview'
    name = 'DA_Studio' + cls
    cfg = unreal.load_asset(path + '/' + name) if unreal.EditorAssetLibrary.does_asset_exist(path + '/' + name) else None
    if not cfg:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property('data_asset_class', unreal.CustomizationStudioConfig)
        cfg = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, path, unreal.CustomizationStudioConfig, factory)
    cfg.set_editor_property('class_id', cls)
    cfg.set_editor_property('body_types', names)
    cfg.set_editor_property('default_body_type', names[0])
    cfg.set_editor_property('default_hair_id', names[0])
    cfg.set_editor_property('customizable_object', unreal.load_asset('/Game/FPS/Customizable/CO_' + cls))
    cfg.set_editor_property('enabled_clothing_parts', ['Top', 'Bottom', 'Vest' if cls == 'Trooper' else 'Attachment', 'Boots'])
    originals = {n: assembled_grooms(n) for n in names}
    choices = []
    for target in names:
        for owner in names:
            groom, binding = originals[owner]['Hair']
            if target != owner:
                binding = unreal.load_asset(f'/Game/FPS/Customizable/Character/HairBindings/{cls}/GB_{owner}_To_{target}')
            assert binding
            choices.append(unreal.StudioGroomChoice(body_type=target, id=owner, label=owner, groom=groom, binding=binding, component='Hair'))
        for component, (groom, binding) in originals[target].items():
            if component != 'Hair':
                assert component in ['Eyebrows', 'Eyelashes', 'Fuzz', 'Mustache', 'Beard'], component
                choices.append(unreal.StudioGroomChoice(body_type=target, id='Fixed', label=component, groom=groom, binding=binding, component=component))
    cfg.set_editor_property('grooms', choices)
    configs[cls] = cfg
verify()
for cfg in configs.values():
    assert unreal.EditorAssetLibrary.save_loaded_asset(cfg, only_if_is_dirty=False)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/MutableLevel')
actors = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a, unreal.CustomizationStudioActor)]
assert len(actors) == 1
actor = actors[0]
configs['Ranger'] = actor.get_editor_property('config')
assert configs['Ranger'] and configs['Ranger'].class_id == 'Ranger'
actor.set_editor_property('class_configs', configs)
assert actor.get_editor_property('use_reference_pose_grooms')
assert level.save_current_level()
unreal.log('CLASS_STUDIOS_AND_LEVEL_SAVED')
