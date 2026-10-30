"""Record portable project inputs and saved scenarios, excluding local caches/venv.

Run only after an intentional release update. This records files; it does not
verify UE correctness, install dependencies, copy files, or create a runnable exe.
"""
import hashlib
import json
import os
from pathlib import Path

root = Path(__file__).resolve().parent
if os.name == "nt" and not str(root).startswith("\\\\?\\"):
    root = Path("\\\\?\\" + str(root))
files = set()
for directory in ("docs", "ResourceIndex", "TacticalDemo/Source", "TacticalDemo/Config",
                  "TacticalDemo/Content", "TacticalDemo/Scripts", "TacticalDemo/Binaries/Win64",
                  "TacticalDemo/Saved/Scenarios", "QwenRuntime/model"):
    folder = root / directory
    if not folder.is_dir():
        raise SystemExit("Missing portable directory: " + directory)
    for file in folder.rglob("*"):
        if file.is_file() and "__pycache__" not in file.parts and file.suffix.lower() not in (".pyc", ".pdb"):
            files.add(file)
for directory, suffixes in (("", (".md", ".py")), ("TacticalDemo", (".md", ".cmd", ".uproject", ".vsconfig")),
                            ("QwenRuntime", (".py", ".txt"))):
    files.update(file for file in (root / directory).iterdir() if file.is_file() and file.suffix.lower() in suffixes)
files.update(root / name for name in ("LocalPaths.example.json", "FILE_MANIFEST.json", "TacticalDemo/.vsconfig", "TacticalDemo/.gitignore"))
files.update(root / name for name in (".gitignore", ".gitattributes", "RepositoryManifest.json", "LICENSE") if (root / name).is_file())
records = []
for file in sorted(files):
    digest = hashlib.sha256()
    with file.open("rb") as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b""):
            digest.update(block)
    records.append({"path": file.relative_to(root).as_posix(), "bytes": file.stat().st_size, "sha256": digest.hexdigest()})
manifest = {"kind": "portable-source-project", "note": "Current file snapshot only; excludes LocalPaths.json, venv, caches, logs, debug symbols and historical Saved evidence except user scenarios. Requires UE and Python dependencies on the destination machine.", "files": records}
(root / "CURRENT_MANIFEST.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
print("CURRENT_MANIFEST_WRITTEN", len(records), "files", sum(item["bytes"] for item in records), "bytes")
