"""Read-only source/profile/bundle provenance for an isolated outdoor build."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(root):
    return {str(p.relative_to(root)): ({"symlink": str(p.readlink())}
            if p.is_symlink() else {"sha256": digest(p)})
            for p in sorted(root.rglob("*")) if p.is_symlink() or p.is_file()}


if __name__ == "__main__":
    output = Path(sys.argv[1])
    output.mkdir(parents=True, exist_ok=False)
    profile = Path("remaster/profiles/ALttP/alttp-profile.toml")
    data = {"head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
            "status": subprocess.check_output(["git", "status", "--short"], text=True),
            "canonical_sha256": digest(profile),
            "ordinary_release": tree(Path("build/release/Snes9x.app")),
            "tracked_source": {p: digest(Path(p)) for p in subprocess.check_output(
                ["git", "ls-files"], text=True).splitlines() if Path(p).is_file()}}
    (output / "startup-pins.json").write_text(json.dumps(data, indent=2) + "\n")
    (output / "current-profile.toml").write_bytes(profile.read_bytes())
    (output / "startup.diff").write_bytes(subprocess.check_output(["git", "diff", "HEAD"]))
