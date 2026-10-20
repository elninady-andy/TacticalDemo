"""Import source FBX aircraft under /Game/plane and restore the supplied terrain material."""
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
out = project / "Saved/MoveImport"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
materials = {}
for data in registry.get_assets_by_path("/Game/Airfight", recursive=True):
    if str(data.asset_class_path.asset_name) in ("Material", "MaterialInstanceConstant"):
        name = str(data.asset_name)
        assert name not in materials, name
        materials[name] = str(data.package_name)
report = {"models": [], "terrain": None}
for model, name in (("J20_Map3", "SM_J20_Map3"), ("F35", "SM_F35")):
    folder = "/Game/plane/" + model + "/mesh"
    path = folder + "/" + name
    if not assets.does_asset_exist(path):
        options = unreal.FbxImportUI()
        options.automated_import_should_detect_type = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        static = options.static_mesh_import_data
        for key, value in {"combine_meshes": True, "auto_generate_collision": False, "generate_lightmap_u_vs": False,
                           "build_nanite": False, "transform_vertex_to_absolute": True}.items():
            static.set_editor_property(key, value)
        task = unreal.AssetImportTask()
        task.filename = str(out / (model + ".fbx"))
        task.destination_path = folder
        task.destination_name = name
        task.automated = True
        task.replace_existing = False
        task.save = True
        task.options = options
        tools.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    assert isinstance(mesh, unreal.StaticMesh), path
    bindings = []
    for index, slot in enumerate(mesh.static_materials):
        original = str(slot.get_editor_property("imported_material_slot_name"))
        if original not in materials:
            raise RuntimeError("Unknown source material: %s / %s" % (model, original))
        material = unreal.load_asset(materials[original])
        assert material, materials[original]
        mesh.set_material(index, material)
        bindings.append(material.get_path_name())
    bounds = mesh.get_bounds()
    # A reusable aircraft must be centred locally, with aircraft-scale dimensions.
    assert bounds.origin.length() < 2000 and 500 < bounds.box_extent.length() < 3000, str(bounds)
    assert assets.save_loaded_asset(mesh)
    report["models"].append({"id": model, "asset": mesh.get_path_name(), "materials": bindings,
                              "boundsOrigin": [bounds.origin.x, bounds.origin.y, bounds.origin.z],
                              "boundsExtent": [bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z]})
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level("/Game/Maps/map3")
terrain = unreal.load_asset("/Game/Airfight/Materials/lu-aer_Inst")
assert terrain
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if actor.get_actor_label() == "Mountain":
        actor.static_mesh_component.set_material(0, terrain)
        report["terrain"] = terrain.get_path_name()
assert report["terrain"]
assert level.save_current_level()
(out / "AircraftImport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MOVE_AIRCRAFT_IMPORTED J20_Map3 F35 terrain=original")
