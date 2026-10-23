# Geometry-equivalent artwork cache — reviewable but incomplete

## Urgent room061 repeated dirty-frame repair (supersedes first-stage timing)

Parent verification: production cache regression passes; independent120frame
recorded-state replay reports unchangedmedian8.47/p958.82ms, dirty-cyclemedian
8.31/p958.65ms with76exactstatehits, forcedfullmedian34.60/p9539.50ms.
This run overlapped the application build and is confirmation of operation,
not a controlled A/B speedup measurement. Arm64 Release and Debug builds and
`git diff --check` passed. Matching fresh live FPS/event trace remains pending.

### Cold-penalty resolution / pre-first-stage baseline (latest)

The70→111ms pair above is NOT a reproducible cold regression. Short sequential
ABBA-style runs, identical own041 +5real recorded VRAM samples,30measured+10warmup,
only one mode/process per run, stage clocks compiled ONLY in probes:

|variant|cold median range / p95 range ms|recorded dirty median / p95 range ms|
|---|---|---|
|pre-first-stage bounded ablation|33.6231–33.9351 /34.2804–36.0197|33.5870–33.8705 /33.9748–34.4584|
|first-stage classify ablation|33.5367–33.8479 /34.1284–34.1827|33.3008–34.8058 /33.9341–36.6853|
|final repair (4runs)|33.8912–33.9658 /34.3010–34.6420|8.04337–8.44375 /8.37258–8.82975|

Cold ALLvariants:80classification calls,120solver calls over40frames; final
proof attempts0. Final40state stores cost3.11454–4.53038ms TOTAL (0.078–0.113ms
perframe). Classifications608–620ms total, solves408–419ms; the store cannot
explain40ms/frame penalty. Earlier uncontrolled long-run drift did not replicate
in two short alternating series. No assertion that the host was fully idle:
no other benchmark/build was started by this worker during sequential runs.

Dirty pre/stage1:48classification/72solve calls; final4/6 (2cold states),2proof
attempts total0.174–0.189ms,2stores0.327–0.368ms,22restores0.650–0.852ms.
Unsupported changed metadata never repeatedly compares on recurring states:
exact solved-state lookup handles each revisit. Removed redundant post-classifier
equivalence/metadata scan after exact dependency proof rejects; classified records
still feed full fallback once, no solver preparation duplication. Both complete
BG state inventories match pre/stage1/final and explicit forcedfull for every5state.

PRE ablation scope: `ALTTP_TEST_PRE_ARTWORK_CACHE` disables equivalent-artwork
proof/compare, restores, stores; retains current classifier/solver fixes, exact
evidence input keys and heap-retained classifier outputs. It reproduces pre-stage
per-change work (two classifications/three solves), not an archived historic binary
or old profile. Original cache stage did NOT add classifier/solver calls on this
actual sample workload; its single-state behavior failed to eliminate the existing
rebuild bottleneck. Thus the claim the stage CAUSED user's141.68ms is not established.
This repair eliminates repeated work without weakening heights; live app/frame
regression root cause still requires parent's matched runtime measurement.

Latest artifacts:cache061-alternating-mode{1,2}.json include every command/order/
stage total; `cache061-cold-alternating.json` retains preceding corroborating pair.
Production clocks absent unless ALTTP_TEST_CACHE_STAGES; no new perframe logging.
New ASan/UBSan regression and diffcheck pass after redundant scan removal.

Exact short comparison commands (same OUT/CAP as below):
```sh
c++ -std=c++17 -O2 -DALTTP_TEST_CACHE_STAGES -DALTTP_TEST_PRE_ARTWORK_CACHE remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-pre061"
c++ -std=c++17 -O2 -DALTTP_TEST_CACHE_STAGES -DALTTP_TEST_CLASSIFY_ARTWORK_REUSE remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-stage1061"
c++ -std=c++17 -O2 -DALTTP_TEST_CACHE_STAGES remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-final061"
python3 -B remaster/tests/dungeon_geometry_cache_alternating.py --captures "$CAP" --output "$OUT" --mode 1
python3 -B remaster/tests/dungeon_geometry_cache_alternating.py --captures "$CAP" --output "$OUT" --mode 2
```

