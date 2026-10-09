# Room062 / immutable016 — reviewable but incomplete; CPU regression repaired

No user app terminated/started, no interactive benchmark, no renderer/macfrontend
edits or quality reduction. Parent owns GPU investigation/build/independent tests.
Screenshot20FPS/GPU21.11ms direct21.02ms is NOT explained completely by this CPU
repair: direct transport alone exceeds16.67ms. Dropped0 is not proof of60Hz.

## Measured defects and implementation

Parent independently reproduced the CPU improvement over180frames, running the
reference/final pair sequentially before builds: cachedmedian8.33→6.58ms,
movement7.03→6.40ms,changingactor8.19→6.46ms; zero solve epochs in allthree.
Forced-rebuildmedian56.83→41.61ms (singlepair, not livep95 evidence). Arm64 Release
and Debug builds and `git diff --check` passed.

Packet-local GPU inspection with `tools/inspect_frame_lights.cpp`: newest018
contains11active physicalspheres/448adaptive sphere samples, versus016's4/32
and017's2/128.018 includes eight missing-pixel spheres from two unlinked depth
emitter identities plus three torch fragments. Screenshot12spheres/896maxsamples
is NOT that exact packet/state, and these differing cameras/animation phases are
not a same-state before/after build comparison. Renderer direct batching includes
sphere samples in addition to prepared pixel sources; the displayed24direct
samples alone therefore understates total transport work. No GPU/shader change
was made in this repair; the21ms direct-pass regression remains unisolated and
needs matched live state/build timing. Offscreen light transport can become newly
eligible when height repair proves a previously unknown base; causation for the
user's screenshot has not been established.

* Door-region application scanned the whole attachment vector in `offsetAt`
  and `floorAt` for essentially every BG pixel, twice per buffer and both BGs.
  New per-application4096cell head index + ordered linked attachment indices
  preserves EVERY plane/mask/ambiguous claim; lookup is O(1)+claims for thatcell.
  Heap-backed; no large emulation-stack arrays, no cached stale ownership.
* Potential support enumerated up to242536origin positions peractiveplane on
  each room solve. It now enumerates only exact body-origin intervals intersecting
  removed cells, deduplicated at pixel phase: X[cellX−15..cellX+7],
  Y[cellY−23..cellY−1], clipped to legalbounds. Predicate/ownership/denominators
  unchanged. This reduces rebuild spikes, not ordinary cached-frame work.
* Pot template probe now stops immediately after exact rejection, rather than
  reading allfourmembers at every room position/variant. Accepted sequence is
  identical. No solver output/authoring/identity defaults changed.

Actor matching bounded16actors×128OAM slots is perframe but not the dominant
measured cost. Synthetic changing carried fields did NOT invalidate the solve.
Camera/Link movement did NOT invalidate either. Geometry-profile hash and room
map hashing run only after existing dirty/input changes; no weakened invalidation.
Ordinary cached EndFrame still performs snapshot comparisons/application and
metadata/instance work. Forced room rebuild remains expensive; genuine changed
BG graphics/attributes cannot simply be excluded. Live rebuild frequency requires
parent-coordinated runtime sampling, not an invented animation trace.

## Controlled offline production benchmark

Exact016 OWN profile, USA ROM/camera/completepins from lifecycle report.
Production `S9xRemasterEndFrame`, stock observations reinjected eachframe;
timer surrounds EndFrame only (not input setup/serialization/GPU). Optimized C++
`-O2`,10warmup then180measuredframes (syntheticSCN1:120). Modes separately reset
context. No processes intentionally run in parallel in the reported pair.
Bounded reference build defines `ALTTP_TEST_LINEAR_FRAME_REGIONS` and
`ALTTP_TEST_EXHAUSTIVE_PERSISTENT_BODIES`; preserves correctness and restores
old costly searches. Both versions use the same final classification rules.

|mode|reference median/p95 ms|final median/p95 ms|solve epochs|
|---|---|---|---|
|cached016|11.096 /13.803|7.472 /7.993|0/0|
|camera+Link syntheticmovement|9.284 /10.806|7.596 /8.296|0/0|
|changing carried actor fields|9.613 /10.517|7.591 /8.014|0/0|
|explicit forced rebuild EVERYframe|63.136 /69.710|55.261 /93.616|190/190|

