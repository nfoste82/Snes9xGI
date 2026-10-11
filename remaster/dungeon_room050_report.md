# Room0x050 / own settled state — blocked by missing structural evidence

Latest separate obstacle/statue repair: see `dungeon_statue_assembly_report.md`.
User8044813840be1f26 is one of two BG1 2x3 statues, not BG2 wallunknowns.
Wholefoot-supported terminal assembly repair now gives all12parts commonbase0,
unchanged authored relief; previous part policy gave headbase8. Parentbuild
section below predates that latest change and needs independent rerun.

## Parent independent verification / built products

Independent compilation and execution passed shared-structure, scrolling
ownership, transition-cache and actual-engine-route regressions (152 completed
frames, 76 each direction). Own capture probe independently reproduced
89,217 matched BG usages, 419 solved / 489 unknown walls and 16 shared witnesses.
Two-crossing diagnostic regression passed, including delayed reverse crossing,
120-frame bounds and orphan-sidecar nonoverwrite.

Fresh parent solver replay and complete-room v2 under
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/parent050-gate/`
confirm validator exit1, reachable support denominators [0,376]. No room approval.
Arm64 Release and Debug builds and `git diff --check` passed; build log:
`/Users/nic.foster/.local/share/opencode/tool-output/tool_1271a3001001K35Cx22FFDjPJA`.
Products are `build/release/Snes9x.app` and `build/debug/Snes9x.app`.
Running app untouched. Fresh user transition confirmation and actual same-state
plane0 capture remain required; counterfactual plane0 is not an observation.

## Follow-up: real shared-shell repair (supersedes initial wall-only diagnosis)

Implemented general independent structural solving for unobserved planes where
CURRENT ROM-owned BG2 shell placements remain solid/interacting. Structural
nodes do not activate movement or create support. A BG2 shell solid on BOTH
planes is one visible physical face regardless of different BG1 lower artwork;
its already solved structural node can anchor signed compatible face contacts
on the other plane. BG1 equality still requires paired identical artwork and
same graphics. New BG2 equality is used for single-observed-plane states only;
existing two-observed-plane cross-BG/stair transport is preserved. Shell contact
attr04 is admitted ONLY with accepted ROM wall ownership; shared equality itself
still requires solid01/02/28/2a, never04 or00. No flat/unobserved support datum.

Own050 now **419solved /489unknown walls**,16sharedsolid witnesses,
**379additional effective runtime wall bases**,0conflicts. Plane supports remain
[0,3564], missing[0,0], reachable graph376/376,4/4fallback,overlap0.
Capture probe asserts exact counts. Existing419 solved structural bases remain
authored signed values; no arrays changed. Full v2 rerun exits1:489unknown walls
and existing4compiledfallback missing/mismatches. Still NOT complete-room approval.

Actual immutable contact/seam component audit gives concrete residual ambiguity:
*476wall nodes in one connected unobserved-plane graph*, span0..40 in relative
accepted seam units, **zero support nodes, zero flat nodes, no contradictory
equalities, no shared-solid anchor**. Example node plane0 cell(3,4), encoded259:
adding datum0 versus datum50 to EVERY base satisfies the SAME accepted seam
constraints. These are ambiguity witnesses, not proposed heights. Nine other
isolated ROM wall nodes plus4 authored/unowned walls remain. Thus no permitted
absolute base for this graph follows from current observed plane1 capture.
Counterfactual exact-input plane0 scope (NOT actual observation) anchors that
476-node graph through1144plane0legal support cells. However that scope loses
different plane1-only anchors: counterfactual total476solved/432unknown is NOT
an additive repair or authority to merge plane arrays.

Smallest missing observation: **same constructed room050/profile state while Link
actually occupies gameplay plane0**, complete own sidecars; compare full maps/
attrs/currentgraphics before any state transport. One witnessed floor base/contact
on that graph, e.g. its real plane0 floor below north wall, removes its one free
datum. A plane0 screenshot without fullinputs is insufficient. Remaining13wall
parts need exact corner/overwrite continuation contacts independently; they do
not justify refusing the379 already proved repairs. `components-component-report`
records all encoded cells, free datums, counterfactual scope and rejected nodes.

Corpus after scoped fix:346contexts/319IDs,339reuses/7fallbacks,complete output
equivalence; **3contexts/3rooms improve by394walls**, unknown256552→256158,
conflicts1288unchanged. Missing[529,0] unchanged. Wider BG1 activation trial
introduced unsupported conflicts; rejected and removed. Final shared capability
is visibleBG2-only and all existing cross-BG tests pass.

### Actual scrolling coordinate verification

Built a separate headless reference adapter against verified engine objects;
did NOT alter/launch user app. Played actual default050 S400,N500 through060
and back.152complete phase2+ transition records,76each direction. For EVERY
256x224position, runtime ownership agrees with actual loader-origin interval;
ordinary room-grid policy had0disagreements in this route. This is real engine
execution, not matching historical Snes9x chronology/visual review.
USA `Dungeon_LoadHeader` uses actual BGscroll±256 rounded512, not always roomID
grid (teleport doors remap). Adapter now retains `$0418` direction and `$062c/e`
destination origin after phase2. Ownership uses signed16bit offset difference;
all four directions, remappedIDs and origins0/512/65024 wrap regression passes.
Actual phase13 resets direction on completion;phase14+entireviewport destination
explicitly handles reset. Legacy state only uses ordinary cardinal grid pairs,
not noncardinal/teleport guessing. Additive sidecars preserve old schema30.

Diagnostic now supports TWO delayed crossings,120frames each (240total), rather
than spending reverse direction in first two-second window. One unique prefix
captures both direction sequences. Existence checks include ALL six sidecars;
an orphan sidecar cannot be overwritten/paired. Diagnostic regression proves
navigation unarmed, first/reverse120bounds, delayed reverse, orphan preservation,
and no thirdcrossing. Successful outputs finalized at ordinary packet boundary.

### Follow-up exact commands / results

Use ROOT/CAP/OUT below. New all-flip shared-structure and ASan/UBSan PASS;
sharedface,unobservedwall,BGownership,crossBGsupport,entry,cache,transition PASS.
06172/06024step source+fullfield oracles PASS. V2own050exit1,060exit1,
061050/051exit2 unchanged diagonalproofgap. `git diff --check` PASS.

```sh
c++ -std=c++17 -O2 remaster/dungeon_unobserved_shared_structure_test.cpp -o "$ROOT/room050-shared"
"$ROOT/room050-shared"
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_unobserved_shared_structure_test.cpp -o "$ROOT/room050-shared-san"
"$ROOT/room050-shared-san"
c++ -std=c++17 -O2 -DALTTP_TEST_WALL_COMPONENT_AUDIT remaster/dungeon_room050_probe.cpp -o "$ROOT/room050-component-probe"
ALTTP_WALL_COMPONENT_AUDIT="$OUT/component-actual-plane0.jsonl" "$ROOT/room050-component-probe" "$CAP/frame_room050.s9xrmf" "$OUT/components"
# Component explorer compile flag ONLY; app builds have no auditI/O/graphcopies.
python3 -B tools/ALttP/alttp_wall_component_report.py "$OUT/components" --audit "$OUT/component-actual-plane0.jsonl" --before-corpus "$OUT/corpus.json" --after-corpus "$OUT/corpus-shared-scoped.json"
python3 -B tools/ALttP/alttp_build_route_adapter.py --engine build/remaster-geometry/room-preview-engine-v21 --output "$OUT"
# Run in engine workdir (tool workdir, not modifying engine files):
ALTTP_ROUTE_ALLOW_ROOM_TRANSITIONS=1 "$OUT/route-renderer" --linked 0x50 "$OUT/route-south.raw" -1 0 0 0 S400,N500
c++ -std=c++17 -O2 remaster/dungeon_engine_scroll_route_test.cpp -o "$ROOT/room050-engine-route"
"$ROOT/room050-engine-route" "$OUT/route-south.raw.route.jsonl"
c++ -std=c++17 -O2 remaster/dungeon_scroll_capture_test.cpp -o "$ROOT/room050-diagnostic"
"$ROOT/room050-diagnostic" "$OUT/diagnostic-tested" "$CAP/frame_room050.s9xrmf.profile.toml"
# Existing prefix deliberately fails uniqueness; use NEWprefix for repeated test.
```

Fresh corpus binary/command as below, output `corpus-shared-scoped.json`.
Latest own solver/gates live-runtime/validation-v2 overwrite REVIEW artifacts only,
source captures immutable. Earlier before-corpus and exploration states retained.
Additional files: unobserved_shared_structure_test.cpp,engine_scroll_route_test.cpp,
scroll_capture_test.cpp; localignored tools wall_anchor_explore.py,
wall_component_report.py,build_route_adapter.py,scroll_route_summary.py;
room_preview_adapter.c additive diagnostics allowing explicitoptin room crossing.
Parent builds after final height/runtime changes. No app interaction by worker.

Scrolling lifecycle repair is reviewable, NOT a completed repair of room050.
Parent owns independent build/verification. Running user app untouched.

## Immutable evidence and two causes

`frame_room050.s9xrmf` means HEX050 = 80, not room061 sequence050.
Its own packet/profile/input/templates/state/terrain sidecars all exist. ROM
66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb,
profile serialized FNV5e1caa974dab0652, profile SHA256
5559eda01e6ef313fda1c25dd2ffb4b0a4e522691069a743ece457495d6a4fff.
Exact SHA pins and state in `room050-review/gate.json` below. Module7/submodule0/
phase0, previous96, Link402/2693 plane1, scroll256/2585, rise50, graphics16384,
complete maps sizes3/3, map bases4096/0. Own template bytes equal verified ROM.
Complete WRAM/PPU8192 words agree; 89217 visible BG usages independently match
hash, flips, canonical pixel and camera. No current profile substitution.

**Wall cause:** metadata activated and ROM templates recognized, but visible
solid-plane0 assemblies are nonblocking occluders on observed plane1. Solver
activates only observed movement plane (no retained in-room stairs). It correctly
does NOT ground visible walls from underpass support; missing general capability
is an independently anchored structural assembly graph on unobserved planes,
including compatible high/low/corner contacts and dispatch/overwrite ownership.
868 unresolved walls: 864 ROM-recognized,4 authored-only; all868 are nonblocking
on observed plane and solid/interacting on other plane. 596 have attr[1,0],
272 attr[4,0]. All have local metadata. No default elevation or neighbor fill.
Trial structural-node activation alone yielded NO improvement and was removed.
New conservative failure-class regression proves all flips preserve underpass0
while wall remains explicitly unknown; it is a blocker test, NOT a wall repair.

Reference ALTPRV1 room050 verifies against its own raw export, and ALL4096 final
BG2 words match own live state. Writer classes are in provenance-comparison.json.
Historical live dispatch sidecar is NOT present; matching final words are not
asserted to reconstruct historical execution. No chronological transition
packets or second-door own capture were supplied. Entry is not a height anchor.

**Scrolling cause / shared runtime repair:** destination solves were prohibited
for entire submodule2, even after completed WRAM room/collision construction.
USA selectable collision builder `$0200` reaches5 after basic/object/door/crystal
overrides. Verified supertile phase8 starts scroll, phases8..15 retain completed
room construction while PPU upload progresses. Adapter now retains builder state
(255 unknown for old evidence); publication admits ONLY2/phase8..15/builder5 with
graphics and existing exact maps/attrs/ROM/profile checks. Old snapshots cannot
attest it. Both completed room maps remain independent during scroll. Changed
inputs still invalidate/re-solve, eight-artwork-state bound unchanged.

Room ownership previously treated EVERY nonzero submodule as scroll and could
select stale previous-room maps during intra-room stairs8/16 or subtile1. Only
supertile2 now uses unwrapped cardinal room boundary. Existing handlers8/16
phase0/1 EXACT current-artwork publication retained. No emitter/hash bypass.
Destination application checks actual displayed tileNumber/palette/flips against
current WRAM placement during incremental upload, in addition to producer hash/
flip checks; rejected words cannot receive bases, fallback, mixed-floor or preview
arrays. Outgoing fallback/mixed-floor now check flips as well as hash. Runtime
destination maps are not applied while publication is unsupported (BG1 too).
No schema30/profile/asset arrays/rise/diagonal/pot/stair shape changes.

These are demonstrated GENERAL mechanisms; absence of chronological packets
means literal user's outgoing scroll cadence is not yet visually proved fixed.

## Coverage, mandatory gates, corpus

Own solver: plane supports [0,3564] solved/no missing; walls40solved/868unknown,
zero structural conflicts. Complete reachable doorway graph from OWN Link seed:
[0,376] support cells,376/376 effective locals,4/4 direct fallback placements,
overlap0. Different denominators are explicit: solver inventories all legal-body
components; graph inventories reachable component under modeled ordinary/door
rules. Special transitions remain unmodeled, not fake connections.
Complete v2 with raw/ALTPAT1/state/OWN profile/runtime/compiledHM3/verifiedROM:
**exit1**,868unknown walls; compiled4missing/4mismatches (fallback cells). Structural
constraint and support missing counts0. All8192 placed BG usages not fully
classified/anchored; no complete-room claim or capture-approval handoff.
Fixture own060047 v2exit1; own061050/051 v2exit2 independent diagonal proof gap
BG2(2,38), unchanged. Exact commands/stdout/stderr in fixture-gates/live*/gate.json.

Corpus346 contexts/319IDs, bothBGs/planes, default reference states+synthetic
animation differential, own050 profile.339equivalence reuses/7fullfallbacks; exact
complete map/diagnostic/packet equality. Unknown walls256552, conflicts1288,
missing supports[529,0]. Specific unobserved-plane wall omission22035 placements,
20791 ROM-recognized. Missing320thID, all dynamic switch/water/door states and
chronological two-room scroll inputs unsupported; this is not game-wide success.

## Commands/results and artifacts

Repo root; ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode,
CAP="$HOME/Library/Application Support/Snes9x/Remaster",
OUT="$ROOT/room050-review" (already exists). No git stage/commit/reset/clean.

```sh
c++ -std=c++17 -O2 remaster/dungeon_room050_probe.cpp -o "$ROOT/room050-classify"
"$ROOT/room050-classify" "$CAP/frame_room050.s9xrmf" "$OUT/after"
c++ -std=c++17 -O2 tools/ALttP/alttp_runtime_room_heights.cpp -o "$ROOT/room050-solver"
"$ROOT/diagonal-venv/bin/python" -B tools/ALttP/alttp_replay_live_height_evidence.py "$CAP/frame_room050.s9xrmf" --solver "$ROOT/room050-solver" --output "$OUT" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
"$ROOT/diagonal-venv/bin/python" -B tools/ALttP/alttp_room050_review.py --capture "$CAP/frame_room050.s9xrmf" --output "$OUT" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
c++ -std=c++17 -O2 remaster/dungeon_geometry_cache_probe.cpp -o "$ROOT/room050-corpus"
python3 -B remaster/dungeon_geometry_cache_corpus.py --corpus "$ROOT/diagonal-sets-priority" --profile "$CAP/frame_room050.s9xrmf.profile.toml" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --probe "$ROOT/room050-corpus" --output "$OUT/corpus.json"
for test in dungeon_scroll_ownership_test dungeon_unobserved_wall_test dungeon_transition_cache_test dungeon_geometry_cache_test dungeon_entry_lifecycle_test dungeon_pot_placement_test dungeon_disconnected_plane_test offscreen_emitters_test; do
 c++ -std=c++17 -O2 "remaster/$test.cpp" -o "$ROOT/room050-$test" && "$ROOT/room050-$test"
