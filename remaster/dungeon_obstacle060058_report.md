# Room060 / own058 — reviewable but incomplete

Shared runtime repair implemented; **not room approval or capture approval**.
Own immutable inputs/profile/artwork were not rewritten. No app lifecycle, git
stage/commit/reset or app build operations. Parent owns independent tests/builds.
Profile22/frame30, per-member authored/fallback policy and bounded eight-state
exact cache are unchanged.

## Diagnosis and real effective-height result

Own058 is settled module7/submodule0/phase0, room0x060, previous0x061,
plane0, collisionMode0, Link[384,3454], scroll[256,3345], destination[0,3072]
(not attested), graphicsBG1/BG2base16384, both complete maps size3, rise50.
ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Own serialized profile FNV `3e493450dbd28e3b`;
SHA256 `caf4ca7d1f51d5900d484c447645b7e17dd769dcff5393e0e91a69b901e0bcfa`.
Packet SHA256 `1041e4f7334054fd4efa9bf906eed63ee246e30d5876b837cc530ed12f7513dd`.
Complete input SHA256 `7f893360602149ef00a469eccf620abc60da18e51a8ea699ec86a006669871dc`.
Template SHA256 `be6a8a9c5b436772f96700973307ec81fc31843e06118e0e567b658e59bc5ddd`.
All six source pins (including state/terrain) are in OUT/summary.json.

The square obstacles are already authored group4 `Floor_torch_pedastal`, not
missing tile metadata: four width1 height2 copies at x44,45,50,51 / y54..55.
Hashes top `9943c286d5e45726` local1..7, foot `fd86d3ed217c0d03` local0..7,
top final words0dc0/4dc0, plane attributes27/00 (hex).
Declared foot contacts y55 touch occupied support y56, plane0 component0 base24.
The four group1 `Wall_pillar` copies at the same x / y58..61 are globally
V-reflected. Their explicit canonical foot becomes y58, outward edge y57;
both occupied foot and outward support witness plane0 component0 base24.
Hashes y58..61: `abe85623ae3f8007`0..5, `ba4694946b93ee92`6..31,
`25b414f2340a85a6`13..18, `2ad3233860fffd3b`18..18.
Ownership is the complete exact authored footprint/flips, not a hash whitelist or
inferred ROM writer. No historical live dispatch stream is present/claimed.

The underlying BG1 art is a DISTINCT complete floor pattern, hashes
`d2ef88b0a91300d6` / `4d5247551edca28e`, explicit authored Floor with all64
local pixels0; completed lower attributes00. Reproduced lower body occupancy
remains separate at base0. Prior group contact voting wrongly admitted that lower
support as another owner of the upper obstacle, conflicting with base24.
Two earlier classification losses obscured that provenance: lower floor activation
consulted asset groups but ignored explicit Floor roles, and group reset discarded
cross-BG usage/floor masks. Final map words/graphics, not screenshot proximity,
identify this underlying support.

Reusable rule: preserve those contextual fields during group classification;
activate explicit lower Floor roles. Only a COMPLETE distinct BG1 classified
zero-local floor under every matched BG2 member, corroborated by upper body
attributes limited to00/01/02/27 with at least one01/02/27 and all lower00,
excludes lower occupancy from obstacle ownership. 27 is Hookshottables in verified
reference TileDetect (tile_detect.c342), not a height label or movement edge.
Every declared upper contact must still exist and agree on component/plane/base.
Missing/nonflat lower metadata, unknown special body attributes, all00 bodies,
BG1 groups and competing ownership retain the original conservative voting.
Independent Python v2 proof reconstructs the same evidence without native claims.

| own058 result | before / policy ablation | after |
|---|---:|---:|
| exact packet BG identity/flip/camera matches |103279|103279|
| required/solved group members |24/0|24/24|
| conflicting group assemblies |8|0|
| actual visible obstacle pixel usages raised |0/1536|1536/1536|
| authored local replacements |0|0|
| BG2 structural conflict count |11|3|
| BG2 solved/unknown walls |1017/9|1017/9|
| BG1 unknown walls / conflicts |0/0|0/0|

All24members use floorBase24, not configured rise50, nor local/Z rebasing.
Pedestal effective pixels24..31; pillars compose unchanged relief with base24
(e.g. middle30..55). Authored Z ignored; missing arrays retain generated displayed
bottom0/top7 + Z*8 + shared floor base. Actor plane maps are NOT changed to member
geometry. This is actual production EndFrame packet composition, not debug tint.

## Verification, corpus and mandatory gate

