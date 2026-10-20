"""Read-only verification of map3 assets, transforms and default startup configuration."""
import configparser
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir())
out = project / "Saved/Map3Import"
nodes = {n["name"]: n for n in json.loads((out / "SceneNodes.json").read_text(encoding="utf-8"))}
report = json.loads((out / "ImportReport.json").read_text(encoding="utf-8"))
bindings = json.loads((out / "AirportMaterialBindings.json").read_text(encoding="utf-8"))
ini = configparser.ConfigParser(strict=False)
ini.read(project / "Config/DefaultEngine.ini", encoding="utf-8-sig")
for key in ("EditorStartupMap", "GameDefaultMap"):
    assert ini["/Script/EngineSettings.GameMapsSettings"][key].split('.')[0] == "/Game/VRAirport/Maps/VRAirportMap", key
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Maps/map3")
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_world_settings().get_editor_property("default_game_mode").get_name() == "DemoGameMode"
found, empty_slots, terrain_materials = [], [], {}
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor):
        continue
    node = nodes[actor.get_actor_label()]
    component = actor.static_mesh_component
    mesh = component.static_mesh
    assert mesh
    if node['name'] in ('Mountain', 'Landscape1'):
        terrain_materials[node['name']] = component.get_material(0).get_path_name()
    if node['name'] == 'Mountain':
        assert terrain_materials['Mountain'] == '/Game/Airfight/Materials/lu-aer_Inst.lu-aer_Inst'
    for index in range(component.get_num_materials()):
        if not component.get_material(index):
            empty_slots.append({"actor": node["name"], "slot": index, "source": str(mesh.static_materials[index].material_slot_name)})
    if mesh.get_path_name().startswith("/Game/Map3/"):
        bounds = mesh.get_bounds()
        lo, hi = bounds.origin - bounds.box_extent, bounds.origin + bounds.box_extent
        actual = [lo.x, lo.y, lo.z, hi.x, hi.y, hi.z]
        error = max(abs(a-b) for a,b in zip(actual, node["worldMin"]+node["worldMax"]))
        assert error < 1, (node["name"], "baked mesh bounds", error)
    else:
        expected = bindings[node["name"]]
        assert mesh.get_path_name() == expected["mesh"]
        assert component.get_num_materials() == len(expected["materials"])
        for index, path in enumerate(expected["materials"]):
            actual = component.get_material(index)
            assert actual and actual.get_path_name() == (path or "/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"), (node["name"], index)
        position = actor.get_actor_location()
        assert max(abs(a-b) for a,b in zip([position.x, position.y, position.z], node["position"])) < 0.1
        rotation = actor.get_actor_rotation().quaternion()
        # q and -q describe the same rotation.
        dot = sum(a*b for a,b in zip([rotation.x,rotation.y,rotation.z,rotation.w], node["quaternion"]))
        assert abs(abs(dot)-1) < 1e-5
        scale = actor.get_actor_scale3d()
        assert max(abs(a-b) for a,b in zip([scale.x, scale.y, scale.z], node["scale"])) < 1e-5
    found.append(node["name"])
assert len(found) == report["environmentActorCount"] == 105
(out / "EmptyMaterialSlots.json").write_text(json.dumps(empty_slots, ensure_ascii=False, indent=2), encoding="utf-8")
assert not empty_slots, "Empty material slots; see EmptyMaterialSlots.json"
result = {"map": "/Game/Maps/map3", "environmentActors": len(found),
          "reusedMeshes": len(report["reused"]), "newMeshes": len(report["imported"]),
          "initialImportFallbackMaterialSlots": report["fallbackMaterials"], "defaultStartup": "airport",
          "currentTerrainMaterials": terrain_materials,
          "reusedMaterialFallbacks": report.get("reusedMaterialFallbacks", []),
          "transformsAndMaterialBindings": "passed"}
(out / "AssetVerification.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MAP3_ASSETS_VERIFIED environment=105 reused=96 new=9 default=airport")