Root cause reproduced in production EndFrame replay: complete room061 maps and
attributes are byte-identical across recorded inputs, while referenced VRAM
changes. With the OWN latest complete061_041 profile, sampled identities do NOT
always have equivalent geometry metadata/classification. The first-stage cache
retained only ONE solve; repeated animation identities fell back through both-BG
classification and up to three full solves. Earlier synthetic animation metadata
extension concealed this failure class. The miss path does NOT classify a BG
twice: retained classifier results already feed all fallback solves. The new
template memcmp is12,876 bytes, not the80–150ms source. No live sample was attached
here (pgrep found no Snes9x process); the screenshot's exact141.68ms event remains
unattributed until parent build/runtime comparison. Do NOT claim causality proved
for that exact screenshot or restored60FPS.

Shared conservative repair:
1. Exact dependency-proved equivalent artwork refresh before classification:
   complete words/attributes/stairs/templates/profile/bases/planes/support pins
   unchanged, ALL changed identities retain identical activated geometry fields
   and every group membership. The classifier reads decoded content via identities
   only, not palette-index values; ROM writers read unchanged words. Therefore
   retain COMPLETE classification records unchanged, refresh hashes only after
   the whole proof succeeds. Rejection modifies nothing and uses full fallback.
2. Bounded8-state heap cache of previously solved, genuinely DIFFERENT artwork
   states. Input hash is ONLY lookup: verify every referenced byte across bothBGs
   before restoring complete maps, diagnostics, classifications and producers.
   All non-artwork/support/entry/profile/plane/stair changes invalidate states;
   no entry-attested solve is stored. Unknown/conflict states remain distinct.
   Exact-state publication advances epoch (geometry may differ); evidence and
   solve keys match the restored state's exact current inputs. No actor gating.
   Cold/unseen states still classify/solve fully. Bounded memory ~several MiB,
   not a globally growing room/profile map. No renderer or schema changes.

Matched CPU replay pin: frame_room061_041.s9xrmf OWN profile serialized8540631947d0fa70,
profile SHA2567fe5abdbec32bec58c288ced1a61884af2cf40fe11f8bf92a8aa3b2af9947465;
five REAL recorded complete VRAM samples061_018,025,026,032,041 (all complete maps/
attributes byte-identical).120 measuredframes+10warmup/mode, sequential-O2 builds,
timer ONLY EndFrame, no renderer/GPU/startup/setup included. Replay cycles recorded
samples, NOT an observed chronological gameplay trace; RGB/actors remain replay
stock. Source states may predate latest artwork: current own041 profile is fixed
for this workload, no synthetic animation metadata was added.

|production mode|first-stage classify ablation median/p95 ms|repair median/p95 ms|
|---|---|---|
|unchanged own041|17.2326 /28.8738|16.3057 /22.9140|
|recorded referenced-VRAM cycles|59.6023 /91.7592|20.5915 /33.8652|
|explicit full rebuild EACHframe|70.7108 /103.224|111.430 /148.856|

Dirty mode:78 state publications in each build;76 repaired exact-state hits,
0 equivalence shortcuts (metadata really differs), no repeated solver on those
hits. Every one of5 complete two-BG state inventories equal old classify path
AND repaired forced-full; no missing/conflict/ownership changes. Full-rebuild
timing regressed/noisy in this sequential run: no claim cold-miss improvement.
Ordinary cached application still16ms here, so further whole-frame performance
work/actual release measurements needed. Results: cache061-performance.json and
bench061-{slow,fast}-mode{1,2}-state*.inventory.

