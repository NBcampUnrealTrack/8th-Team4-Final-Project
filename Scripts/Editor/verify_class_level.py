"""Fresh-process validation of saved class configuration and level wiring."""
import sys
from pathlib import Path
import unreal
sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).parent))
from verify_class_studios import verify
verify()
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level('/Game/Maps/MutableLevel')
actors = [a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a, unreal.CustomizationStudioActor)]
assert len(actors) == 1
a = actors[0]
configs = {str(k): v for k, v in a.class_configs.items()}
assert set(configs) == {'Ranger', 'Trooper', 'Vanguard'}
assert a.config == configs['Ranger']
assert a.get_editor_property('use_reference_pose_grooms')
for name, cfg in configs.items():
    assert name == cfg.class_id and len(cfg.body_types) == 3
unreal.log('CLASS_LEVEL_VERIFIED')
