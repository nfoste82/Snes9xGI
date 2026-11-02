"""Pin source, every local TOML profile, and ordinary Release without mutation."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tree(root):
    return {str(p.relative_to(root)): {"symlink": str(p.readlink())} if p.is_symlink()
            else {"sha256": sha(p)} for p in sorted(root.rglob("*"))
            if p.is_symlink() or p.is_file()}


def pins():
    paths = subprocess.check_output(["git", "ls-files", "--cached", "--others", "--exclude-standard"], text=True).splitlines()
    return {"head": subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
            "status": subprocess.check_output(["git", "status", "--short"], text=True),
            "sources": {p: sha(Path(p)) for p in paths if Path(p).is_file()},
            "profiles": {str(p): sha(p) for root in (Path("remaster/profiles"), Path("build"))
                         for p in sorted(root.rglob("*.toml")) if p.is_file()},
            "ordinary_release": tree(Path("build/release/Snes9x.app"))}


if __name__ == "__main__":
    out = Path(sys.argv[1])
    data = pins()
    if len(sys.argv) > 2:
        before = json.loads(Path(sys.argv[2]).read_text())
        for key in ("profiles", "ordinary_release"):
            assert data[key] == before[key], key + " changed"
    out.write_text(json.dumps(data, indent=2) + "\n")
