"""Read-only UI and private eye-material contract for the six new variants."""
import unreal
for cls,names,parts in [
    ('Trooper',['MHC_Frey','MHC_Gavin','Mason'],['Top','Bottom','Vest','Boots']),
    ('Vanguard',['MHC_Crowd80','Dean','MHC_Shrank'],['Top','Bottom','Attachment','Boots'])]:
    cfg=unreal.load_asset('/Game/FPS/Customizable/Character/Preview/DA_Studio'+cls)
    assert set(map(str,cfg.enabled_clothing_parts))==set(parts),cls+' missing clothing controls'
    for name in names:
        for side in ['L','R']:
            mat=unreal.load_asset(f'/Game/FPS/Customizable/Character/Materials/ClassEyes/MI_{name}_Eye{side}')
            assert mat,name+side
            assert not unreal.MaterialEditingLibrary.get_material_instance_static_switch_parameter_value(mat,'Use Baked Material'),name+side
unreal.log('CLASS_COLOR_CONTROLS_VERIFIED')