Final regression evidence:346contexts/319rooms complete differential still passes,
339 equivalent reuses/7 fallbacks, source profile pin unchanged; equivalent-artwork
median0.282625ms vs full26.5375ms (empty8x8 frame corpus, NOT live frame cost).
All available captures re-enumerated94:31complete,28exact-equivalent,3same rejected
evidence,63packet-only (new061_043..046 have no complete sidecars). Actual latest
complete state remains041; newest046 cannot establish room-state/animation input.
ASan/UBSan production cache regression including DIFFERENT-state revisit passes;
entry/transition/profile/pot/high-corner and small-stack tests pass; diffcheck pass.
062016 productionpacket unchanged, mandatory complete gate exit1/report identical
(cache-final-gate.json), no approval or fresh interactive comparison.

Exact benchmark reproduction (repository root; existing OUT directory):
```sh
OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
CAP="$HOME/Library/Application Support/Snes9x/Remaster"
c++ -std=c++17 -O2 -DALTTP_TEST_CLASSIFY_ARTWORK_REUSE remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-slow061"
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-fast061"
export ALTTP_CACHE_BENCHMARK=120
export ALTTP_CACHE_GRAPHICS_SAMPLES="$CAP/frame_room061_018.s9xrmf;$CAP/frame_room061_025.s9xrmf;$CAP/frame_room061_026.s9xrmf;$CAP/frame_room061_032.s9xrmf;$CAP/frame_room061_041.s9xrmf"
"$OUT/cache-slow061" "$CAP/frame_room061_041.s9xrmf" "$OUT/bench061-slow"
"$OUT/cache-fast061" "$CAP/frame_room061_041.s9xrmf" "$OUT/bench061-fast"
unset ALTTP_CACHE_BENCHMARK ALTTP_CACHE_GRAPHICS_SAMPLES
```
Parent owns builds/independent runtime checks. No app termination, launch, GUI
benchmark or renderer edits. Next: complete chronological animation input trace
from the actual regressing state/profile and CPU stage trace; retain full unknowns.

## Implementation boundary

First-stage implementation below is historical; urgent repair above supersedes
its classify-every-artwork-change cost. Only referenced VRAM artwork changes are eligible. Complete BG words (including
priority/palette/flips), completed attributes, stairs, room/ROM, graphics bases,
rise, profile geometry and templates must be unchanged. Both BGs are fully
reclassified, then EVERY Placement field except tileHash is compared explicitly.
Changed identities additionally require identical activated role/semantic,
height, normals, occlusion, door mask and group metadata. Missing metadata is not
silently equated to authored defaults. All dependencies/ownership/diagonal and
observer fields survive. Ambiguous/unknown/conflicting outputs are retained, not
filled. Both producer hash arrays refresh before application/capture/offscreen
emission. Exact evidence keys advance independently of the last solve geometry
key/epoch. No renderer, schema, artwork or user-array edits.

Entry attestations, pending transitions, supplied support relations, changed
support/topology/words/profile/planes take the existing full fallback. Complete
template bytes now participate in change detection and evidence identity (the
old key hashed only three ranges despite classification consuming other ranges).
Actor and camera fields remain per-frame application inputs, not solve inputs.
Heap-backed retained classifications; no component-local solving introduced.

## Evidence

OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode

* New production regression passes optimized and Debug: equivalent animation,
  changed-relief animation, refreshed identity/emission, exact complete two-BG
  maps/diagnostics and serialized packets against forced full solve, blocker
  pickup attribute change, flips/priority, planes/stairs/templates/profile/rooms,
  camera/actor non-invalidation and exhaustive Placement comparison contract.
