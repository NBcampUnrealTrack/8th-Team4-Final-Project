"""
mine_mid 60개 폴더 일괄 임포트 + 머티리얼 자동 생성/연결 스크립트.

언리얼의 FBX 자동 머티리얼 생성(임포트 시 텍스처 자동 검색/연결)은 FBX 안에
실제로 이미지가 연결된 머티리얼이 내장되어 있어야 동작한다. Quixel raw
FBX는 그게 없어서 자동 연결이 실패한다(World Grid Material로 표시됨).

이 스크립트는 그 자동 기능을 아예 쓰지 않는다: FBX는 메시만 가져오고
(import_materials=False), 텍스처는 폴더명 기준으로 직접 찾아서 별도로
머티리얼 인스턴스를 만들어 강제로 연결한다. 따라서 FBX 내부 이름이나
임베디드 머티리얼 상태와 무관하게 항상 동작한다 (원본 mine_mid 그대로
써도 됨, Blender로 미리 손볼 필요 없음).

실행 방법:
  1. Edit > Plugins 에서 "Python Editor Script Plugin" 활성화 (재시작 필요할 수 있음)
  2. Window > Developer Tools > Output Log 열기
  3. 하단 입력창에 다음 입력 후 Enter:
     py "C:/Unreal/RogueFPS/Scripts/import_mine_mid.py"

결과는 Output Log에 [OK]/[SKIP]/[FAIL] 태그로 출력된다.
"""

import os
import unreal

SOURCE_ROOT = r"C:\Users\j1b2c3\OneDrive\바탕 화면\mine_mid"
DEST_ROOT = "/Game/Fab/Ind_Min_Mid"
MASTER_MATERIAL_PATH = f"{DEST_ROOT}/M_Master_Prop_Mid"

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

TEX_KEYWORDS = {
    "BaseColor": ["basecolor", "albedo", "diffuse"],
    "Normal": ["normal"],
    "Roughness": ["roughness"],
    "AO": ["_ao.", "_ao_"],
    "Metalness": ["metalness", "metallic"],
}


def log_ok(name):
    unreal.log(f"[OK] {name}")


def log_skip(name, reason):
    unreal.log_warning(f"[SKIP] {name}: {reason}")


def log_fail(name, reason):
    unreal.log_error(f"[FAIL] {name}: {reason}")


