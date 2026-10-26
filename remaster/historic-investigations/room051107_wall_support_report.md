# Room051 own107: west-facing curtain wall bands

Room `0x051`, own `frame_room051_107.s9xrmf`, Module7/submodule0:
**blocked by missing evidence** for an automatic geometry repair. Exact placement
support and physical scene application already work. The two uniquely authored
west-facing assets have intrinsic0..7 arrays while their actual east-facing
counterparts and parallel members contain4..11/12..19. No runtime/profile height
change is made: assigning +4/+12 from appearance/common footprint alone would
invent continuous-surface ownership and override authored geometry semantics.
The shared independent audit now exposes these datum disagreements, explicitly
non-gating and never used as solver inputs. This is diagnostic coverage, NOT a
runtime height repair or complete-room approval.

## Inputs and exact reproduction

Input directory: `~/Library/Application Support/Snes9x/Remaster/`, own107 packet
and its **own** complete companions, never the current canonical profile.

* ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
* Packet SHA256 `630c9db71efcf3db4dc98fb32419dacf8b7728ad91c4ef1be344d9f7eaa9c2e9`.
* Own profile SHA256 `025e261c0ece848ea9fb800d7d678b0bb7b23b6b9d753248be410230da62743d`,
  serialized FNV `a0a42bc961c35a5e`. Separate generated gate copy:
  `build/room051107/gate/replay-input-profile.toml`.
* Complete-input SHA256 `33ff7a484193d9a948001adf9edb451844fa73c059e09105301781ed1e351174`.
* All seven original file pins: `build/room051107/pins.json`.
* Previous room061; Link `(760,2767)` plane0; scroll `(640,2659)`;
  graphics bases16384 on both BGs; configured rise50; collision mode0;
  complete state pin in `gate/input-comparison.json` and `gate/gate.json`.

Complete WRAM/PPU map mismatch count0. Production replay independently matches
**168842** main/sub/physical-scene BG observations against exact final room words,
hashes, flips and canonical tile-pixel mapping. No stale capture or orientation
substitution. All authored height/normal sources remain unchanged, including
signed sources elsewhere in the packet. No macOS/profile edits or commits.

## Exact placement table: actual heights unchanged before → after

`A = v1:4bpp:a2af9baf8d303df7`, `F = v1:4bpp:f91826cad69954c9`.
Both are intrinsic **Obstacle**, not Wall, and belong to complete explicit
`West_facing_curtains` group13. No context or placement-local height override.
Each cell has64 physical-scene pixels; total768. All H flips are0: the user's
unique west-authored identity observation is correct. V flips transform sampling
without altering the X gradient.

|BG|X,Y|asset|final word|H/V|assembly|scene base before→after|local / effective span|
|--|--|--|--|--|--:|--|--|
|2|45,17|A|1167|0/0|1069|24→24|0..7 /24..31|
|2|46,17|F|1147|0/0|1069|24→24|0..7 /24..31|
|2|45,18|A|9167|0/1|1197|24→24|0..7 /24..31|
|2|46,18|F|9147|0/1|1197|24→24|0..7 /24..31|
|2|45,23|A|1167|0/0|1453|24→24|0..7 /24..31|
|2|46,23|F|1147|0/0|1453|24→24|0..7 /24..31|
|2|45,24|A|9167|0/1|1581|24→24|0..7 /24..31|
|2|46,24|F|9147|0/1|1581|24→24|0..7 /24..31|
|2|45,29|A|1167|0/0|1837|24→24|0..7 /24..31|
|2|46,29|F|1147|0/0|1837|24→24|0..7 /24..31|
|2|45,30|A|9167|0/1|1965|24→24|0..7 /24..31|
|2|46,30|F|9147|0/1|1965|24→24|0..7 /24..31|

Intrinsic64 arrays are exactly `height[y*8+x]=x`. Authored normals have westward
X components; no mistaken east-canonical H normalization occurs. Existing
`normalIgnoreTileTransform`/context/mask data is not rewritten. Stock main-screen
instances are0 before/after, while the independent physical scene is24 before/
after. Production lighting fields consume24+local for all768 target pixels and
retain authored normals. Treating main's untouched stock references as lighting
heights would misdiagnose this case.

### Support and hidden floor evidence

Each complete group has eight members and two explicit col0 contacts. All six
target-containing groups prove both contacts on plane0, movement component0,
base24, without conflict. Supporting direction is west2; target members are NOT
contacts. Col0 contact atX44 meets outside floor atX43. Solid body attr is0x6f
on plane0,0 on plane1. Distinct BG1 zero-local Floor metadata under the complete
BG2 footprint plus completed collision semantics corroborates obstacle-plane
ownership. This already implemented shared rule selects plane0 independently
of Link plane, priority or camera visibility.

