# Room060 own092 — measured performance repair; room heights still incomplete

## Verdict

Dominant measured replay cost is **direct-light visibility traversal on GPU**,
not Princess Zelda or repeated warm room solves. The supplied frame has no
historical timings: the user's live deterioration is not reproduced as a large
092-versus-earlier regression. A verified, output-identical CPU optimization
reduces warm092 EndFrame by about2.4ms (28%). GPU cost remains about20ms; this
does not establish60FPS or solve the reported live chronology.

No profile/shader quality edits, ray budget cuts, geometry output changes,
commits, Door recovery changes or rollback of prior runtime fixes.

## Pinned comparisons

ROM SHA256 `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Own capture/profile/map/attribute/template/terrain SHA256 pins in `pins.json` at
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/perf092-review/`.
Native Apple M3 Max CPU/Metal replay; C++17/O2 geometry. Each input uses its own
profile, not the latest root profile. Complete BG identity/camera checks pass.

|060 capture|profile serialized FNV|camera|Link plane|instances/metadata|
|---|---|---|---|---|
|092|7d5a12c5603c13ff|256,3243|1|1915/108|
|085|5a53a1f5b75105d6|256,3345|0|1866/87|
|079|3f781bcda5e8990c|256,3220|1|1707/80|

079 is the closer same-plane/camera comparison;085 is the requested earlier
south view. Different cameras/profiles/animation are NOT a controlled historical
A/B. All have scale200,bounces0,configured connections1,patch size5,representatives3,
original contribution0.0348732,area enabled brightness0.2. Direct replay uses the
shipping exhaustive path (direct sample override0), so configured connections1
does not reduce dense direct visibility work. No sphere sources in these frames.

## CPU diagnosis and exact repair

Native `sample` over1200warm092frames: placement application dominates EndFrame;
the call tree repeatedly enters per-pixel offset/floor/border selection, including
string role/group checks. Warm run has ZERO classifications/solves. Follower
pixels do not invalidate the complete-room input cache. All16normal terrain
sprite slots in these own terrain sidecars are inactive: Zelda is a tagalong
OAM producer, not one of these normal sprite support footprints. The actor-data
ablation is therefore NOT a true Zelda removal experiment.

Shared `frame.h` optimization retains per-pixel placement decisions from the
observation pass for the immediately following instance-splitting pass. These
decisions are immutable during this invocation. No cross-frame/cache reuse:
door masks, flips, signed bases, source-room ownership, camera, overwritten
placements, both BGs and independent planes retain their exact checks. Scratch
is heap-backed (~1.31MiB for two256x224 pixel buffers); not the small emulator
thread stack. Source pointers are never retained across invocations.

Three alternating-order independent runs, each5warmup+30measured per mode;
reported below are ranges of each run's median, ms. No concurrent GPU benchmarks.

| input/mode | reference | candidate |
|---|---|---|
|092 warm|8.44–8.55|6.10–6.13|
|092 forced full classification/solve|39.44–39.56|36.91–37.38|
|092 controlled moving camera/Link|8.42–8.62|6.12–6.24|
|092 alternating planes|20.07–20.70|17.77–18.01|
|085 warm|8.84–9.13|6.29–6.55|
|079 warm|8.03–8.32|5.80–5.93|

092warm p95 reference8.67–8.76 vs candidate6.38–6.66ms. Moving case triggers
zero classifications/solves; alternating-plane case retains required solve
invalidation and no redundant classification. Cold case explicitly forces full
rebuild each sample; it is not natural cache-miss frequency. Moving chronology
is controlled perturbation of a pinned frame, NOT recorded live movement.
Actor-evidence-off092warm6.07–6.12ms vs6.10–6.13 normally: no causal actor penalty.

Exact oracle: candidate vs `ALTTP_TEST_REFERENCE_PLACEMENT_SELECTION` reference
build produces byte-identical serialized packets AND complete two-BG inventories
for all63valid local packets.162lack complete evidence;9stale observations refuse.
For092/085/079, ten measured outputs in each of all five modes are additionally
written and compared byte-for-byte, not only hash assertions. New
`dungeon_selection_reuse_test.cpp` compares transformed/signed repeated-instance
splits and fallback/mixed-floor decisions against reference. Other wall/door,
stair, transparent-border, pedestal, obstacle, cache, pit/corner tests pass.

## Lighting CPU/GPU measurements and Zelda causality

