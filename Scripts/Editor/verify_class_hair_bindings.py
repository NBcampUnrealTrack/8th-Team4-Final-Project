"""Check cross-face hair bindings; no eyebrows, facial hair or Ranger changes."""
import unreal

CLASSES = {
    'Trooper': [('MHC_Frey', 'Middle_part'), ('MHC_Gavin', 'GA_Gavin_Hair'), ('Mason', 'Hair_S_CurlyFade')],
    'Vanguard': [('MHC_Crowd80', 'Hair_S_Casual'), ('Dean', 'Hair_S_CoilBuzzCut'), ('MHC_Shrank', 'Hair_S_BuzzCut')],
}
ROOT = '/Game/FPS/Customizable/Character/HairBindings'
failures = []
for cls, entries in CLASSES.items():
    for owner, hair in entries:
        for target, _ in entries:
            if target == owner:
                continue
            path = f'{ROOT}/{cls}/GB_{owner}_To_{target}'
            if not unreal.EditorAssetLibrary.does_asset_exist(path):
                failures.append('Missing binding: '+path)
                continue
            binding = unreal.load_asset(path)
            assert isinstance(binding, unreal.GroomBindingAsset), path
            assert binding.get_editor_property('groom').get_path_name() == f'/Game/MetaHumans/{owner}/Grooms/{hair}.{hair}', path
            for prop, person in [('source_skeletal_mesh', owner), ('target_skeletal_mesh', target)]:
                mesh = binding.get_editor_property(prop)
                assert mesh and mesh.get_path_name() == f'/Game/MetaHumans/{person}/Face/SKM_{person}_FaceMesh.SKM_{person}_FaceMesh', path
            unreal.SystemLibrary.execute_console_command(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), 'Editor.AsyncAssetCompilationFinishAll')
            groups = binding.get_editor_property('group_infos')
            assert len(groups) > 0, 'No built groups: '+path
            assert all(g.get_editor_property('sim_root_count') > 0 for g in groups), 'No bound guides: '+path
            unreal.log('CLASS_HAIR_VERIFIED '+path)
assert not failures, '\n'.join(failures)
unreal.log('CLASS_HAIR_ALL_12_VERIFIED')