* Corpus: 346 graphics/entrance contexts, 319 room IDs, BOTH BGs/planes,
  default states in diagonal-sets-priority. 339 geometry reuses, 7 full fallbacks;
  every complete map field/diagnostic and production packet equals forced solve.
  Source ROM SHA256 66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb;
  source profile SHA256 769f20ff708168e900a093d05cf7deeaa8176dead1ae7372734358d765ced137.
  Exact per-context records/timings/remaining unknowns: geometry-cache-corpus.json.
  Unchanged residual totals: missing movement [529,0], unknown walls 256552,
  conflicts 1288. Room060: missing [0,0], unknown walls9, conflicts3; room061:
  missing [0,0], unknown walls2, conflicts0. Both reused and matched full solves.
  This probe preserves zero transparency and synthetically cycles nonzero palette
  indices, extending canonical metadata to synthetic variants only inside the
  probe. NOT captured gameplay animation evidence. One of 320 room IDs and
  additional switch/water/chest/door states are not represented by this corpus.
* Offline -O2 production EndFrame changed-artwork median 16.4229 ms versus forced
  full solve 20.8226 ms across reused contexts (~21% less, ~4.40 ms). One sample per
  context, not steady-state p95/live FPS proof. Classification remains the cost.
* Existing semantic-cache, placement-reuse, terrain lifecycle, pots, high corners,
  terminal corners, entry lifecycle, transition cache, cross-BG wall support,
  doorframe placement/runtime, door band and door foot regressions pass (14).
* Release small-stack test passes on 512/544 KiB. git diff --check passes.
* Exact production room062/016 EndFrame packet equals preceding perf-production016
  byte-for-byte. Existing producer assertions pass: upper/lower persistent pot
  support, north/south door masks, active plane and camera behavior.
* Mandatory validator rerun: cache-gate.json records exact command; complete
  cache-validation-v2.json equals prior validation-v2.json. Exit 1 unchanged:
  1248/1248 reachable supports, 32/32 frames, 6/6 persistent supports, 673 solved /
  7 unknown walls, 1 conflict, 2 compiled missing / 67 mismatches. NOT room approval.
* Follow-up: all91 available room060/061 packets enumerated;31 have all own-input
  sidecars,60 lack complete inputs. Of31,28 pass strict packet/maps/camera identity
  and exact enabled-versus-forced-full production differential (18 room060,
  10 room061). Both COMPLETE BG map inventories include every field, ownership,
  unknown/conflict and diagnostic; all serialized production packet bytes equal
  including main/sub/SCN1, artwork, normals/emission, actors and schema30 fields.
  2,855,289 BG pixel usages checked against own complete maps/VRAM/flips/camera.
  No current-profile substitution.16 distinct serialized profile pins; exact SHA256
  for each original packet/profile/input/templates/state and optional terrain in
  geometry-cache-captures/manifest.json.13 matched older packets lack continuity
  rom_identity/grounded attestations: preserve their legacy no-continuity behavior,
  do not invent entry evidence. No accepted capture has a nonempty entry anchor.
* Three complete-sidecar packets rejected IDENTICALLY before solving in both
  builds:060_011 main pixel41568 BG2 cell(44,54), word3520, captured hash
  9645985611384305761 versus input8343671643936895600;061_006 and061_007 main
  pixel10594 BG2 cell(28,32), word11714, hashes14856962653691622703 versus
  1322130623150636892 (reversed in007). These are mismatched own packet/VRAM
  evidence, NOT cache regressions. Never reuse their geometry or call them passed.
* The reported variable-diagonal failure was an INCORRECT TEST INVOCATION:
  existing test hardcodes room061, Link(649,3347), plane0 and direct BG2 solve,
  but I supplied room060_032. Cache-enabled and cache-ablated builds both exit1
  with identical `no new room conflicts`; exact assertion-state inventories equal:
  conflicts1, unknown walls76, geometry_conflicts=[], issue_placements=[].
  Correct room061_032 OWN input (profile pin818aad8e7af1c72f) passes the entire
  existing regression in BOTH builds; assertion-state conflicts0/unknown walls0.
  This direct solver path never accesses the room cache. Separate correctly pinned
  PRODUCTION room060_032 differential (pin312c06256d6ecf0e) matches exactly:
  BG1 conflicts4/unknown walls0, BG2 conflicts3/unknown walls55, missing[0,0]
  and geometry_conflicts=[] on both. No introduced conflict or hidden repair.
  Commands/exits in cache-variable-diagonal-diagnosis.json; full assertion
  inventories and exact conflict reports in diagonal-{060,061}-{enabled,full}.*.