Production-shader native replay, three order-rotated runs per frame,
5warmup+25measured GPU command buffers, pipelines compiled outside GPU duration.
These are immutable capture replay timings, not presentation FPS. CPU fields:
5warmup+40measured, forced-full vs unchanged incremental; mesh rebuilt separately.

| capture | GPU median (ms) | GPU p95 (ms) | CPU fields full/warm median | mesh median |
|---|---|---|---|---|
|092|20.27–20.42|21.85–22.11|2.67–2.70 /1.88–1.95|1.72–1.76|
|085|19.80–19.92|20.76–21.98|2.79–2.81 /1.96–1.99|1.70–1.71|
|079|22.98–23.07|24.89–24.98|2.32–2.33 /1.57–1.58|1.69–1.73|

|capture|visible emitting pixels|hidden emitter pixels|patches|prepared direct records|object pixels|
|---|---|---|---|---|---|
|092|260|428|18|264|772|
|085|322|366|21|304|285|
|079|197|447|15|300|285|

092does not have an explosion of lights/sources. Hidden emitters matter even
offscreen. Prepared records include authored depth strata; executed visibility
count depends on receiver orientation/geometry, occlusion and adaptive depth,
so raw record count alone does not predict time. Theoretical dense upper bound
264×57344 receiver-record pairs≈15.1million, before sidedness/culling/depth rays;
not an actual executed-ray counter. Configured representatives3 preserved.

Read-only diagnostic ablations, never shipping policies:
- Remove ALLobject pixels from092 (not Zelda alone): GPU medians19.55/20.20/20.41ms,
  still the same bottleneck, source records264 unchanged. Different output,
  so no speedup or image-equivalence claim. This provides no evidence Zelda
  causes the slowdown; one tagged follower cannot be isolated causally from
  these static packets.
- Bypass direct visibility in the diagnostic shader:0.370/0.374/0.377ms medians,
  compared with~20.3ms normally. Output changes, deliberately. This isolates
  visibility as overwhelmingly dominant, not field preparation or source setup.

No GPU optimization was shipped from these ablations. CPU optimization does
not alter shader inputs/output, proven by whole effective-packet equality.
The standalone harness measures visibility-block build, source prep, direct
transport/reduction and composite; it does NOT replay the application's separate
area-directional pass, presentation/driver waits, emulator execution, scene-field
upload or scheduling. It retains area metadata but does not claim area-pass
timing. Bounces0 means no missing indirect workload in this specific comparison.
No live pacing/drop trace or historical timing exists in the packet. Different
torch animation/visible blocker complexity and live cache-miss frequency remain
plausible contributors to reported chronology, not proven causes.

## Gates, builds and remaining evidence

Paired offline corpus346contexts/319rooms: zero changed complete geometry
inventories/counts; all solver outputs invariant. Final corpus audit pins own007
metadata separately; no offline room-height approval. Mandatory own092 v2:
711/711 supports,planes437/274,zero missing,exit1 (remaining structural conflict).
Room071001/007/008v2 still297/297,465/465,297/297 graph-scoped support,zero conflicts,
unknown BG2 walls270/264/264,exits1. Wider state/traversal and visual gates remain
incomplete. Performance equality is not room approval.

`git diff --check` passed. Release universal arm64/x86_64 build passed and updated
`build/release/Snes9x.app`; bundled framework matches product framework. SHA256
`36ae3fce625b9456be44582a3d644b395eb2100ad7c906e3fd88242dd1273e24`.
Relaunch older running app to use it. No fresh live GUI comparison performed.

Files: runtime change only `remaster/frame.h`; benchmark modes/oracles in existing
`dungeon_geometry_cache_capture_probe.cpp`; new selection regression; native
Metal probe gained repeated timing, cached pipelines, CPU field metrics and
explicit diagnostic ablations. Shader source/profiles are unchanged. Artifact
root above contains `geometry-*.txt`, `gpu-*.txt`, ablation logs, exact mode
packet/map streams, `equivalence.json`, `corpus.json`, SHA pins and own092 gate.

Next measured work: live Release trace spanning return-to-room, follower motion,
torch animation, solve epochs, CPU-active and GPU completion/drop timings. For
GPU improvements, profile exact visibility traversal/intersection counters on
these source/receiver geometries before choosing an output-preserving acceleration;
do not reduce light quality based on Zelda correlation.
