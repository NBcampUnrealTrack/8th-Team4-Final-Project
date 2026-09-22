"""Prepare private color materials and reconnect the two studio COs (editor closed).

Back up the two COs/configs first. Does not save original MetaHuman assets or maps.
"""
import unreal

classes = {'Trooper': ['MHC_Frey', 'MHC_Gavin', 'Mason'],
           'Vanguard': ['MHC_Crowd80', 'Dean', 'MHC_Shrank']}
for names in classes.values():
    for name in names:
        for side in ('L', 'R'):
            source = f'/Game/MetaHumans/{name}/Face/Materials/MI_Eye{side}_Baked'
            dest = f'/Game/FPS/Customizable/Character/Materials/ClassEyes/MI_{name}_Eye{side}'
            original = unreal.load_asset(source)
            assert original, source
            mat = unreal.load_asset(dest) if unreal.EditorAssetLibrary.does_asset_exist(dest) else unreal.EditorAssetLibrary.duplicate_asset(source, dest)
            assert mat and mat.get_editor_property('parent') == original.get_editor_property('parent'), dest
            # UE 5.7's setter returns false even on success; verify the readback.
            unreal.MaterialEditingLibrary.set_material_instance_static_switch_parameter_value(mat, 'Use Baked Material', False)
            assert not unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(mat, 'Use Baked Material')
            unreal.MaterialEditingLibrary.update_material_instance(mat)
            assert unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
# Rebuild every body branch, including legacy graphs that shared only the first
# body's material nodes. Build invokes ClassColors.Apply before compiling/saving.
unreal.SystemLibrary.execute_console_command(world, 'FPS.ClassVariants.Build')
unreal.SystemLibrary.execute_console_command(world, 'FPS.ClassVariants.Verify')
assert unreal.SystemLibrary.get_console_variable_int_value('FPS.ClassVariants.Failures') == 0
for cls in classes:
    cfg = unreal.load_asset('/Game/FPS/Customizable/Character/Preview/DA_Studio' + cls)
    cfg.set_editor_property('enabled_clothing_parts', ['Top', 'Bottom', 'Vest' if cls == 'Trooper' else 'Attachment', 'Boots'])
    assert unreal.EditorAssetLibrary.save_loaded_asset(cfg, only_if_is_dirty=False)
unreal.log('CLASS_COLORS_APPLIED')
