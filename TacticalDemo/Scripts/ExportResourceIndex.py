"""Run with UE's Python commandlet; report on-disk and dynamic JSON references."""
import csv
import json
from pathlib import Path
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
output = project / "Saved" / "PortableResourceIndex"
output.mkdir(parents=True, exist_ok=True)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.search_all_assets(True)
options = unreal.AssetRegistryDependencyOptions()
options.include_hard_package_references = True
options.include_soft_package_references = True
assets = registry.get_assets_by_path("/Game", recursive=True, include_only_on_disk_assets=True)
records = {}
for asset in assets:
    package = str(asset.package_name)
    stem = project / "Content" / package.removeprefix("/Game/")
    files = [str(p.relative_to(project)).replace("\\", "/") for p in stem.parent.glob(stem.name + ".*") if p.suffix in (".umap", ".uasset", ".uexp", ".ubulk")]
    records[package] = {"package": package, "class": str(asset.asset_class_path), "files": sorted(files), "dependencies": sorted(str(d) for d in registry.get_dependencies(package, options) or [])}

missing = set()
def closure(roots):
    visited, pending = set(), list(roots)
    while pending:
        name = pending.pop()
        if name in visited:
            continue
        visited.add(name)
        if name.startswith("/Game/") and name not in records:
            missing.add(name)
            continue
        if name in records:
            if not records[name]["files"]:
                raise RuntimeError("No on-disk file for " + name)
            pending.extend(records[name]["dependencies"])
    return sorted(visited)

library = json.loads((project / "Config/EquipmentLibrary.json").read_text(encoding="utf-8-sig"))
dynamic = set()
def references(value):
    if isinstance(value, str) and value.startswith(("/Game/", "/Engine/")):
        dynamic.add(value.split(".")[0])
    elif isinstance(value, dict):
        for child in value.values():
            references(child)
    elif isinstance(value, list):
        for child in value:
            references(child)
references(library)
for line in (project / "Config/DefaultGame.ini").read_text(encoding="utf-8-sig").splitlines():
    if line.startswith(("UnitMaterialPath=", "HighlightMaterialPath=")):
        references(line.split("=", 1)[1].strip())
for name in dynamic:
    if not unreal.load_asset(name):
        raise RuntimeError("Cannot load registered model/material/texture: " + name)
maps = {}
for name, record in records.items():
    if any(f.endswith(".umap") for f in record["files"]):
        if not unreal.load_asset(name):
            raise RuntimeError("Cannot load map: " + name)
        maps[name] = {"files": record["files"], "mapDependencies": closure([name]), "runtimeDependencies": closure([name, *dynamic, "/Game/Materials/M_Unit"])}
all_dependencies = closure(records.keys())
result = {"assets": records, "maps": maps, "dynamicConfigPackages": sorted(dynamic), "allDependencies": all_dependencies, "note": "runtimeDependencies includes JSON registered models/textures, which are spawned by C++ at Play time."}
(output / "MissingDependencies.json").write_text(json.dumps(sorted(missing), ensure_ascii=False, indent=2), encoding="utf-8")
(output / "AssetIndex.json").write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
with (output / "AssetIndex.csv").open("w", encoding="utf-8-sig", newline="") as stream:
    writer = csv.writer(stream)
    writer.writerow(["UE package", "Class", "Project files", "Direct dependencies"])
    for name, record in sorted(records.items()):
        writer.writerow([name, record["class"], ";".join(record["files"]), ";".join(record["dependencies"])])
lines = ["# 地图与资源索引", "", "路径相对 TacticalDemo。/Game/ 对应 Content/，/Engine/ 由 UE 5.3 提供。", "", "飞机在 Play 时由 C++ 根据 JSON 生成；仅打开 umap 不会生成运行时编队。", ""]
for name, info in sorted(maps.items()):
    lines += ["## " + name, "", "地图文件：" + ", ".join(info["files"]), "", "| UE资源 | 工程文件 |", "| --- | --- |"]
    for dependency in info["runtimeDependencies"]:
        files = records.get(dependency, {}).get("files", [])
        lines.append("| " + dependency + " | " + ("<br>".join(files) if files else "UE引擎或内置插件提供") + " |")
    lines.append("")
(output / "MAP_RESOURCES.md").write_text("\n".join(lines), encoding="utf-8")
if missing:
    raise RuntimeError("Missing project dependencies: " + ", ".join(sorted(missing)))
unreal.log("PORTABLE_RESOURCES_VERIFIED assets=%d maps=%d" % (len(records), len(maps)))
