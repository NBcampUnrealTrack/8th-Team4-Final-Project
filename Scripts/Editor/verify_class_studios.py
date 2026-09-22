"""Read-only class config and assembled grooming contract."""
import unreal

CLASSES = {'Trooper': ['MHC_Frey', 'MHC_Gavin', 'Mason'], 'Vanguard': ['MHC_Crowd80', 'Dean', 'MHC_Shrank']}

def assembled_grooms(name):
    bp = unreal.load_asset(f'/Game/MetaHumans/{name}/BP_{name}')
    assert bp, name
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    result = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if isinstance(obj, unreal.GroomComponent):
            component = obj.get_name().removesuffix('_GEN_VARIABLE')
            groom, binding = obj.get_editor_property('groom_asset'), obj.get_editor_property('binding_asset')
            if groom and binding:
                assert component not in result or result[component] == (groom, binding), component
                result[component] = (groom, binding)
    assert 'Hair' in result, name
    return result

def verify():
    for cls, names in CLASSES.items():
        cfg = unreal.load_asset(f'/Game/FPS/Customizable/Character/Preview/DA_Studio{cls}')
        assert cfg, cls
        assert cfg.class_id == cls and list(cfg.body_types) == names
        assert cfg.default_body_type == names[0] and str(cfg.default_hair_id) == names[0]
        assert cfg.customizable_object.get_name() == 'CO_' + cls
        assert set(map(str, cfg.enabled_clothing_parts)) == {'Top', 'Bottom', 'Vest' if cls == 'Trooper' else 'Attachment', 'Boots'}
        hair = [h for h in cfg.grooms if str(h.component) == 'Hair']
        assert len(hair) == 9
        for target in names:
            fixed = assembled_grooms(target)
            choices = [h for h in hair if h.body_type == target]
            assert [str(h.id) for h in choices] == names
            for h in choices:
                assert h.groom == assembled_grooms(str(h.id))['Hair'][0]
                assert h.binding.get_editor_property('target_skeletal_mesh').get_name() == f'SKM_{target}_FaceMesh'
            actual = {str(h.component): (h.groom, h.binding) for h in cfg.grooms if h.body_type == target and str(h.component) != 'Hair'}
            assert actual == {k: v for k, v in fixed.items() if k != 'Hair'}, target
        unreal.log('CLASS_STUDIO_VERIFIED ' + cls)

if __name__ == '__main__':
    verify()