PASS native policy regression O2 and ASan/UBSan: transformed contacts/allH/V,
explicit lower role without group, lower nonflat rejection, missing/overwritten/
overlapping/plane/component ownership rejection, signed support, authored arrays,
mixed generatedZ, eight exact states/cache versus full inventories. PASS Python
authored10 (includes signed base NOT higher-plane selection and special-attribute
rejection), validator24, diagonal5. PASS existing symmetric column, statue assembly,
pot placement, geometry cache and complete placement-equivalence own060 input.
Own066 NW and own048 SW actual production gradient assertions pass with their
OWN profile/capture pins:112589 /112531 BG pixel matches respectively.
`git diff --check` passes. No app build or fresh GUI/gameplay comparison performed.

Reference audit before/after:346/346contexts,319rooms, bothBGs/planes;0 missing
inputs/refusals, immutable own058 profile. Required authored members1802;
solved664→712; unresolved1138→1090, including794unmatched. Contact claims284;
witnessed226→222 (excluding non-owning lower witnesses exposes4 genuinely missing
upper contacts); assembly conflicts16→0. All cache/full complete maps/diagnostics/
packets equal. Affected contexts051/entrance116,060/3,062/5. This is sampled reference
default/entrance states, not exhaustive dynamic states; one of320rooms absent.
Own-input corpus165packets:46replayed,112unsupported missing sidecars,
7stale map/packet/camera refusals; all original inputs immutable. This corpus uses
each packet's own historical profile, not current metadata substituted everywhere.

Mandatory v2 actually ran over own058 lossless complete ALTPPD1 + matching
ALTPAT1 both planes, own profile, pinned local state, complete reexplored doorway
graph and compiled signed ALTPHM3. All8192WRAM/PPUwords agree and template sidecar
agrees with verified ROM. Playback stairs borrowed ONLY after exact completed
attribute/stair equality; no fresh Snes9x movement playback claimed.
**Gate exit1**: reachable supports711/711, planes[437,274],0missing support;
24overlap coordinates kept independent; immediate fallback12/12; authored proof
24/24members,8/8contacts,0missing/conflicts/native-Python pixel mismatches.
Runtime wider ordinary support scope required[1581,3628], missing[0,0]; persistent
potential removed-pot support[12,0], missing[0,0]. These are DIFFERENT denominators
from doorway reachable support. Directional inventory enumerates16384BG/plane
usages; this does not claim all8192placements solved geometric/excluded.

Unresolved room gates:9unknown BG2walls,3native structural conflicts,
1conflicting component; door marked18/24solved; directional wall-plane coverage
2728placements/10912edges with1550missing and67seam conflicts. Compiled artifact
check reports8missing/38mismatches (support/structural projection gaps, not group
pixel mismatches). Preserve these failures; zero obstacle unknowns is NOT complete
room coverage. General next capability: resolve wall/door contact provenance and
structural plane representation/export, then rerun independent complete-room gate.
Fresh matching gameplay across relevant views/planes still needed after parent
build. Do not hand this room back as approved for capture.

## Independent rerun commands / artifacts

### Parent independent verification

Parent rebuilt and ran the own058 production probe independently: 103279 exact
BG usages matched; all 24 obstacle cells and 1536 visible obstacle usages received
base24, with zero local overrides and eight conflict-free assemblies. Output prefix:
`$ROOT/parent060058-after`. Native authored policy regression passed; independent
Python authored10, validator24 and diagonal5 tests passed.

Parent reran complete-room validation v2 against the pinned full-room artifacts;
it retained `success:false`, 711/711 supports, 12/12 fallback placements and nine
unknown walls. Output: `$ROOT/parent060058-validation-v2.json` and `.svg`.
This confirms the targeted obstacle repair, not room approval.

Arm64 Release and Debug builds and `git diff --check` passed. Build log:
`/Users/nic.foster/.local/share/opencode/tool-output/tool_12c009bf1001nTG7jqwDS2e12Q`.
Products: `build/release/Snes9x.app`, `build/debug/Snes9x.app`.
Existing deployment/deprecation warnings remain. Running app untouched;
fresh user gameplay/visual confirmation is pending.

From repo root (all outputs temporary, never overwrite sources):

