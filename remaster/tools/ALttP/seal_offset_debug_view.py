"""Seal completed offset-view gates and byte preservation; never edits inputs."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from pin_offset_debug_inputs import pins, tree


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


if __name__ == "__main__":
    root = Path(sys.argv[1])
    before = json.loads((root / "startup-pins.json").read_text())
    after = pins()
    assert after["ordinary_release"] == before["ordinary_release"]
    assert all(after["profiles"].get(k) == v for k, v in before["profiles"].items())
    gates = list((root / "combined-final-full-gate").rglob("results.json"))
    assert len(gates) == 1
    gate = json.loads(gates[0].read_text())
    assert gate["status"] == "passed", gate["status"]
    assert all(t["status"] == "passed" for t in gate["tests"])
    for test in gate["tests"]:
        for name, h in test["source_fingerprint"]["files"].items():
            assert sha(Path(name)) == h, name
    assert "** BUILD SUCCEEDED **" in (root / "release-build-sealed.log").read_text()
    subprocess.run(["git", "diff", "--check"], check=True)
    after["preview_app"] = tree(Path("build/overworld-height-preview/Snes9x.app"))
    after["gate"] = {"path": str(gates[0]), "sha256": sha(gates[0]), "counts": gate["counts"]}
    after["preservation"] = {"ordinary_entries": len(before["ordinary_release"]),
                             "existing_profiles": len(before["profiles"]),
                             "new_test_fixture_profiles": len(set(after["profiles"]) - set(before["profiles"]))}
    (root / "final-pins.json").write_text(json.dumps(after, indent=2) + "\n")
    (root / "implementation.diff").write_bytes(subprocess.check_output(["git", "diff", "HEAD"]))
    artifacts = {str(p.relative_to(root)): sha(p) for p in sorted(root.rglob("*"))
                 if p.is_file() and p.name != "artifact-sha256.json"}
    (root / "artifact-sha256.json").write_text(json.dumps(artifacts, indent=2) + "\n")
    print(json.dumps(after["preservation"], indent=2))
