# ALttP tools

ALttP-specific scripts, portable utilities, tests, metadata, and investigation
notes live here. Run commands from the repository root, for example:

```sh
python3 -B remaster/tools/ALttP/alttp_tile_semantic_atlas.py --help
python3 -B remaster/tools/ALttP/alttp_atlas_profile.py --help
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_*.py'
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_profile_validate.cpp -o /tmp/alttp-profile-validate
```

Profiles are in `remaster/profiles/ALttP/`. Authored tile semantics are in
`remaster/tools/ALttP/alttp-tile-semantics-v1.json`; generated atlas and inspection
artifacts remain under `build/remaster-geometry/`.

Keep local profile backups out of Git; the canonical profile is versioned already.
Historical snapshots removed during cleanup remain available with `git show`:

```sh
git show e99b2841:Profiles/ALttP/alttp-profile_backup8.toml > build/alttp-profile-backup8.toml
git show e99b2841:Profiles/ALttP/alttp-profile.toml > build/alttp-profile-backup9.toml
git show 15f012b4:Profiles/ALttP/alttp-profile.toml > build/alttp-profile-pre-artwork-emission.toml
```

Backups 9 and 10 were identical. The emission migration tool and its tests are
still supported; its `.toml.bak` safety snapshots are local-only.

Standalone C++ tests and audit tools are not app build inputs. Keep assertion-bearing
regressions and shared replay/corpus harnesses even when they are outside the curated
`remaster/tools/run_remaster_tests.py` suite. One-off hardcoded diagnostic dumpers can be
retired without deleting their historical reports. For profile canonical round-trip
checks, use `alttp_profile_validate.cpp` rather than a separate diagonal checker.

For assembled NPC poses, facing/animation browsing, canonical tile editing and
image-derived default normals/heights, see [NPC-WORKBENCH.md](NPC-WORKBENCH.md).

Historical notes and generated reports may refer to the former `remaster/tools/alttp…`
paths. Use `remaster/tools/ALttP/alttp…` for those files now. Python module names remain
unchanged; their sibling imports resolve when scripts are run directly.
