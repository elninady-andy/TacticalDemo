import unreal

for name in ("M_Unit", "M_Ground"):
    material = unreal.load_asset("/Game/Materials/" + name)
    assert material, "Missing material: " + name
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    assert isinstance(node, unreal.MaterialExpressionVectorParameter), name + ": base color not driven by color parameter"
assert unreal.EditorAssetLibrary.does_asset_exist("/Game/Maps/Demo"), "Missing Demo map"
unreal.log("TACTICAL_ASSETS_VERIFIED")
