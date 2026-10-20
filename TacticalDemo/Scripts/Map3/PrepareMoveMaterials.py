"""Copy the exact dependency closure of source aircraft/terrain materials, never overwrite."""
import argparse
import hashlib
import json
import shutil
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--source-project", required=True, type=Path)
args = parser.parse_args()
project = Path(__file__).resolve().parents[2]
out = project / "Saved/MoveImport"
source = json.loads((out / "SourceProject.json").read_text(encoding="utf-8"))
roots = set()
for actor in source["actors"]:
    if actor["name"] in ("BP_J20S2", "BP_F_35", "Mountain"):
        for mesh in actor["meshes"]:
            roots.update(path.split(".")[0] for path in mesh["materials"] if path)
pending, visited = list(roots), set()
while pending:
    package = pending.pop()
    if not package.startswith("/Game/") or package in visited:
        continue
    if package not in source["assets"]:
        raise RuntimeError("Missing source dependency " + package)
    visited.add(package)
    pending.extend(source["assets"][package]["dependencies"])
plan = []
for package in sorted(visited):
    if source["assets"][package]["class"] not in ("Material", "MaterialInstanceConstant", "MaterialFunction", "Texture2D"):
        raise RuntimeError("Unexpected material dependency " + package)
    relative = Path("Content") / (package.removeprefix("/Game/") + ".uasset")
    src, dst = args.source_project / relative, project / relative
    if not src.is_file():
        raise FileNotFoundError(src)
    digest = hashlib.sha256(src.read_bytes()).hexdigest()
    if dst.exists() and hashlib.sha256(dst.read_bytes()).hexdigest() != digest:
        raise RuntimeError("Refuse to overwrite existing package " + str(dst))
    plan.append({"package": package, "relative": str(relative), "sha256": digest})
for record in plan:
    src, dst = args.source_project / record["relative"], project / record["relative"]
    dst.parent.mkdir(parents=True, exist_ok=True)
    if not dst.exists():
        shutil.copyfile(src, dst)
    assert hashlib.sha256(dst.read_bytes()).hexdigest() == record["sha256"]
(out / "MaterialMigration.json").write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding="utf-8")
print("MATERIAL_DEPENDENCIES_COPIED", len(plan))
