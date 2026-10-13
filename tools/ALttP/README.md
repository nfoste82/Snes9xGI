# ALttP tools

ALttP-specific scripts, portable utilities, tests, metadata, and investigation
notes live here. Run commands from the repository root, for example:

```sh
python3 -B tools/ALttP/alttp_tile_semantic_atlas.py --help
python3 -B tools/ALttP/alttp_atlas_profile.py --help
python3 -B -m unittest discover -s tools/ALttP -p 'test_alttp_*.py'
c++ -std=c++17 -O2 tools/ALttP/alttp_profile_validate.cpp -o /tmp/alttp-profile-validate
```

Profiles are in `Profiles/ALttP/`. Authored tile semantics are in
`tools/ALttP/alttp-tile-semantics-v1.json`; generated atlas and inspection
artifacts remain under `build/remaster-geometry/`.

For assembled NPC poses, facing/animation browsing, canonical tile editing and
image-derived default normals/heights, see [NPC-WORKBENCH.md](NPC-WORKBENCH.md).

Historical notes and generated reports may refer to the former `tools/alttp…`
paths. Use `tools/ALttP/alttp…` for those files now. Python module names remain
unchanged; their sibling imports resolve when scripts are run directly.