Earlier uncontended stage runs: initial preoptimizationcached9.391ms, indexed
6.416ms; indexed forcedmiss50.880ms, bounded-origins42.118ms. Later system noise
is substantial: forcedmiss finalp95regressed in reported pair; do NOT claim a
stablep95speedup. Initial parallel experiment was discarded as a controlledpair.
SyntheticSCN1 (copy ofmain, NOT real capturedscene) cachedreference13.077/16.544
versusfinal12.742/13.546ms; actorreference14.281/19.407 versus11.946/15.797ms.
These are workload microbenchmarks, not proof user's20FPS is solved or fresh
animation/gameplay evidence. No real animated complete-state trace was available.

Output counts identical: cached/actor1763instances, movement2298at180frames,
4frameLights,0capturedSCN1. SyntheticSCN157344pixels retains samecounts. Actor mode
changes value-ownedfields but has no fabricated actual potOAMproducer. Screenshot
428emitters/24samples is GPU prepared light workload, NOT these4frameLightrecords;
do not equate denominators. Exact fullpacket equality proves this optimization
adds NO fragmentation or emitter metadata, but does not reproduce screenshot's
current lighting/profile/animation state. Parent owns that comparison.

## Correctness and gates

* Exact COMPLETE runtime solver JSON equal before/after for346contexts/319rooms:
  ALLfields, not only solvedbases.0changedplacements. Counts88000solvedwall,
  256552unknown,1288conflicts,missingmovement[529,0];persistent4380required,
  3010solved,1370missing unchanged. Evidence/denominators remainexplicit.
* Production016 `.s9xrmf` byte-for-byte equal against preceding final correctness
  packet AND boundedreferencepacket. Heights/instances/locals/asset/emission/light
  fields/palette/schema unchanged. Complete production path north/south/cache/
  plane/camera tests pass. No debug-only change.
* Doorframe allaxes/HV/splitBG/ambiguousplanes masks, terrainlifecycle, persistent
  dual-owner rejection tests PASS; `git diff --check`PASS.
* Mandatory fullv2 rerun against matchingcomplete016 graph/inputs/profile/runtime
  and NEWproductionmap retains **exit1**:1248/1248reachable,32/32frames,
  6/6persistent,673/7walls,1conflictrecord,2compiledmissing/67mismatch. No gate
  weakened or roomapproval. Exactcommand retained in`door-pot-lifecycle/gate.json`.

Files changed this performance task ONLY: `remaster/frame.h`, tiny earlyreject
loop inshared`remaster/remaster.h`, benchmark extension in
`tools/ALttP/alttp_room062_endframe_probe.cpp`, new
`alttp_height_perf_equivalence.py`, thisreport. No gfx.cpp/mac/render/shader edit.
Parent GPU work remains separate; all shared dirty artwork/correctness preserved.

Artifacts existingOUT=`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/door-pot-lifecycle`:
`perf-{before,index,final,ablation}`,`solver-perf-{reference,final}`,
`perf-corpus-equivalence.json`, byteequal`perf-{reference,production}016.s9xrmf`,
productionJSON/PPM/maps,`fixtures/corpus.json`performance-equivalencecounts,
regeneratedv2JSON/SVG. Timings above captured from toolstdout, not a liveappreport.

Reproduce repo-root:
```sh
c++ -std=c++17 -O2 tools/ALttP/alttp_room062_endframe_probe.cpp -o "$OUT/perf-final"
c++ -std=c++17 -O2 -DALTTP_TEST_LINEAR_FRAME_REGIONS -DALTTP_TEST_EXHAUSTIVE_PERSISTENT_BODIES tools/ALttP/alttp_room062_endframe_probe.cpp -o "$OUT/perf-ablation"
ALTTP_ENDFRAME_BENCHMARK=180 "$OUT/perf-ablation" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$OUT/perf-ablation"
ALTTP_ENDFRAME_BENCHMARK=180 "$OUT/perf-final" "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_016.s9xrmf" "$OUT/perf-final"
```
Next: parentbuild/independentbenchmark; coordinate liveCPU sample and actual
currentGPU/profile packet to establish remaining21msdirect transport and rebuild
frequency. No userapp intervention performed here. CPU regression repair is
implemented, not a claim of60FPS or completedroomgeometry.
