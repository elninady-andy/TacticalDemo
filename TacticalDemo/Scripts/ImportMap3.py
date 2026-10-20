"""Import the prepared map3 environment without changing existing resources.

Input: Saved/Map3Import/{SceneNodes.json,NewEnvironment.fbx}, prepared with
the UE-bundled FBX SDK. Existing meshes are reused only after bounds validation.
Aircraft remain managed by C++; exported weather/animation nodes are excluded.
"""
import json
import runpy
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
evidence = project / "Saved/Map3Import"
nodes = json.loads((evidence / "SceneNodes.json").read_text(encoding="utf-8"))
assets = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
meshes, materials = {}, {}
for data in registry.get_assets_by_path("/Game", recursive=True):
    if str(data.package_name).startswith("/Game/Map3/"):
        continue
    kind = str(data.asset_class_path.asset_name)
    if kind == "StaticMesh":
        meshes.setdefault(str(data.asset_name), []).append(str(data.package_name))
    elif kind in ("Material", "MaterialInstanceConstant"):
        materials.setdefault(str(data.asset_name), []).append(str(data.package_name))

new_names = {"Mountain", "Landscape1", "bighouse_Object997", "bighouse_Object998",
             "bighouse_Box008_4", "bighouse_Object999", "Airport_Object905",
             "Airport_Object906", "Airport_Object907"}
skip = {"BP_F_35", "Rainbow", "Ultra_Dynamic_Sky_Sphere", "StaticCloudsSphere", "Overcast_Turbulence"}
environment = [n for n in nodes if not n["name"].startswith(("UCX_", "J20S_")) and n["name"] not in skip]
report = {"reused": [], "imported": [], "fallbackMaterials": [], "reusedMaterialFallbacks": [],
          "excluded": [n["name"] for n in nodes if n not in environment]}

def xyz(v):
    return [v.x, v.y, v.z]

# Fail before modifying anything if an apparent name match has different geometry bounds.
for node in environment:
    name = node["name"]
    if name in new_names:
        continue
    candidates = meshes.get(name, [])
    if len(candidates) != 1:
        raise RuntimeError("Ambiguous or missing mesh: " + name)
    mesh = unreal.load_asset(candidates[0])
    bounds = mesh.get_bounds()
    lo = xyz(bounds.origin - bounds.box_extent)
    hi = xyz(bounds.origin + bounds.box_extent)
    delta = max(abs(a-b) for a,b in zip(lo+hi, node["localMin"]+node["localMax"]))
    if delta > 1.0:
        raise RuntimeError("Mesh bounds differ from FBX: %s (%s cm)" % (name, delta))
    node["asset"] = candidates[0]
    report["reused"].append({"name": name, "asset": candidates[0], "boundsErrorCm": delta})

if assets.does_asset_exist("/Game/Maps/map3"):
    raise RuntimeError("map3 already exists; refuse to overwrite user edits")
binding_helpers = runpy.run_path(str(Path(__file__).parent / "Map3/MaterialBindings.py"))
airport_bindings = binding_helpers["capture_airport_bindings"]()
(evidence / "AirportMaterialBindings.json").write_text(json.dumps(airport_bindings, ensure_ascii=False, indent=2), encoding="utf-8")
tools = unreal.AssetToolsHelpers.get_asset_tools()
if not assets.list_assets("/Game/Map3/Geometry", recursive=True):
    options = unreal.FbxImportUI()
    options.set_editor_property("automated_import_should_detect_type", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    static = options.static_mesh_import_data
    for key, value in {"combine_meshes": False, "auto_generate_collision": False,
                       "generate_lightmap_u_vs": False, "build_nanite": False,
                       "transform_vertex_to_absolute": True}.items():
        static.set_editor_property(key, value)
    task = unreal.AssetImportTask()
    task.filename = str(evidence / "NewEnvironment.fbx")
    task.destination_path = "/Game/Map3/Geometry"
    task.automated = True
    task.replace_existing = False
    task.save = True
    task.options = options
    tools.import_asset_tasks([task])
    unreal.log("MAP3_IMPORTED_PATHS " + str(task.imported_object_paths))

new_meshes = {}
for path in assets.list_assets("/Game/Map3/Geometry", recursive=True):
    mesh = unreal.load_asset(path)
    if isinstance(mesh, unreal.StaticMesh):
        name = mesh.get_name()
        matches = [n for n in new_names if name == n or name.endswith("_" + n)]
        if len(matches) != 1:
            raise RuntimeError("Unexpected imported mesh " + name)
        new_meshes[matches[0]] = mesh
if set(new_meshes) != new_names:
    raise RuntimeError("New geometry set incomplete: " + str(set(new_meshes)))

fallback_path = "/Game/Map3/Materials/M_Map3Terrain"
terrain = unreal.load_asset(fallback_path) if assets.does_asset_exist(fallback_path) else None
if not terrain:
    terrain = tools.create_asset("M_Map3Terrain", "/Game/Map3/Materials", unreal.Material, unreal.MaterialFactoryNew())
    color = unreal.MaterialEditingLibrary.create_material_expression(terrain, unreal.MaterialExpressionConstant3Vector, -200, 0)
    color.set_editor_property("constant", unreal.LinearColor(0.12, 0.16, 0.08, 1))
    unreal.MaterialEditingLibrary.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(terrain)
    assets.save_loaded_asset(terrain)

for name, mesh in new_meshes.items():
    for index, slot in enumerate(mesh.static_materials):
        original = str(slot.get_editor_property("imported_material_slot_name"))
        candidates = materials.get(original, [])
        material = unreal.load_asset(candidates[0]) if len(candidates) == 1 else None
        if not material:
            material = terrain if name in ("Mountain", "Landscape1") else unreal.load_asset("/Engine/EngineMaterials/WorldGridMaterial")
            report["fallbackMaterials"].append({"mesh": name, "slot": index, "source": original, "replacement": material.get_path_name()})
        mesh.set_material(index, material)
    assets.save_loaded_asset(mesh)
    report["imported"].append(mesh.get_path_name())

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for node in environment:
    mesh = new_meshes[node["name"]] if node["name"] in new_names else unreal.load_asset(node["asset"])
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0))
    actor.set_actor_label(node["name"])
    actor.static_mesh_component.set_static_mesh(mesh)
    if node["name"] not in new_names:
        rotation = unreal.Quat(*node["quaternion"]).rotator()
        actor.set_actor_location_and_rotation(unreal.Vector(*node["position"]), rotation, False, True)
        actor.set_actor_scale3d(unreal.Vector(*node["scale"]))
        # FBX can omit unused slots; use the original UE instance slot layout.
        report["reusedMaterialFallbacks"].extend(binding_helpers["apply_airport_binding"](actor, airport_bindings))
    actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
sun = actors.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 3000), unreal.Rotator(-55, -30, 0))
sun.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sun.light_component.set_editor_property("intensity", 5.0)
sky = actors.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 1000))
sky.light_component.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("intensity", 1.0)
actors.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
world.get_world_settings().set_editor_property("default_game_mode", unreal.load_class(None, "/Script/TacticalDemo.DemoGameMode"))
if not unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/Maps/map3"):
    raise RuntimeError("Cannot save map3")
report["environmentActorCount"] = len(environment)
(evidence / "ImportReport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MAP3_IMPORT_VERIFIED " + str(len(environment)) + " environmental actors; " + str(len(report["reused"])) + " reused meshes")
