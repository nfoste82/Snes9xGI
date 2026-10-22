# Cardinal floor relief continuity (Room051 own035)

## Result and cause

Pinned `frame_room051_035.s9xrmf`, hashes `773dcf4a53f87b93` and
`0c2a4c549a67b13f`: eight BG2 usages at x23/40,y7..10 lacked plane0
support bases (255), so their visible base incorrectly came from plane1 zero.
Both are authored Floor with 0..1 relief. Classification intentionally defers
nonflat floor art from the flat-plateau seam graph. The one-tile-wide strips
cannot contain a complete Link body, so neither structural nor movement paths
supplied the upper base. This was not absent profile geometry.

`frame.h` now assigns terminal floor bases through same-BG cardinal floor
connectivity, separately on observed gameplay planes. It requires explicit Floor
classification/authoring, local geometry, ordinary collision and structural-plane
eligibility, plus independently occupied solved support. Disagreeing existing
bases or bad structural nodes reject the region. Unknown art, wall/obstacle
assemblies, excluded planes, hidden distinct BG1 art and diagonal-only contacts
cannot connect regions. No movement components, constraints, reachable counts,
or authored arrays are changed. Relief does not become a rise/plateau datum.

All64 reported-hash usages now have visible/plane0 base50. Eight missing bases
are repaired; the other56 are unchanged. Effective heights are50..51, original
normals/arrays retained. Production EndFrame verifies110,803 identity/flip/camera
samples and896 affected main/sub/scene pixel usages. Existing platform34/throne32
regression also passes unchanged (4,203 pixel usages).

## Tests and corpus

* `dungeon_floor_continuity_test.cpp`: both BGs, four flips, raised24 support,
  narrow relief strips, preserved local arrays, actual packet application,
  no movement inflation, unknown/wall/solid/plane barriers, diagonal rejection,
  and a strip touching conflicting upper24/lower0 support. Optimized and
  ASan/UBSan pass. Included in the full portable gate.
* `dungeon_floor_continuity_probe.cpp`: complete own-input production EndFrame
  check, all64 occurrences/eight narrow cells, actual pixel bases and local sources.
* `dungeon_floor_continuity_corpus_probe.cpp`: extends the complete cache corpus
  probe with both-BG/both-plane authored-floor denominators. Its explicit success
  return avoids falling off a macro-renamed main in the wrapper.
* Full portable gate23/23; runner Python6/6; validator Python26/26;
  Debug/Release arm64 builds and `git diff --check` pass.

Corpus uses346default/entrance contexts across319room IDs, both BGs/planes,
unchanged capture profile. Authored Floor+local-source+ordinary-collision
denominator161,724; missing70,340→69,940 (400 recovered,40contexts). Counts include
unobserved planes and preexisting classification/support gaps, not just reachable
floor. Structural conflict-node sum17,983 unchanged. Existing movement missing,
unknown wall and obstacle-solved counters unchanged. Complete cached/full maps,
diagnostics and packet comparisons pass. Synthetic animation is a cache stress
test, not attested gameplay or all-state approval.

Artifacts: `build/room051035-floor-corpus-{before,after}.json`,
`build/room051035-{before,after}-bg{0,1}.json`,
`build/room051035-floor-after.s9xrmf`,
`build/room051035-tests-complete/20261022T191113Z-ynpg6gxj/results.json`.
Baseline solver/corpus builds define `ALTTP_TEST_NO_FLOOR_CONTINUITY`, which
ablates only this repair while preserving the rest of the dirty checkout.

## Mandatory complete-room validation v2: still fails, not room approval

Matching lossless ALTPPD1, complete ALTPAT1, captured state and profile reproduced
in `build/room051035-validation-{before,after}/`. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
profile serialized FNV1a64 `3b98a54cc03c23ea`; profile SHA256
`370b3d0d58edf65b61a506b5560d49b2642b9dd24e2928e78e82618a9339abb3`.
Gate manifests preserve all packet/input/state/template/terrain/sprite pins.

Validator now additionally enumerates all authored floor usages on both complete
BGs/planes, including nonreachable relief, with base/local/effective ranges,
flips, fallback directions and missing denominators. This inventory explicitly
reports runtime base claims and independently pinned local sources; it is NOT
an independent proof of floor ownership and does not relabel movement coverage.
Python regression tests the extra denominator and missing-plane retention.

Floor inventories (solved/required):

| BG | Plane | Before | After |
|---|---|---|---|
| BG1 | 0 |110/922|110/922|
| BG1 | 1 |0/3780|0/3780|
| BG2 | 0 |890/898|898/898|
| BG2 | 1 |830/838|830/838|

Required reachable plane denominators[76,0],76/76valid,0missing/overlap;
fallback10/10,0conflicts. Existing BG2walls564solved/54unknown;
runtime structural conflict count4; authored obstacles64/144solved,
16missing/64ambiguous-overlap,24/28contacts; doorframes8/12solved;
compiled map0missing/8mismatches. These existing validation counts are unchanged.
No directed stair playback was supplied or invented. Independent support proof
for every nonreachable floor, unobserved-plane ownership, unresolved walls,
overlapping groups and doorframe ownership remain general capabilities/gaps;
this repair is not full-room/game-wide approval.

## Reproduction

```sh
c++ -std=c++17 -O2 remaster/dungeon_floor_continuity_probe.cpp -o build/room051035-floor-probe
build/room051035-floor-probe "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_035.s9xrmf" build/room051035-floor-after
c++ -std=c++17 -O2 tools/ALttP/alttp_runtime_room_heights.cpp -o build/room051035-solver
python3 -B tools/ALttP/alttp_replay_live_height_evidence.py "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_035.s9xrmf" --solver build/room051035-solver --output build/room051035-validation-after --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B remaster/sprite_obstacle_gate.py --capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_035.s9xrmf" --output build/room051035-validation-after --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
```

Create output directories first. No authored file/capture was overwritten.
Fresh interactive gameplay comparison not performed. Relaunch
`build/release/Snes9x.app` or `build/debug/Snes9x.app`; historical captures retain
their original geometry unless replayed through the production probe.
