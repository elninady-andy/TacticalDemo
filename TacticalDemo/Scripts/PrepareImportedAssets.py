import unreal
import os
import shutil

root = unreal.Paths.project_dir()
backup = os.path.join(root, "Backup", "ImportedAssets-2026-10-09")
for relative in ["Content/plane/J10/mesh/J10B.uasset", "Content/plane/J10/materials/02_-_Default.uasset", "Content/VRAirport/Maps/VRAirportMap.umap"]:
    source = os.path.join(root, relative)
    target = os.path.join(backup, relative)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    if not os.path.exists(target):
        shutil.copy2(source, target)
mesh = unreal.load_asset("/Game/plane/J10/mesh/J10B")
for index, name in enumerate(["02_-_Default", "03_-_Default"]):
    material = unreal.load_asset("/Game/plane/J10/materials/" + name)
    if not material:
        raise RuntimeError("Missing J10 material " + name)
    mesh.set_material(index, material)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
material = unreal.load_asset("/Game/plane/J10/materials/02_-_Default")
texture = unreal.load_asset("/Game/plane/J10/texture/j10b_d")
if not texture:
    raise RuntimeError("Missing original J10 diffuse texture")
pending = [unreal.MaterialEditingLibrary.get_material_property_input_node(material, unreal.MaterialProperty.MP_BASE_COLOR)]
visited = set()
fixed = 0
while pending:
    node = pending.pop()
    if not node or node.get_path_name() in visited:
        continue
    visited.add(node.get_path_name())
    if isinstance(node, unreal.MaterialExpressionTextureSample) and not node.get_editor_property("texture"):
        node.set_editor_property("texture", texture)
        fixed += 1
    pending.extend(unreal.MaterialEditingLibrary.get_inputs_for_material_expression(material, node))
if fixed:
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material):
        raise RuntimeError("Could not save repaired J10 material")
unreal.log("J10_TEXTURE_REFERENCES_REPAIRED " + str(fixed))
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not level.load_level("/Game/VRAirport/Maps/VRAirportMap"):
    raise RuntimeError("Airport map failed to load")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/TacticalDemo.DemoGameMode"))
if not level.save_current_level():
    raise RuntimeError("Airport game mode failed to save")
unreal.log("IMPORTED_ASSETS_PREPARED")
