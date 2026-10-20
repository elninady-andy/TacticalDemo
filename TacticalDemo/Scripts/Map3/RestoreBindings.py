"""Complete map3's existing-mesh material bindings, saving only map3."""
import json
import runpy
from pathlib import Path
import unreal

helpers = runpy.run_path(str(Path(__file__).with_name("MaterialBindings.py")))
bindings = helpers["capture_airport_bindings"]()
project = Path(unreal.Paths.project_dir())
out = project / "Saved/Map3Import"
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assert level.load_level("/Game/Maps/map3")
fallbacks, count = [], 0
for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if not isinstance(actor, unreal.StaticMeshActor) or actor.static_mesh_component.static_mesh.get_path_name().startswith("/Game/Map3/"):
        continue
    fallbacks.extend(helpers["apply_airport_binding"](actor, bindings))
    count += 1
assert count == 96, count
assert level.save_current_level()
report = json.loads((out / "ImportReport.json").read_text(encoding="utf-8"))
report["reusedMaterialFallbacks"] = fallbacks
(out / "AirportMaterialBindings.json").write_text(json.dumps(bindings, ensure_ascii=False, indent=2), encoding="utf-8")
(out / "ImportReport.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
unreal.log("MAP3_ACTOR_MATERIALS_BOUND reused=%d fallbackSlots=%d" % (count, len(fallbacks)))