Under every target cell, BG2 movement support is plane0 unknown/plane1 **0**;
the separate BG2 structural obstacle datum is **24**. Both survive. BG1 final
hashes at rows17/23/29 are `9c8a1e10362481e4` atX45 and
`4d5247551edca28e` atX46; rows18/24/30 use `f105c9a9889035dc`/
`d2ef88b0a91300d6`. Their own BG1 bases must not be pasted into BG2 member relief.
The apparent lower-plane-under-obstacle hypothesis is real scene structure,
but NOT the missing-height cause here: upper support already reaches all targets.

## Actual east counterparts, not guessed hash similarities

For each targetY above, exact west-side BG2 `(18,Y)` is
`v1:4bpp:ad3b150df3ea5b3b`, H0, corresponding V, authored local `11-x`,
range4..11; `(17,Y)` is `v1:4bpp:221db4615adb55fd`, H0, local `19-x`,
range12..19. Both use complete `East_facing_curtains` assembly/base24 and Z0,
giving effective28..35 and36..43. Their detailed normals have positiveX.

Within each exact west target assembly, the adjacent tangent row already contains
H-flipped `92bb4bf4c7026dd7` and `fad8903afb5ad095`, whose displayed samples are
4+x and12+x. The target row hasx for BOTH bands. Thus same support24 composes to
different effective sections; there is no missing whole-assembly24 offset.
Would-be matching deltas are +4 and+12, not one floor rise and not an H flip.
All these exact sources, full normal/height arrays, BG1 underlays and word/attribute
pins are in `target-placement-evidence.json`; full both-BG8192-cell inventory in
`wall-before.walls.jsonl` and `final.walls.jsonl`; all buffer pixel sources in
`before.sources.jsonl`.

## Failing stage and smallest legitimate repair capability

Metadata exists and is active → exact complete authored ownership succeeds →
both explicit movement contacts solve24 on the unique plane → local sources are
preserved → actual scene/lighting composes24+x. The discrepancy is in **intrinsic
section datum / absent explicit continuous-surface contact semantics**, not
placement floor solving, transform normalization, diagnostic projection or scene
suppression. Static obstacle grouping deliberately supports arbitrary furniture
and relief; common group membership never promises every neighboring edge is one
continuous wall. Existing nonzero member Z is ignored for members with authored
height, by the established contract; silently changing this would also alter
signed authored groups elsewhere and cannot express +4 in tile units.

Safe next options require coordination/approval:

1. Confirm the intended canonical bands for these two uniquely authored assets;
   if intrinsic arrays were accidentally zero-based, explicitly approved canonical
   reusable metadata correction4+x/12+x is the smallest repair. **No such edit is
   made here.**
2. If0..7 is intentional local relief, add explicit reusable per-member continuous
   surface/contact/datum evidence (pixel-unit, separate from floor support). Then
   solve signed section relationships, reject disagreeing contacts/overwrites,
   independently validate and publish contextual offsets without modifying arrays.

The new `parallel_section_datums` audit exposes exact transformed affine tangent
disagreements on same-owned assemblies, but never assumes those contacts are
physical seams, alters solver output, or counts proposals as repaired geometry.
It also exposes legitimate-looking throne discontinuities in the corpus—direct
evidence that automatic common-footprint seam flattening is not justified.

## Tests and available corpus

New fixture-free `dungeon_obstacle_intrinsic_datum_test.cpp` passes64 cases under
ASan/UBSan: both BGs, four H/V transforms, independent member-facing flips,
signed local arrays, exact ownership overwritten, competing complete claims,
ambiguous plane rejection and explicit distinct hidden-floor ownership. It asserts
that neighboring relief never manufactures a member datum.
New own107 probe checks12/768 complete members/scene uses, lighting fields,
canonical sources and all packet height/normal arrays. Repeated unchanged-profile
publication/resume three times produces exact packet and both-BG-map invariance.
Python new diagnostic2tests cover transforms/signed arrays/immutable input,
different owners, generated sources, nonaffine/flat/opposite profiles; existing
authored-obstacle16tests and complete-room-validator26tests pass.

Current own-input ASan/UBSan regression replays all pass, each with three repeated
unchanged-profile publication/resume cycles:104follower128records0→24/main-scene264;
099north398doorway pixels;098south512;092stair467+jamb256 with transformed surviving
column/overwrite negatives;061010896doorway pixels/169178identity observations;
071completeBG1stair16/288solvedwalls and overwrittenBG2rejection. Logs
`build/room051107/regression*.log`. Before/final107serialized packets and full
8192-cell placement inventories are byte-identical. Original seven source pins
are rechecked unchanged. `git diff --check` passes.

