"""Record tracked source and LFS assets; stage intentional changes before running.

This snapshot excludes ignored machine files, model weights, binaries and personal
scenarios. It does not replace CURRENT_MANIFEST.json for a full portable package.
"""
import hashlib
import json
import os
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parent
names = subprocess.check_output(["git", "ls-files", "-z"], cwd=root).decode("utf-8").split("\0")
if os.name == "nt" and not str(root).startswith("\\\\?\\"):
    root = Path("\\\\?\\" + str(root))
records = []
for name in sorted(filter(None, names)):
    if name == "RepositoryManifest.json":
        continue
    path = root / name
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    records.append({"path": name, "bytes": path.stat().st_size, "sha256": digest.hexdigest()})
manifest = {"kind": "github-source-project", "note": "Requires Git LFS assets and UE 5.3.2; model weights, generated binaries and personal scenarios are excluded.", "files": records}
(root / "RepositoryManifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("REPOSITORY_MANIFEST_WRITTEN", len(records), "files", sum(item["bytes"] for item in records), "bytes")
