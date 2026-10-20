import unreal

# Duplicate original materials; only the aircraft body diffuse input is parameterized.
definitions = [
    ("/Game/plane/J20/materials/03_-_Default", "/Game/plane/J20/materials/M_J20_TextureSwitch", "/Game/plane/J20/texture/mat_J20_Airframe_Diffuse"),
    ("/Game/plane/J10/materials/02_-_Default", "/Game/plane/J10/materials/M_J10_TextureSwitch", "/Game/plane/J10/texture/j10b_d"),
]
for source, destination, texture_path in definitions:
    material = unreal.load_asset(destination)
    if not material:
        material = unreal.EditorAssetLibrary.duplicate_asset(source, destination)
    if not isinstance(material, unreal.Material):
        raise RuntimeError("Cannot create switchable body material " + destination)
    node = unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)
    if not isinstance(node, unreal.MaterialExpressionTextureSampleParameter2D):
        if not isinstance(node, unreal.MaterialExpressionTextureSample):
            raise RuntimeError("Unexpected original body material graph: " + destination)
        node = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -350, 0)
        node.set_editor_property("parameter_name", "AircraftDiffuse")
        if not unreal.MaterialEditingLibrary.connect_material_property(node, "RGB", unreal.MaterialProperty.MP_BASE_COLOR):
            raise RuntimeError("Could not connect body texture parameter")
    node.set_editor_property("texture", unreal.load_asset(texture_path))
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save " + destination)
    unreal.log("TEXTURE_SWITCH_MATERIAL_READY " + destination)
