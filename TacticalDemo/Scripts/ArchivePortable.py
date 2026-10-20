"""Finalize an already verified staging directory and validate every ZIP member."""
import argparse
import hashlib
import json
import zipfile
from pathlib import Path

def digest_stream(stream):
    digest = hashlib.sha256()
    for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
        digest.update(block)
    return digest.hexdigest()

parser = argparse.ArgumentParser()
parser.add_argument("--stage", required=True)
parser.add_argument("--archive", required=True)
parser.add_argument("--replace", action="store_true", help="Replace a previously generated archive with the same bundle root")
args = parser.parse_args()
stage, archive = Path(args.stage).resolve(), Path(args.archive).resolve()
if archive.exists() and not args.replace:
    raise SystemExit("Archive already exists; use a new filename")
if archive.exists():
    with zipfile.ZipFile(archive) as previous:
        if stage.name + "/FILE_MANIFEST.json" not in previous.namelist():
            raise SystemExit("Refusing to replace an unrelated archive")
if stage == archive.parent or stage in archive.parents:
    raise SystemExit("Archive must be outside the staging directory")
if not (stage / "TacticalDemo/TacticalDemo.uproject").is_file():
    raise SystemExit("Not a portable UE staging directory")
def included(path):
    parts=path.relative_to(stage).parts
    if parts[0]=="TacticalDemo" and len(parts)>1 and parts[1] in (".vs", "Intermediate", "DerivedDataCache"):
        return False
    if "__pycache__" in parts or path.suffix==".pyc":
        return False
    return path.is_file() and path.name!="FILE_MANIFEST.json"

files = []
for path in sorted(p for p in stage.rglob("*") if included(p)):
    with path.open("rb") as stream:
        digest = digest_stream(stream)
    files.append({"path": path.relative_to(stage).as_posix(), "bytes": path.stat().st_size, "sha256": digest})
manifest = stage / "FILE_MANIFEST.json"
manifest.write_text(json.dumps({"files": files, "totalBytes": sum(i["bytes"] for i in files)}, ensure_ascii=False, indent=2), encoding="utf-8")
print("MANIFEST_READY", len(files), flush=True)
archive.parent.mkdir(parents=True, exist_ok=True)
with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED, compresslevel=1, allowZip64=True) as zipped:
    for path in sorted([stage/item["path"] for item in files]+[manifest]):
        zipped.write(path, (Path(stage.name) / path.relative_to(stage)).as_posix())
print("ARCHIVE_WRITTEN", archive, archive.stat().st_size, flush=True)
with zipfile.ZipFile(archive) as zipped:
    prefix = stage.name + "/"
    entries = set(zipped.namelist())
    expected = {prefix + item["path"] for item in files} | {prefix + "FILE_MANIFEST.json"}
    if entries != expected:
        raise RuntimeError("ZIP contents differ from manifest")
    if json.loads(zipped.read(prefix + "FILE_MANIFEST.json")) != json.loads(manifest.read_text(encoding="utf-8")):
        raise RuntimeError("ZIP manifest differs from staging manifest")
    for item in files:
        with zipped.open(prefix + item["path"]) as stream:
            if digest_stream(stream) != item["sha256"]:
                raise RuntimeError("ZIP hash mismatch: " + item["path"])
with archive.open("rb") as stream:
    archive_digest = digest_stream(stream)
archive.with_suffix(".zip.sha256.txt").write_text(archive_digest + "  " + archive.name + "\n", encoding="ascii")
print("ARCHIVE_VERIFIED", len(files), "SHA256", archive_digest, flush=True)
