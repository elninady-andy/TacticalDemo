import unreal
import json
import os

report = {"models": [], "map": {}}
for path in ["/Game/plane/J20/mesh/J20S", "/Game/plane/J10/mesh/J10B"]:
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.StaticMesh):
        raise RuntimeError("Expected loadable StaticMesh: " + path)
    bounds = asset.get_bounds()
    report["models"].append({"asset": asset.get_path_name(), "origin": str(bounds.origin), "extent": str(bounds.box_extent), "materials": len(asset.static_materials)})
map_path = "/Game/VRAirport/Maps/VRAirportMap"
if not unreal.EditorLevelLibrary.load_level(map_path):
    raise RuntimeError("Could not load airport map")
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.EditorLevelLibrary.get_all_level_actors()
report["map"] = {"asset": map_path, "actorCount": len(actors), "gameMode": str(world.get_world_settings().get_editor_property("default_game_mode")), "starts": []}
for actor in actors:
    if isinstance(actor, unreal.PlayerStart):
        report["map"]["starts"].append({"name": actor.get_name(), "location": str(actor.get_actor_location())})
report["map"]["sampleActors"] = [{"name": a.get_name(), "location": str(a.get_actor_location())} for a in actors if isinstance(a, unreal.StaticMeshActor)][:8]
target = os.path.join(unreal.Paths.project_saved_dir(), "Evidence", "ImportedAssets.json")
os.makedirs(os.path.dirname(target), exist_ok=True)
with open(target, "w", encoding="utf-8") as f:
    json.dump(report, f, ensure_ascii=False, indent=2)
unreal.log("IMPORTED_ASSETS_VERIFIED " + json.dumps(report, ensure_ascii=False))