def get_or_create_master_material():
    existing = unreal.EditorAssetLibrary.load_asset(MASTER_MATERIAL_PATH)
    if existing:
        return existing

    factory = unreal.MaterialFactoryNew()
    mat = asset_tools.create_asset("M_Master_Prop_Mid", DEST_ROOT, unreal.Material, factory)

    def make_tex_param(name, x, y, sampler_type):
        node = unreal.MaterialEditingLibrary.create_material_expression(
            mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y
        )
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler_type)
        return node

    base_color = make_tex_param("BaseColor", -450, -300, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    normal = make_tex_param("Normal", -450, 0, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    roughness = make_tex_param("Roughness", -450, 300, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    ao = make_tex_param("AO", -450, 600, unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)

    metallic_scalar = unreal.MaterialEditingLibrary.create_material_expression(
        mat, unreal.MaterialExpressionScalarParameter, -450, 900
    )
    metallic_scalar.set_editor_property("parameter_name", "Metallic")
    metallic_scalar.set_editor_property("default_value", 0.0)

    unreal.MaterialEditingLibrary.connect_material_property(base_color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    unreal.MaterialEditingLibrary.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    unreal.MaterialEditingLibrary.connect_material_property(
        metallic_scalar, "", unreal.MaterialProperty.MP_METALLIC
    )

    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def find_textures(folder_path):
    found = {}
    for f in os.listdir(folder_path):
        lower = f.lower()
        if not lower.endswith((".jpg", ".jpeg", ".png", ".tga")):
            continue
        for param, keywords in TEX_KEYWORDS.items():
            if param in found:
                continue
            if any(k in lower for k in keywords):
                found[param] = os.path.join(folder_path, f)
                break
    return found


def import_single_asset(filename, dest_path):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest_path
    task.automated = True
    task.save = True
    task.replace_existing = True
    asset_tools.import_asset_tasks([task])
    return list(task.imported_object_paths)


def import_fbx(fbx_path, dest_path):
    task = unreal.AssetImportTask()
    task.filename = fbx_path
    task.destination_path = dest_path
    task.automated = True
    task.save = True
    task.replace_existing = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    if options.static_mesh_import_data:
        options.static_mesh_import_data.set_editor_property(
            "normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
        )
    task.options = options

    asset_tools.import_asset_tasks([task])
    return list(task.imported_object_paths)


def build_material_instance(folder_name, dest_path, textures, master):
    mi_factory = unreal.MaterialInstanceConstantFactoryNew()
    mi_name = f"MI_{folder_name}"

    existing = unreal.EditorAssetLibrary.load_asset(f"{dest_path}/{mi_name}")
    if existing:
        mi = existing
    else:
        mi = asset_tools.create_asset(mi_name, dest_path, unreal.MaterialInstanceConstant, mi_factory)

    # parent는 팩토리가 아니라 생성된 인스턴스에 직접 지정 (버전 간 가장 안전한 방식)
    unreal.MaterialEditingLibrary.set_material_instance_parent(mi, master)

    for param, tex_file in textures.items():
        if param == "Metalness":
            continue
        imported = import_single_asset(tex_file, dest_path)
        if not imported:
            continue
        tex_asset = unreal.load_asset(imported[0])
        unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(mi, param, tex_asset)

    if "Metalness" in textures:
        unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(mi, "Metallic", 1.0)

    unreal.EditorAssetLibrary.save_loaded_asset(mi)
    return mi


def process_folder(folder_name, master):
    src_folder = os.path.join(SOURCE_ROOT, folder_name)
    dest_path = f"{DEST_ROOT}/{folder_name}"

    fbx_files = sorted(f for f in os.listdir(src_folder) if f.lower().endswith(".fbx"))
    textures = find_textures(src_folder)

    if not textures.get("BaseColor"):
        log_skip(folder_name, "BaseColor 텍스처 없음 - 수동 확인 필요")
        return

    mi = build_material_instance(folder_name, dest_path, textures, master)

    if not fbx_files:
        log_ok(f"{folder_name} (머티리얼만, 메시 없음)")
        return

    imported_meshes = []
    for fbx_file in fbx_files:
        fbx_path = os.path.join(src_folder, fbx_file)
        imported = import_fbx(fbx_path, dest_path)
        if not imported:
            log_fail(folder_name, f"{fbx_file} 임포트 실패")
            continue
        imported_meshes.extend(imported)

    if not imported_meshes:
        log_fail(folder_name, "메시 임포트 전부 실패")
        return

    for mesh_path in imported_meshes:
        mesh = unreal.load_asset(mesh_path)
        if not isinstance(mesh, unreal.StaticMesh):
            continue
        mesh.set_material(0, mi)
        unreal.EditorAssetLibrary.save_loaded_asset(mesh)

    log_ok(f"{folder_name} (메시 {len(imported_meshes)}개 + 머티리얼)")


def main():
    if not os.path.isdir(SOURCE_ROOT):
        unreal.log_error(f"소스 폴더를 찾을 수 없음: {SOURCE_ROOT}")
        return

    if not unreal.EditorAssetLibrary.does_directory_exist(DEST_ROOT):
        unreal.EditorAssetLibrary.make_directory(DEST_ROOT)

    master = get_or_create_master_material()

    folders = sorted(
        f for f in os.listdir(SOURCE_ROOT) if os.path.isdir(os.path.join(SOURCE_ROOT, f))
    )
    unreal.log(f"=== 총 {len(folders)}개 폴더 처리 시작 ===")

    for folder_name in folders:
        try:
            process_folder(folder_name, master)
        except Exception as e:
            log_fail(folder_name, str(e))

    unreal.log("=== 전체 작업 완료 ===")


main()
