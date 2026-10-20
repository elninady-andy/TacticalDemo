"""Create only the new fixed-gold aircraft overlay; preserve existing assets."""
import unreal

path = "/Game/Materials/M_AircraftHighlight"
assets = unreal.EditorAssetLibrary
edit = unreal.MaterialEditingLibrary
if assets.does_asset_exist(path):
    material = unreal.load_asset(path)
    if not edit.get_material_property_input_node(material, unreal.MaterialProperty.MP_EMISSIVE_COLOR):
        raise RuntimeError("Existing highlight asset is incomplete; inspect before changing it")
else:
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "M_AircraftHighlight", "/Game/Materials", unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    rim = edit.create_material_expression(material, unreal.MaterialExpressionFresnel, -600, 0)
    rim.set_editor_property("exponent", 2.0)
    rim.set_editor_property("base_reflect_fraction", 0.2)
    gold = edit.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -600, 160)
    gold.set_editor_property("constant", unreal.LinearColor(12.0, 4.2, 0.3, 1.0))
    glow = edit.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 0)
    assert edit.connect_material_expressions(rim, "", glow, "A")
    assert edit.connect_material_expressions(gold, "", glow, "B")
    assert edit.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    opacity = edit.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 180)
    opacity.set_editor_property("r", 1.0)
    assert edit.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    edit.recompile_material(material)
    if not assets.save_loaded_asset(material):
        raise RuntimeError("Could not save aircraft highlight material")
unreal.log("AIRCRAFT_HIGHLIGHT_MATERIAL_READY")
