"""Exercise verification against isolated tiny snapshots, never project assets."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

VERIFIER = Path(__file__).resolve().parents[2] / "VerifyPackage.py"


class PackageVerifierTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        shutil.copyfile(VERIFIER, self.root / "VerifyPackage.py")
        (self.root / "asset.txt").write_bytes(b"current")
        self.snapshot("CURRENT_MANIFEST.json", b"current")
        self.snapshot("FILE_MANIFEST.json", b"original")

    def snapshot(self, name, value):
        (self.root / name).write_text(json.dumps({"files": [{"path": "asset.txt", "bytes": len(value), "sha256": hashlib.sha256(value).hexdigest()}]}), encoding="utf-8")

    def run_verifier(self, *args):
        return subprocess.run([sys.executable, str(self.root / "VerifyPackage.py"), *args], capture_output=True, text=True)

    def test_current_snapshot_is_default(self):
        result = self.run_verifier()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("PACKAGE_FILES_VERIFIED 1", result.stdout)

    def test_explicit_original_is_not_silently_replaced(self):
        result = self.run_verifier("--manifest", "FILE_MANIFEST.json")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Mismatch: asset.txt", result.stderr)

    def test_same_length_tampering_is_detected(self):
        (self.root / "asset.txt").write_bytes(b"changed")
        self.assertNotEqual(self.run_verifier().returncode, 0)

    def test_missing_file_is_reported(self):
        (self.root / "asset.txt").unlink()
        result = self.run_verifier()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("Missing: asset.txt", result.stderr)

    def test_original_only_bundle_still_works(self):
        (self.root / "CURRENT_MANIFEST.json").unlink()
        self.snapshot("FILE_MANIFEST.json", b"current")
        self.assertEqual(self.run_verifier().returncode, 0)


if __name__ == "__main__":
    unittest.main()