Own-input corpus union: **133 unique complete input/profile/state combinations**,
266 BG solves, **1089536 placed BG cells**, seven rooms:
002(4),011(1),012(6),022(1),041(2),051(93),061(26). All complete-input audits accepted;
74 candidate Application Support packet paths lack at least one companion and
are explicitly listed in `corpus-inputs.json`, not silently counted. This is not
320 rooms, all graphics contexts/states, every viewport or gameplay validation.

All258 prior metric rows from own104 corpus are exactly unchanged by same complete
input pins. Across expanded133 corpus: required support plane0/1 sums
280608/718096, missing0/0, structural conflicts626, unknown walls1494,
unknown doorway2460, visible doorway conflicts0. No runtime heights change, so
newly solved geometry0 and new solver missing/conflicts0. Existing authored member
inventory21212:1344 unresolved including180 unmatched; support-conflict members0.
Targets: **492 placed members across41 own-inputs**, every one already base24 with
intrinsic0..7. Diagnostic628 parallel-section candidates across58inputs:
492 target differences (absolute4/12),136 throne differences (8/16); these are
explicit **unproved** seam proposals, not newly solved or newly conflicting
runtime geometry. Full CSV/pins and candidate JSON retained under `build/room051107/`.

## Mandatory complete-room v2: exits1, not approved

`gate/gate.json` records actual complete validator command/exit1. Inputs include
full matching ALTPPD1,ALTPAT1, pinned state/profile, complete doorway graph, runtime
JSON and compiled **ALTPHM3** map. Reference directed transitions transfer only
on exact collision digest equality (true). Reachable graph denominator886/0:
886/886 support and26/26fallbacks pass. Runtime candidate962/3720 missing0/0;
BG2 walls564solved/0unknown; structural conflicts4. Doorway20/20 and independent
stair32plane-members pass. Existing blockers remain:

* compiled map missing two visible24 and two plane0 entries at `(30,39),(33,39)`;
* sixteen unexpected BG1 runtime obstacle claims over the stair footprint,
  recorded independently, not confused with target obstacle ownership;
* directional continuity audit1704wall-plane placements/6816edges has992missing
  and56conflict observations. These stricter placed-geometry findings are retained.

Independent authored-group counts240/240 and68/68 contacts do NOT certify correct
canonical member datums. No reachable-gate/hash-array-presence waiver is used.
No fresh interactive Snes9x Height/Normals comparison was performed. Generated
`final.s9xrmf` is unchanged-geometry evidence, NOT a reviewed repair or permission
to capture an approved room. Parent owns aggregate Metal/app builds.

## Reproduction / stable handoff

Parent independently reran the exact107 production probe under ASan/UBSan with
unchanged-profile resume checks:168842identity observations pass, target bases
and intrinsic sources remain unchanged. Parent also passes all64fixture-free
intrinsic-datum cases under ASan/UBSan and both new Python diagnostic tests.
Evidence: `build/room051107-parent/capture-sanitized.log`. No runtime repair,
profile correction, complete-room approval or fresh visual acceptance is claimed.

```sh
mkdir -p build/room051107/gate
c++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  remaster/tests/room051107_wall_capture_probe.cpp -o build/room051107/capture-san
ALTTP_PROFILE_RESUME_REGRESSION=1 build/room051107/capture-san \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_107.s9xrmf" \
  build/room051107/final
c++ -std=c++17 -O1 -g -fsanitize=address,undefined \
  remaster/tests/dungeon_obstacle_intrinsic_datum_test.cpp -o build/room051107/intrinsic-san
build/room051107/intrinsic-san
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_obstacle_member_datums.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_authored_obstacles.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_validate_room_heights.py'
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o build/room051107/solver
python3 -B remaster/tools/ALttP/alttp_replay_live_height_evidence.py \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_107.s9xrmf" \
  --solver build/room051107/solver --output build/room051107/gate \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B remaster/tools/ALttP/alttp_capture_complete_gate.py \
  --capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_107.s9xrmf" \
  --output build/room051107/gate \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
  --fixture build/room051_098_103/gate-103/live-graph.json
```

The gate wrapper records validator exit1 in JSON; it does not necessarily return
that status itself. Replay full `gate.json.command` to get the direct gate status.
No production runtime source or solver result has changed. Minimal next action:
coordinate the intended asset datum versus explicit continuous-section semantics
before implementing a runtime inference or touching canonical authored arrays.
