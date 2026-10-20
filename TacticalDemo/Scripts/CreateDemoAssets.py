"""Run once in UE editor; generate native .uasset/.umap files, never overwrite existing assets."""
import unreal

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
materials = unreal.MaterialEditingLibrary


def make_material(name, color, parameter):
    path = "/Game/Materials/" + name
    if assets.does_asset_exist(path):
        mat = unreal.load_asset(path)
        if materials.get_material_property_input_node(mat, unreal.MaterialProperty.MP_BASE_COLOR):
            return mat
        # Repair only the incomplete generated graph from the initial scaffold.
        materials.delete_all_material_expressions(mat)
    else:
        mat = asset_tools.create_asset(name, "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    expr = materials.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -300, 0)
    expr.set_editor_property("parameter_name", parameter)
    expr.set_editor_property("default_value", unreal.LinearColor(*color))
    assert materials.connect_material_property(expr, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Gentle ambient color keeps red/blue readable even on the shaded side.
    glow = materials.create_material_expression(mat, unreal.MaterialExpressionMultiply, -100, 250)
    glow.set_editor_property("const_b", 0.2 if name == "M_Unit" else 0.08)
    assert materials.connect_material_expressions(expr, "", glow, "A")
    assert materials.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = materials.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 150)
    rough.set_editor_property("r", 0.65)
    assert materials.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    materials.recompile_material(mat)
    assets.save_loaded_asset(mat)
    return mat


make_material("M_Unit", (1.0, 0.04, 0.025, 1.0), "SideColor")
floor_mat = make_material("M_Ground", (0.12, 0.15, 0.20, 1.0), "GroundColor")

if not assets.does_asset_exist("/Game/Maps/Demo"):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    ground = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, -50))
    ground.set_actor_label("Demo Ground")
    ground.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))
    ground.set_actor_scale3d(unreal.Vector(200, 200, 1))
    ground.static_mesh_component.set_material(0, floor_mat)
    sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(-55, -30, 0))
    sun.light_component.set_editor_property("intensity", 5.0)
    sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
    sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    sky.light_component.set_editor_property("intensity", 1.0)
    actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/Demo"):
        raise RuntimeError("Failed to save /Game/Maps/Demo")

unreal.log("TACTICAL_ASSETS_READY")
