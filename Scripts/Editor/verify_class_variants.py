import unreal
w=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(w,'FPS.ClassVariants.Verify')
assert unreal.SystemLibrary.get_console_variable_int_value('FPS.ClassVariants.Failures')==0