* New production geometry-cache test passes AddressSanitizer + UndefinedBehaviorSanitizer
  (-O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer), no diagnostics.

## Reproduce (repository root)

```sh
OUT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_test.cpp -o "$OUT/geometry-cache-test"
"$OUT/geometry-cache-test"
c++ -std=c++17 -O0 -g remaster/tests/dungeon_geometry_cache_test.cpp -o "$OUT/cache-debug-test"
"$OUT/cache-debug-test"
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_probe.cpp -o "$OUT/geometry-cache-probe"
python3 -B remaster/tests/dungeon_geometry_cache_corpus.py \
 --corpus "$OUT/diagonal-sets-priority" --profile remaster/profiles/ALttP/alttp-profile.toml \
 --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
 --probe "$OUT/geometry-cache-probe" --output "$OUT/geometry-cache-corpus.json"
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_room062_endframe_probe.cpp -o "$OUT/cache-endframe"
"$OUT/cache-endframe" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$OUT/cache-production016"
cmp "$OUT/cache-production016.s9xrmf" "$OUT/door-pot-lifecycle/perf-production016.s9xrmf"
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-capture-enabled"
c++ -std=c++17 -O2 -DALTTP_TEST_DISABLE_GEOMETRY_REUSE remaster/tests/dungeon_geometry_cache_capture_probe.cpp -o "$OUT/cache-capture-full"
# Existing output directory geometry-cache-captures is required.
python3 -B remaster/tests/dungeon_geometry_cache_captures.py \
 --captures "$HOME/Library/Application Support/Snes9x/Remaster" \
 --enabled "$OUT/cache-capture-enabled" --full "$OUT/cache-capture-full" \
 --output "$OUT/geometry-cache-captures"
c++ -std=c++17 -O2 remaster/tests/dungeon_variable_diagonal_test.cpp -o "$OUT/variable-cache-enabled"
c++ -std=c++17 -O2 -DALTTP_TEST_DISABLE_GEOMETRY_REUSE remaster/tests/dungeon_variable_diagonal_test.cpp -o "$OUT/variable-cache-full"
"$OUT/variable-cache-enabled" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_032.s9xrmf"
"$OUT/variable-cache-full" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_032.s9xrmf"
# The diagnostic-only direct-solve diagonal dumper was retired during cleanup.
# Keep the variable-diagonal assertions above and the capture differential harness;
# cache ablation cannot affect the retired dumper's direct-solve path.
c++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer remaster/tests/dungeon_geometry_cache_test.cpp -o "$OUT/cache-sanitize-test"
"$OUT/cache-sanitize-test"
python3 -B -c 'import json,subprocess; from pathlib import Path; p=Path("/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/cache-gate.json"); raise SystemExit(subprocess.run(json.loads(p.read_text())["command"]).returncode)'
git diff --check
```

## Residual / next stages

Parent owns arm64 builds and independent validation; not performed here. No live
app/capture intervention, no renderer edits. Complete own-input060/061 differential
is finished with explicit31/28/3 sidecar/accepted/rejected denominator, not a
gameplay visual approval.60 packet-only inputs and3 mismatched packets still lack
admissible geometry evidence. No real changed-artwork complete animation trace
was supplied; capture differential tests unchanged live inputs, while the separate
synthetic corpus and production animation test exercise actual equivalence reuse.
Next: matched complete live animation trace/profile timing. Then cache
decoding/classification by exact referenced identity
and assembly dependency (with overwrite/ownership rejection), rather than skipping
classification or globally ignoring word bits. Topology/support-local solving is
separate future work. Retained classifications cost roughly two Placement grids;
unchanged frames add a complete template memcmp, not profile scans/classification.
