"""Read the supplied Move UE project; write reports to this demo's Saved directory."""
import json
from pathlib import Path
import unreal

output = Path(__file__).resolve().parents[2] / "Saved/MoveImport"
output.mkdir(parents=True, exist_ok=True)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions()
options.include_hard_package_references = True
options.include_soft_package_references = True
records = {}
for data in registry.get_assets_by_path("/Game", recursive=True, include_only_on_disk_assets=True):
    name = str(data.package_name)
    records[name] = {"class": str(data.asset_class_path.asset_name), "dependencies": [str(d) for d in registry.get_dependencies(name, options) or []]}
def vec(v):
    return [v.x, v.y, v.z]
report = {"sourceProject": unreal.Paths.project_dir(), "assets": records, "actors": []}
assert unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Airfight/Maps/map3")
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    row = {"name": actor.get_actor_label(), "class": actor.get_class().get_path_name(), "position": vec(actor.get_actor_location()), "scale": vec(actor.get_actor_scale3d()), "meshes": [], "splines": []}
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = component.static_mesh
        if mesh:
            row["meshes"].append({"component": component.get_name(), "asset": mesh.get_path_name(), "materials": [component.get_material(i).get_path_name() if component.get_material(i) else None for i in range(component.get_num_materials())]})
    for spline in actor.get_components_by_class(unreal.SplineComponent):
        points = []
        for i in range(spline.get_number_of_spline_points()):
            points.append({"position": vec(spline.get_location_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)), "tangent": vec(spline.get_tangent_at_spline_point(i, unreal.SplineCoordinateSpace.WORLD)), "type": str(spline.get_spline_point_type(i))})
        row["splines"].append({"name": spline.get_name(), "closed": spline.is_closed_loop(), "lengthCm": spline.get_spline_length(), "duration": spline.duration, "points": points,
                               "samples": [vec(spline.get_location_at_distance_along_spline(spline.get_spline_length()*i/256, unreal.SplineCoordinateSpace.WORLD)) for i in range(257)]})
    if "Airfight" in row["class"]:
        task = unreal.AssetExportTask()
        task.object = actor
        task.filename = str(output / (actor.get_name()+".t3d"))
        task.automated = True
        task.prompt = False
        task.exporter = unreal.ObjectExporterT3D()
        row["textExported"] = unreal.Exporter.run_asset_export_task(task)
    report["actors"].append(row)
(output / "SourceProject.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MOVE_SOURCE_INSPECTED actors=%d splineActors=%d" % (len(report["actors"]), sum(bool(a["splines"]) for a in report["actors"])))
