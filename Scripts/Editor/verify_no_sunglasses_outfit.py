"""Regression: a dedicated glasses-free wardrobe item must exist."""
import unreal
path = '/Game/Fab/Mercenary/NoSunglasses/WI_OA_outfit_64_NoSunglasses'
assert unreal.EditorAssetLibrary.does_asset_exist(path), 'Missing independent glasses-free wardrobe item'
item = unreal.load_asset(path)
assert item.get_editor_property('principal_asset'), 'Missing outfit source'
assert item.get_editor_property('pipeline'), 'Missing outfit pipeline'
unreal.log('NO_SUNGLASSES_STRUCTURE_VERIFIED')
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
unreal.SystemLibrary.execute_console_command(world,'FPS.Wardrobe.VerifyNoSunglasses')