```sh
ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
OUT="$ROOT/obstacle060058-review"
CAP="$HOME/Library/Application Support/Snes9x/Remaster"
ROM=build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
c++ -std=c++17 -O2 remaster/dungeon_obstacle060058_probe.cpp -o "$ROOT/obstacle060058-after"
"$ROOT/obstacle060058-after" "$CAP/frame_room060_058.s9xrmf" "$OUT/after"
c++ -std=c++17 -O2 -DALTTP_TEST_PRIOR_OBSTACLE_CONTACT_PLANES remaster/dungeon_obstacle060058_probe.cpp -o "$ROOT/obstacle060058-before"
"$ROOT/obstacle060058-before" "$CAP/frame_room060_058.s9xrmf" "$OUT/before"
c++ -std=c++17 -O2 remaster/dungeon_authored_obstacle_test.cpp -o "$ROOT/obstacle060058-policy-test"
"$ROOT/obstacle060058-policy-test"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_authored_obstacle_test.cpp -o "$ROOT/obstacle060058-policy-san"
"$ROOT/obstacle060058-policy-san"
python3 -B -m unittest discover -s tools/ALttP -p test_authored_obstacles.py
python3 -B -m unittest discover -s tools/ALttP -p test_alttp_validate_room_heights.py
python3 -B -m unittest discover -s tools/ALttP -p test_diagonal_preview.py
c++ -std=c++17 -O2 tools/ALttP/alttp_runtime_room_heights.cpp -o "$ROOT/obstacle060058-solver"
python3 -B tools/ALttP/alttp_validate_obstacle_live.py "$CAP/frame_room060_058.s9xrmf" --solver "$ROOT/obstacle060058-solver" --rom "$ROM" --fixture-graph build/remaster-geometry/surface-audit-room-local/room-060-doorway-graph.json --output "$OUT"
# Expected exit1, NOT approval. Wrapper invokes alttp_validate_room_heights.py
# with every complete input and live-runtime.bin. Direct gate rerun:
python3 -B tools/ALttP/alttp_validate_room_heights.py --graph "$OUT/live-doorway-graph.json" --snapshot "$OUT/live-solver-input.raw" --attributes "$OUT/live-solver-input.raw.attr" --state "$OUT/live-traversal-state.json" --profile "$OUT/replay-input-profile.toml" --runtime-solution "$OUT/live-local-seed-runtime.json" --height-map "$OUT/live-runtime.bin" --rom "$ROM" --output "$OUT/live-validation-v2.json" --diagnostic-svg "$OUT/live-validation-v2.svg"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_capture_probe.cpp -o "$ROOT/obstacle060058-replay"
"$ROOT/obstacle060058-replay" "$CAP/frame_room061_066.s9xrmf" "$OUT/own066"
"$ROOT/obstacle060058-replay" "$CAP/frame_room061_048.s9xrmf" "$OUT/own048"
python3 -B tools/ALttP/alttp_audit_authored_obstacle_replays.py --captures "$CAP" --probe "$ROOT/obstacle060058-replay" --output "$OUT"
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_probe.cpp -o "$ROOT/obstacle060058-corpus"
python3 -B tools/ALttP/alttp_audit_authored_reference_corpus.py --manifest "$ROOT/room061-057-review/corpus-after.json" --profile "$CAP/frame_room060_058.s9xrmf.profile.toml" --rom "$ROM" --probe "$ROOT/obstacle060058-corpus" --output "$OUT/reference-after.json"
# Repeat preceding two commands with -DALTTP_TEST_PRIOR_OBSTACLE_CONTACT_PLANES,
# executable obstacle060058-corpus-before, output reference-before.json.
python3 -B tools/ALttP/alttp_obstacle060058_summary.py --review "$OUT" --capture "$CAP/frame_room060_058.s9xrmf"
git diff --check
# REQUIRED PARENT BUILD (not run by this worker):
xcodebuild -project macosx/snes9x.xcodeproj -scheme snes9x -configuration Debug -destination "platform=macOS,arch=arm64" CODE_SIGNING_ALLOWED=NO build
```

Existing native preservation tests: compile/run `remaster/dungeon_symmetric_column_test.cpp`,
`dungeon_obstacle_assembly_test.cpp`, `dungeon_pot_placement_test.cpp`,
`dungeon_geometry_cache_test.cpp`. `dungeon_placement_equivalence_test.cpp` requires
arguments `$OUT/live-solver-input.raw`, its `.attr`, own profile, ROM (no-arg exits2).

OUT contains before/after packet+inventory+JSON+actual-effective JSON, own066/048
packets/inventories, reference-before/after.json, own replay corpus with SHA pins,
input-comparison/replay-manifest, full raw/attr/profile/state/graph/runtime/compiled
map, validation-v2 JSON/SVG, summary.json and final-gate.log.
Changed this slice only: frame.h contact ownership; remaster.h lower-role activation
and preservation; authored obstacle native/Python regressions and independent proof;
new obstacle060058 probe/summary/report. Preserve all other concurrent dirty edits.
