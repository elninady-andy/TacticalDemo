"""Read-only snapshot verification; old migration snapshots remain selectable."""
import argparse
import hashlib
import json
import os
from pathlib import Path

root = Path(__file__).resolve().parent
if os.name == "nt" and not str(root).startswith("\\\\?\\"):
    root = Path("\\\\?\\" + str(root))
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--manifest", help="Snapshot path relative to the bundle, or an absolute path")
args = parser.parse_args()
name = args.manifest or ("CURRENT_MANIFEST.json" if (root / "CURRENT_MANIFEST.json").is_file() else "FILE_MANIFEST.json")
try:
    manifest = json.loads((root / name).read_text(encoding="utf-8-sig"))
except (OSError, ValueError) as error:
    raise SystemExit(f"Cannot read manifest {name}: {error}")
for item in manifest["files"]:
    path=root/item["path"]
    if not path.is_file():
        raise SystemExit("Missing: " + item["path"])
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(4*1024*1024),b""):
            digest.update(block)
    if path.stat().st_size!=item["bytes"] or digest.hexdigest()!=item["sha256"]:
        raise SystemExit("Mismatch: "+item["path"])
print("PACKAGE_FILES_VERIFIED",len(manifest["files"]))
print("Snapshot:", name)
