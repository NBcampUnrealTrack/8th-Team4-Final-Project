"""Create only the 12 requested cross-face scalp-hair bindings."""
import unreal

classes = {
    'Trooper': [('MHC_Frey', 'Middle_part'), ('MHC_Gavin', 'GA_Gavin_Hair'), ('Mason', 'Hair_S_CurlyFade')],
    'Vanguard': [('MHC_Crowd80', 'Hair_S_Casual'), ('Dean', 'Hair_S_CoilBuzzCut'), ('MHC_Shrank', 'Hair_S_BuzzCut')],
}
root = '/Game/FPS/Customizable/Character/HairBindings'
jobs = []
for cls, entries in classes.items():
    for owner, hair in entries:
        groom = unreal.load_asset(f'/Game/MetaHumans/{owner}/Grooms/{hair}')
        source = unreal.load_asset(f'/Game/MetaHumans/{owner}/Face/SKM_{owner}_FaceMesh')
        assert isinstance(groom, unreal.GroomAsset), owner
        assert isinstance(source, unreal.SkeletalMesh), owner
        for target, _ in entries:
            if target == owner:
                continue
            mesh = unreal.load_asset(f'/Game/MetaHumans/{target}/Face/SKM_{target}_FaceMesh')
            assert isinstance(mesh, unreal.SkeletalMesh), target
            path = f'{root}/{cls}/GB_{owner}_To_{target}'
            assert not unreal.EditorAssetLibrary.does_asset_exist(path), 'Refusing to overwrite: '+path
            jobs.append((path, groom, source, mesh))
assert len(jobs) == 12
for path, groom, source, mesh in jobs:
    # Assembled groom geometry is already fitted to its owner's face.
    # Use that face as the transfer source, not the destination's rest pose.
    binding = unreal.GroomLibrary.create_new_groom_binding_asset_with_path(path, groom, mesh, 100, source, 0)
    assert binding, 'Binding build failed: '+path
    unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'Editor.AsyncAssetCompilationFinishAll')
    assert unreal.EditorAssetLibrary.save_loaded_asset(binding, False), path
    unreal.log('CLASS_HAIR_CREATED '+binding.get_path_name())
unreal.log('CLASS_HAIR_CREATED_12')
