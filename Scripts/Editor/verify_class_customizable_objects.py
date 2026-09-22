"""Read-only regression: both class COs must exist independently of Ranger."""
import unreal

for name in ('Trooper','Vanguard'):
    path='/Game/FPS/Customizable/CO_'+name
    assert unreal.EditorAssetLibrary.does_asset_exist(path), 'Missing class CO: '+path
    co=unreal.load_asset(path)
    assert isinstance(co,unreal.CustomizableObject),path
    # Source is a protected editor property; graph and generated-mesh checks
    # are performed by FPS.ClassCO.Test in the editor module.
unreal.log('CLASS_CO_STRUCTURE_VERIFIED')
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world, 'FPS.ClassVariants.Verify')
assert unreal.SystemLibrary.get_console_variable_int_value('FPS.ClassVariants.Failures') == 0, 'Native CO verification failed or did not complete'
unreal.log('CLASS_CO_ALL_VERIFIED')