done
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/dungeon_scroll_ownership_test.cpp -o "$ROOT/room050-scroll-san"
"$ROOT/room050-scroll-san"
c++ -std=c++17 -O2 remaster/dungeon_variable_diagonal_test.cpp -o "$ROOT/room050-diagonal"
"$ROOT/room050-diagonal" "$CAP/frame_room061_032.s9xrmf"
c++ -std=c++17 -O2 remaster/dungeon_stair_emitter_sequence_test.cpp -o "$ROOT/room050-stair"
"$ROOT/room050-stair" "$CAP/frame_room061_051.s9xrmf" "$OUT/stair061"
"$ROOT/room050-stair" "$CAP/frame_room060_047.s9xrmf" "$OUT/stair060"
"$ROOT/diagonal-venv/bin/python" -B tools/ALttP/alttp_stair_light_review.py --captures "$CAP" --output "$OUT/fixture-gates" --solver "$ROOT/room050-solver" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --fixtures build/remaster-geometry/surface-audit-room-local
git diff --check
```

All above narrow regressions passed; ASan/UBSan clean. Stair06172steps/3assemblies/
20sources,06024steps/planechange/6sources with exact full-field oracle pass.
Separate broad `c++ -std=c++17 -O2 remaster/remaster_test.cpp ...` FAILS line739
legacy-v16 synthetic packet deserialization. Not repaired by weakening schema30;
parent should verify baseline. No arm64 build by worker (parent explicitly owns).
Diagnostic capture I/O arming/bound/nonoverwrite still needs own chronology test.

Artifacts OUT: before/after/classified packet+inventory+JSON; placements inventory,
failure-classes.json, provenance-comparison.json, corpus.json, summary.json,
SHA-pinned gate.json/replay-manifest.json, complete raw/attr/state/graph/runtime,
review-map.bin, validation-v2.json/.svg, fixture-gates and stair regression packets.
Exact geometry and own sources, not frontend/GPU comparison. No fresh gameplay.

Changed: gfx.cpp, frame.h, remaster.h, capture_probe.cpp (additive state reader),
geometry_cache_probe.cpp (corpus failure class counts), transition_cache_test.cpp,
new scroll_ownership_test.cpp, unobserved_wall_test.cpp, room050_probe.cpp,
tools/ALttP/alttp_room050_review.py (local ignored tool), this report.

## Smallest next action

Parent independently checks/builds. Structural wall completion requires reusable
dispatch/assembly support proof on unobserved planes, not turning on movement or
making relative/default wall datums. Two-room visual confirmation needs first
120completed frames of a crossing with immutable per-frame inputs. New opt-in
`S9X_REMASTER_DUNGEON_SCROLL_CAPTURE=/absolute/existing-directory/unique-prefix`
arms on first module7/submodule2, writes120schema30 packets+OWN six sidecars after
normal frame finalization. No overwrites, no I/O when disabled; costly diagnostic,
not FPS measurement. User must voluntarily launch parent-verified build with that
environment later; WORKER DOES NOT stop/restart current app. Minimal request: one
unique prefix per door crossing (both directions), normal fixed profile, plus a
settled pre-crossing capture. Then compare current two-room PPU placement words,
outgoing bases, incoming publication and screenshots at boundary before approval.
