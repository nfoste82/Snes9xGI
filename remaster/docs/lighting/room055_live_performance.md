# Room 0x055 live performance — October 28, 2026

The owner subsequently requested optimization independent of historical
attribution and authorized the staged implementation. Current work and scoped
results are recorded in [Optimization Opportunities](room055_optimization_opportunities.md).
The measurements below remain pre-optimization evidence, not validation of the
new production path. PID4784 was absent at a later read-only preparation check;
do not assume that diagnostic app is still running.

## Matched ordinary-live PRE/CURRENT result

The matched ABBA comparison completed using the same user slot12, captured
profile23944ecb, room/Link position, lighting ON Composite, visible3/hidden1,
automatic frame skip(-1) and pacing flags. Both isolated V2 apps use the same
one-shot diagnostic restore hook, current tracing/UI/earlier CPU optimizations
and identical metallibs; PRE differs only in compartment production semantics.
All four launches verified the explicit restore-success marker before tracing.

| Leg | Completed/240 | Rendered refresh median/p95 ms | GPU median/p95 ms | Direct samples |
| --- | ---: | ---: | ---: | ---: |
|CURRENT A|187|18.47/24.30|5.53/9.02|112|
|PRE B|184|18.47/24.24|18.57/35.06|160|
|PRE C|185|18.73/24.65|19.04/34.08|160|
|CURRENT D|187|18.24/23.95|16.15/24.01|112|

This matched state does not reproduce a compartment-split CPU regression.
Visible-face medians are6.2–6.3ms, offscreen producer1.05–1.09ms, and
classification/solves zero in both variants. Both show53–54 changed completed
rows and26–27 artwork restores per leg: roughly two changed events per9 frames,
field preparation2.94ms unchanged versus8.3ms changed. This recurring cost
predates splitting in the controlled comparison; the exact changed dependency
has not been captured sufficiently to authorize a blanket cache exemption.

GPU times vary substantially even between CURRENT legs, so these values do not
establish stable GPU speedup. All unfinished rows remain in the artifacts; PRE B
also has one slot drop. Completed fractions do not establish display FPS or
prove skip reasons. No animation packet was created; per-frame replay/CGRAM
evidence remains absent. Slot12 and canonical profile unchanged. CURRENT was
left running as PID4784 with lighting enabled. Raw traces, summaries, restore
markers and setup screenshots:
`build/remaster-tests/room055-matched-apps/state-hook-v2/acquisition-20261028-194058/`.

Next performance candidate is exact incremental channel reselection with full
fallback and independent emission refresh, plus visible-face assembly cost.
Neither has been repaired by this diagnostic comparison. The user's earlier
near60FPS session remains unexplained; this result does not dismiss it.

## Reported post-split CPU regression — investigation open

The user reports nearly60FPS before compartment changes at visible3/hidden1,
now worse with refresh17–19ms and scene fields2–7ms. This comparison must use
the retained `compartment-startup/` headers, not Git HEAD, because earlier quality
and visible-face optimizations were already present before splitting.

Added per-frame work includes candidate inventory construction, two4096-cell
admission scans and connected-emitter traversal, with repeated palette emission
evaluation for admitted candidates. The geometry-support condition additionally
scans emitting assets; it changes solve requirements only when a review map
previously suppressed solving and sprite support did not already require it.
These are suspects, not measured causes. The earlier GPU cadence result does
not exclude a CPU regression.

Refresh timing spans StartScreenRefresh through the end of EndScreenRefresh,
including packet assembly and presentation CPU. Scene fields is nested later in
that interval and includes render-lock/slot wait, scene/HUD/owner/highlight work,
field-cache update, mesh update/copy and normals. Latest panel values can describe
different frames and must not be summed. An eight-second sample of PID91595
shows substantial visible-face work and some emitter/key/cache-restore stacks,
but cannot establish per-frame invalidation or solve frequency.

A paired pre-split/current CPU benchmark is in progress. The old replay helper
omits emitter palette, live layout and construction phase, so hidden-source parity
must be established before claiming it reproduces the live producer. Palette
must come from provable evidence; a synthetic palette is not live equivalence.
No speculative runtime optimization is justified yet.

### Paired CPU measurement result

Tooling-only exact006 replay now restores complete captured BG raster state,
construction/layout and only unambiguously witnessed palette entries. Current
hidden sources match every captured field (192/192 for006,408/408 for fresh007);
missing required palette witnesses fail closed. The old helper regression fails
at0 versus408 sources. This closes the source-dropping replay gap for these
fixtures, not arbitrary captures.

Across1296 verified observations, warm reconstruction+actual EndFrame measured
PRE13.381/13.311ms versus CURRENT13.012/13.444ms (two legs each). Producer
offscreen time stayed about1.0ms; supplemental admission scan+BFS measured0.003ms.
Field update stayed about0.59–0.60ms and mesh/copy/normals0.26ms when unchanged.
No repeatable multi-ms splitter overhead was reproduced. This excludes neither
live scheduling effects nor changed invalidation frequency; the benchmark omits
emulated rasterization and actual presentation/waits.

Warm cases perform no solves/restores; compiled review maps are absent. Cold or
repeated profile publication causes3 solves/frame in BOTH versions. Synthetic
referenced-graphics alternation causes exact artwork restores and44320 updated
field pixels, with about7.2ms fields plus1.55ms mesh in BOTH versions. This
demonstrates an existing invalidation cost, not its actual live cause/frequency.
Different expected hidden membership (PRE768/CURRENT192) leaves effective
visible field digests identical. No runtime optimization or gameplay setting
change was made.

The remaining diagnostic need is bounded same-frame live tracing of refresh,
producer/solve/restore reasons, changed field dependencies, mesh and lock/slot
waits. Existing latest-value panel readings cannot supply that lineage. Evidence:
`build/remaster-tests/room055-cpu-paired/REPORT.md`, final `runs006-final/`.

### Bounded live diagnostic instrumentation

The user authorized terminating/relaunching the app for diagnosis. A diagnostic
build now supports `S9X_REMASTER_LIVE_TRACE` plus optional
`S9X_REMASTER_LIVE_TRACE_FRAMES` (default120, hard cap600), triggered by the
existing frame-capture request. Rows own stable producer IDs through refresh,
renderer and asynchronous completion/drop. The buffered writer flushes after
completion or30-second timeout, preserving unfinished evidence explicitly.
Output includes refresh-inclusive timing, exclusive producer/field stages,
solve/restore/reuse counts, selection state and multiple actual field-dependency
change flags. It does not supply per-frame replay packets/palette/raster snapshots;
each row declares `replay_evidence:false`. Flags indicate compared input changes,
not proof that each flag independently caused a costly rebuild.

Focused O2/ASan/UBSan tests and Python summary regression pass, including bounded
capture, reverse completion order, drops/cancellation, multiple reason flags and
trace-on/off output parity. Debug/Release builds pass. Synthetic unchanged-cache
diagnostic overhead measured229.6us/frame off versus457.8us on; this is not a live
overhead bound. Live collection/interpretation is pending; no performance fix is
claimed. Prelaunch app/framework/metallib pins and preserved current game/profile
state are required for the coordinated live run.

The first automated state-preservation attempts did not create a save; the app
was not quit and original slot000 remained hash-identical to its backup. Link's
position changed during tooling interaction without an established cause; this
must not be described as a motionless test. The user then manually saved and
identified slot12 for room055. A coordinated diagnostic relaunch will use that
user-provided state, with a separate backup and the captured active profile.

Instrumentation review accepts normal-path ID joining and exclusive scope
accounting, with an important acquisition limit: the frame-capture trigger also
records120 animation frames and serializes the result. Such rows are not an
unperturbed gameplay baseline. A longer trace can sample post-capture work only
with independently established completion bounds; schema1 lacks capture-progress
flags. Selection timing is the whole field-cache update; fields is exclusive and
not identical to the panel's inclusive scene-fields value. Refresh is elapsed
wall time, not thread CPU; rows sample emulated frame before its increment.
Reason flags show changed inputs, not independent causes. Drop/unfinished rows
and unsuccessful GPU status must be handled explicitly.

### Slot12 trace result (capture-perturbed benchmark)

The pinned diagnostic benchmark restored the user-provided slot12 with captured
profile23944ecb, Composite, visible3/hidden1. Slot backup hashcb13e0bc and the
canonical profile remained unchanged. The app exited on benchmark completion;
this was not an ordinary gameplay session. It produced120 completed rows with
no drops; animation tracking perturbed the trace and final capture serialization
took137.6ms refresh. Do not include that endpoint in gameplay percentiles.

For118 interior rows, refresh median18.85ms/p9524.66ms reproduces the reported
range. Visible faces measured6.21ms median; classification and solves were zero.
The field-cache update measured0.87ms on92 unchanged rows versus6.20ms on26
changed rows (inclusive field preparation2.94ms versus8.3ms). The changed rows
occur every9 frames in two alternating groups,13 with exact artwork restores.
All26 show content-geometry, emission-metadata, instance-identity and palette
input differences;13 additionally show raster differences. Profile key is fixed,
and lock/slot waits negligible. Flags are correlations, not causal attribution.

GPU median9.24ms/p9517.10ms. Trace-off benchmark latest-refresh medians17.88/17.99ms
before/after capture are not same-frame measurements or precise overhead proof.
No per-frame replay evidence exists. Artifacts under
`build/remaster-tests/room055-live-trace-pinned/`: `bench-live-20261028.121.jsonl`,
`summary.json` and `benchmark-slot011/report.json`.

Next scope is a trace-only trigger (no animation capture), an ordinary live-state
run and targeted analysis of the repeating field-dependency changes. No inferred
solver-every-frame bug or performance repair is established by this trace.

Independent analysis identifies masks880 and368 (13 frames each). Every changed
frame reselects57344 pixels. Mask880 rebuilds geometry for98 pixels; mask368
rebuilds3707–3709 pixels, resolves24 instances and coincides with the13 exact
artwork restores. The cache promotes qualifying metadata/instance/context/mask/
raster/depth changes to global channel reselection, then avoids many geometry
rebuilds using per-pixel donor/dirty checks. Broad reselection is a candidate cost,
not proof that untouched pixels have equivalent old/new selection dependencies.
The pre-split cache uses the same algorithm. Safe incremental selection requires
old and new references, complete donor/claim parity, independent emission refresh
and conservative full fallback—not a blanket animation exemption.

### Trace-only trigger ready

With both `S9X_REMASTER_LIVE_TRACE` and `S9X_REMASTER_LIVE_TRACE_ONLY=1`, the
existing capture command starts bounded tracing and returns before changing any
packet/animation capture request. Ordinary command behavior is retained without
the explicit mode and trace prefix. Focused regression, ASan/UBSan and final
Debug/Release builds pass; source/product pins are in
`build/remaster-tests/trace-only-trigger/`. No incremental cache optimization has
been implemented. The next acquisition uses ordinary gameplay, not benchmark
mode, and verifies that no animation packet is written.

### Verified lighting-enabled ordinary traces

The user correctly identified that the initial ordinary-session collection had
lighting disabled. Startup defaults lighting off; merely loading the profile/state
and having Composite selected was insufficient verification. Traces241 and482
are excluded from lighting conclusions. After explicitly verifying lighting
checked, Composite, active profile23944ecb and visible3/hidden1, replacement
traces723 and964 were collected without animation capture. PID98557 was left
running with lighting enabled. All freeze slots and canonical profile unchanged.

Each window contains240 refresh rows;723 has188 renderer/GPU-completed rows and
52 unfinished,964 has186 completed and54 unfinished. Completed successful rows
retain112 Direct samples. Unfinished rows are not zero-cost lighting observations
or automatically classified as drops; rendered/skipped lineage needs analysis.

| Completed-row metric |723|964|
| --- | ---: | ---: |
| Refresh median/p95 ms |18.47/24.27|18.35/23.98|
| GPU median/p95 ms |16.18/22.67|15.77/18.96|
| Visible-face median ms |6.26|6.22|
| Offscreen producer median ms |1.08|1.08|

54/53 completed rows reselect all57344 pixels in recurring roughly9-frame
groups. Inclusive field preparation is about8.3ms changed versus2.9ms unchanged;
half the changed events coincide with exact artwork restores. No classification
or solves recorded, lock/slot waits negligible. Reason flags are not independently
causal. These ordinary lit traces confirm the CPU cost without animation capture,
but still lack per-frame replay packets/CGRAM/raster evidence and a matched
pre-split live baseline. No split-caused regression or optimization is proven.
Artifacts: `build/remaster-tests/room055-live-trace-only-20261028/trace.723.jsonl`
and `trace.964.jsonl`.

Independent all-row analysis gives refresh median/p9518.30/24.18ms and
18.24/23.92ms. Every completed rendered refresh exceeds16.67ms. The106
unfinished rows have median0.22ms and zero producer/renderer work; this is strongly
consistent with render skipping under `IPPU.RenderThisFrame`, not proof of GPU
stalling. Render/skip flag and actual frame-skip mode are absent from schema1,
so early exits remain an alternate explanation. Such rows also delay trace flush
until timeout. The completed fraction is not display FPS; serialized uptime lacks
the precision and presentation evidence needed to derive it accurately.

Before changing cache behavior, prepare matched ordinary-live PRE/current builds
from the same current instrumented source, reverting only the compartment
production hunks for PRE. Restore the same slot/profile/quality/view/pacing and
compare repeat runs. Do not replace all of remaster.h with the old snapshot:
that would remove diagnostics and conflate unrelated settings/UI changes.

The matched apps are prepared under `build/remaster-tests/room055-matched-apps/`.
Both Release builds pass and retain identical metallibs. The1516-file snapshots
differ only in the two intended headers: PRE restores producer header3007f50c and
reverses production emitter-support/base-selection hunks while preserving current
diagnostics, UI and prior optimizations. An asset-presence scan remains in PRE
for common trace flags; this is instrumented PRE semantics, not a historical
binary. Staged exact006 regression retains CURRENT192 exact hidden sources and
PRE768 expected sources with equal visible field digests. App/source/profile/slot
manifests, completed gate results and acquisition requirements are in `STATUS.md`
and `OPERATOR.md`. No live result follows from build/parity checks alone; ABBA
ordinary-session acquisition is the next step.

## Fresh settled live check (capture006)

The user authorized a diagnostic capture without movement, settings changes or
restart. `frame_room055_006.s9xrmf` and matching sidecars pin a settled first
frame (the native capture collects120 animation frames). Live evidence is now
present: quadrant `[2,0,19,1,2]`, loaded layout16, no blast walls/overrides,
raw scroll `[2809,2832]`, rendered `[2809,2833]`. The filter validates the
full-width southern half; it does not fail open in this state.

Capture006 retains192 hidden emitting pixels,48 hidden Direct records and24
visible Direct records:72 total, matching all20 live panel observations. Visible
quality is3 and offscreen quality1. Earlier005 retained787 hidden pixels and420
total records at different quality/camera state. Those counts are not a controlled
performance comparison or proof of actual transition continuity.

After the full test gate stopped,20 live latest-value panel observations measured:

| Metric | Min ms | Median ms | Max ms |
| --- | ---: | ---: | ---: |
| CPU lighting |3.26|3.38|8.92|
| Emulator refresh |0.20|16.97|23.45|
| GPU frame |11.81|14.66|28.79|
| Direct GPU |11.39|13.64|15.94|

Latest-value readings can be from different frames; their medians must not be
summed. The remaining observed bottleneck is GPU Direct, not failure to cull the
northern compartment. Cause of the perceived equal/worse live performance is
not yet isolated. Sampling and concurrent GPU activity affect comparisons.
PID87323 started18:03:17 and loaded the17:59 framework UUID
`7266E41E-5A4F-3EA7-BA41-DC0419F3F8D0`, build label17:59:33; later rebuilds on
disk do not replace its code. No profile changes or gameplay movement were made.

Profile Controls now has a vertical scrollbar, resizable screen-constrained
viewport and preserved scroll position. All13 checks pass including isolated
AppKit reachability/resize/placement fixtures; Release build passed. Restart is
required to load this UI; the user's running game was left intact.

### Follow-up packet replay and remaining cost

Warmed31-frame production-renderer ABBA observations using the same renderer
and shader source measured the following GPU medians across two legs:

| Packet / hidden quality | Direct records | GPU median ms |
| --- | ---: | ---: |
|005 /3|420|12.73 /12.86|
|006 /1|72|4.23 /4.25|
|005 /1|156|4.21 /4.18|

005 versus006 changes camera and settings as well as source membership. The
156-versus72 comparison demonstrates that record count alone does not predict
cost for different receiver/source geometry. Diagnostic removal of hidden006
sources reduced replay GPU from about4.3 to1.3ms, changing lighting; a separate
visibility-bypassing shader probe reduced about3.4–5.0 to0.19ms, changing shadows.
Neither is a production optimization or parity result. They identify visibility
traversal as a useful next GPU investigation.

The fixed offscreen replay's roughly4.2ms remains far below the live panel's
roughly14.7ms. Shader-library identity, instrumentation, scheduling, animation and
contention need to be separated before assigning a cause. No live speedup or
regression is proven. Tooling-only parity/timing investigation is underway;
source power, shadow semantics, profile and gameplay state remain unchanged.
Artifacts: approved temp `opencode/room055-perf-006/`.

### Cadence control resolves part of the replay discrepancy

The tooling-only follow-up reproduces the timing gap with immutable packet006:
continuous synchronous replay measures3.65–4.28ms GPU, while paced synchronous
runs measure22.07–24.51ms and paced production asynchronous submission17.20ms
(Direct about11.5ms). A final continuous recheck returns to3.59ms. All533 final
frames retain72 records, six patches and identical geometry inputs. Actual final
Direct (`directMean[0]`, not the legacy emission-seed `.direct` dump) and Composite
bytes match across all13 legs. Counters-disabled paced runs remain slow, so
counter overhead does not explain the discrepancy. No timestamp defect reproduced.

This establishes cadence/load sensitivity, not its underlying hardware/OS cause.
GPU clock/power behavior, scheduling/preemption and contention with the running
emulator remain unresolved. Pacing sleeps16.667ms after service, not an exact60Hz
deadline schedule; measured submission intervals differ. Geometry/animation
differences are unnecessary to reproduce the gap but may still affect live play.
Continuous microbenchmark timing must not be presented as a live frame budget.

Pinned disk compiled metallib and source shader produce identical output; the
exact loaded17:59 metallib is unproven because the disk app was rebuilt after
launch. No exact live-library or120-frame renderer-packet reproduction is claimed.
The next matched-live experiment needs immutable prelaunch app/library pins and
per-frame traces, with controlled cadence and contention. No production renderer
or shader optimization was made from these diagnostics.

Complete evidence: `build/remaster-tests/room055-perf006-engineer/REPORT.md` and
`runs4/summary.json`, including immutable pins and raw per-frame traces.

## Current compartment implementation status

A generalized settled-compartment filter is implemented in the hidden-source
producer. It validates live layout/quadrant, full-size flags, overrides/blast walls
and camera-bound consistency, then selects sources in constructed-room coordinates.
Missing or inconsistent evidence retains legacy whole-room inclusion. All eight
same-room transition phases and shutter animation retain the whole-room source
union; filtering resumes only at settled submodule0/phase0.
Connected emitting footprints at a split
are conservatively retained. No authored geometry, grouping or profile is changed.

The fixture-free production regression passes378 checks (Debug, O2/NDEBUG and
ASan/UBSan). Its nine-source layout selects3 settled and9 in every transition
phase; an actual EndFrame boundary-extended inventory emits4 hidden pixels
settled,10 throughout scrolling and shutters, then4 on settled return. These synthetic
counts are not live room055 results. Inter-room source reconstruction and
unproved visible-occurrence suppression remain outside this patch.

Independent review found three acceptance blockers in the initial implementation:
contradictory camera scroll was not rejected, the union ended at shutter entry
rather than completion, and an existing cached BG1 source-base path lacked a
current-input validity gate. All three were corrected with baseline-failing
regressions. A further review-map compatibility regression was reproduced and
fixed: emitting profiles solve current runtime geometry even with compiled review
visualization, and both BG source paths consume only current runtime solves.
Review visualization is unchanged; review-only emitting profiles can incur extra
CPU solve work. No geometry rules or authored heights were changed.

The corrected contract retains both compartments through submodule5 shutter
completion, fails open on contradictory scroll, and never reuses either BG's
source bases when current geometry inputs are unpublishable. Scroll consistency
uses the documented rendered vertical+1 scanline bias, not arbitrary tolerance.

Implementation details and remaining acceptance are in
[Offscreen Emitters](offscreen_emitters.md). Independent review accepts the targeted
repair; final Release/Debug builds passed, and the frozen-source full+Metal gate
passed80/80 (`build/remaster-tests/20261028T235924Z-ikt2iabp/results.json`).
The234 older dungeon sidecars across12 rooms lack usable layout evidence;
capture006 now validates one settled state only. Transition chronology and visual
review remain required. The investigation sections below describe earlier stages and
must not be read as a claim that no filter is now implemented.

## Finding

Fewer visible emitting pixels did not mean fewer direct-light samples. The live
room055 capture used three representatives and420 samples,396 of them hidden
sources. The earlier room011 live representatives1 comparison used104 samples.
Room055 is substantially GPU-bound in these observations, with presentation
backpressure as well as remaining CPU assembly work. No production changes,
profile edits, live setting changes, restart or Link movement were performed.

## Live evidence

PID61977, launched15:19, performance panel build label15:05:35. Sampled stacks
include the optimized visible-face code and unordered clone lookup. Room label
0x055, Composite, zero bounces, no sphere lights, directional disabled. Area UI
uses profile-default representatives3. The current profile has user edits;
they were preserved.

Twenty read-only panel observations:

| Metric | Minimum | Median | Maximum |
| --- | ---: | ---: | ---: |
| GPU frame ms |38.19|58.31|105.41|
| Direct GPU ms |28.07|45.085|61.43|
| Scene fields ms |2.57|3.13|10.48|
| Presentation queue ms |0.01|0.015|64.60|
| Drawable ms |0.02|21.835|116.46|
| Visible emitter count |77|80|80|
| Direct samples |420|420|420|

These latest-value metrics can refer to different frames. They do not establish
interval throughput or permit summing medians into a frame budget. The8-second
`sample` independently found4277/4283 presentation-queue observations inside
`nextDrawable`. That wait is real but does not by itself identify all causes of
display backpressure. CPU work remains; the earlier CPU optimization is loaded.

Local evidence: `build/remaster-tests/room055-live-sample.txt` and
`room055-live-metrics.jsonl`.

## Fresh captured-packet analysis

Requested a running animation capture through the View menu, without moving
Link. Prefix:
`~/Library/Application Support/Snes9x/Remaster/frame_room055_005.s9xrmf`.

The captured packet contains787 hidden emitting pixels; all have positive depth
and omnidirectional emission, and768 are outside the viewport. The hidden-source
path buckets these into33 screen-aligned8x8 groups. At three representatives,
four depth strata produce33*3*4=396 samples. Six visible patches likewise produce
24 samples. Total420 matches the live panel exactly.

`hidden_emitter_samples.h` buckets hidden sources by screen cell, depth mode and
directional mode. This is not the same grouping as authored combined visible
shapes in `emitter_patches.h`. Visible emitter counts do not include this hidden
work; comparing those counts alone conceals the dominant sampling cost.

### Counter meaning and visible grouping follow-up

The panel's `emitters` value is **visible emitting pixels before aggregation**,
not tile placements, connected fixture sets or representative count.
`mac-render.mm:1353–1373` counts active participation/emission pixels;
`directSampleCount` separately includes visible representatives and hidden
sources, expanded for positive depth. Thus a reported66 emitters is not66
representatives and is not itself evidence of northern source inclusion.

The actual field/patch probe `build/remaster-tests/room055-visible-groups.cpp`
finds77 active visible pixels in capture005 and six representative patches.
Three patches use instances2252/2284 in screen x128–134,y26–33; three use
2253/2285 in x137–143,y26–33. Both vertical pairs use top hash
`34eb3798eedfa0fc` and bottom hash `e41e81c4f0f14d5f`. This is a single
four-tile footprint split into two sets of three representatives, rather than
one set of three. All six patches have depth layout, producing24 Direct records.
The combining algorithm requires reciprocal authored links, including when
adjacent placements repeat a tile identity. Complete-room spatial/link analysis
is tracked separately from this viewport observation.
The capture-time profile links both hashes to the other three flame identities
but neither to itself. Hence equal-identity horizontal neighbors fail the
reciprocal link test, while the top/bottom pair joins. This explains the observed
six visible representatives without interpreting the pixel counter as a group
counter. No authoring or combining rule was changed during inspection.
For the user's intended three complete four-tile fixtures, three representatives
per connected fixture would mean nine representatives. With positive depth,
the current deterministic transport expands these to36 Direct records. Neither
number should be compared to the pre-aggregation visible-pixel counter.

### Complete constructed-room source locations

The tool-first whole-map follow-up found36 BG2 placements of the two observed
flame identities, in nine2x2 occurrences. Tile coordinates (64x64 map):
four at x27–28/39–40,y9–10/21–22; four at x12–13/18–19,y27–28/37–38;
one at x48–49,y37–38. Packet hidden-source positions match384 pixels at the
first four,384 at the next four, and19 at the viewport fixture. Its77 visible
pixels complete864=9*96 pixels across these nine occurrences.

Thus far-north/west whole-room sources are included alongside the camera-local
fixture. The loader has no gameplay-compartment filter. These spatial counts
strongly match the packet, but hidden records lack original tile/BG/plane IDs;
the query does not rule out every other emitting identity. Nor does one capture
locate the user-observed north/south transition boundary: do not call the upper
four and lower five coordinate-band fixtures proven gameplay rooms, or infer
which three fixtures belong to the intended southern compartment. A matched
transition/other-side capture or verified topology is still needed for that.
Source enumeration also does not prove nonzero contribution at every receiver.

Full audit evidence and tooling gaps:
`build/room-height-audits/room055-emission-spatial-20261028/REPORT.md`.

## Controlled offline representative comparison

Actual production renderer, captured packet unchanged except representative
count in a separate generated copy.31 observations each, first five discarded;
the live game remained running, so GPU contention and thermal drift are possible.

| Representatives | Visible patches | Hidden samples | Total samples | GPU median |
| --- | ---: | ---: | ---: | ---: |
|1|2|132|140|24.313 ms|
|3|6|396|420|54.269 ms|

This changes an approximation setting, not an exact-output optimization. It
demonstrates sensitivity to sample workload, not guaranteed live FPS or visual
equivalence. Even one representative remained above a16.67ms GPU frame budget
under these conditions.

Temporary analysis sources/binaries are under `build/remaster-tests/`:
`room055-source-probe.cpp`, `room055-representatives-replay.mm`, and their
corresponding executables and JSONL traces.

## Important replay limit

The existing EndFrame reconstruction helper does **not** reproduce this capture's
hidden-source output: it produced only24 samples versus the packet's420.
Consequently its14.45ms GPU result is not valid evidence of live room055 cost or
an optimization. The packet-only renderer replay retained420 samples and measured
57.69ms GPU median, consistent in scale with the live observations. Any future
EndFrame optimization benchmark for this scene must first restore missing
hidden-source provenance/observed raster inputs and prove packet parity.

## Next boundary

Investigate hidden-source grouping and deterministic depth transport, retaining
offscreen contribution, source power, geometry and occlusion. Do not simply
discard offscreen lights or edit authored depth to make the benchmark faster.
This inspection does not authorize a production repair or approve room geometry.

## Authorized optimization follow-up

The user subsequently authorized compartment-aware source filtering, confident
offscreen sampling/grouping optimizations and an optional independent offscreen
representative setting. The earlier inspection-only authorization limit above
describes that initial investigation, not this follow-up.

### Cost per source versus aggregate work

Offscreen sources do not use a separate intrinsically more expensive transport
kernel. Prepared visible and hidden records pass through the same Direct loop
(`shaders.metal:1282–1307`). Hidden records use a sentinel source ID and skip
visible-patch receiver-self-exclusion; positive-depth hidden records already
participate in the adaptive depth-strata logic when clustered Direct is enabled.
Both evaluate the same facing/form-factor and visibility functions. Ray length,
blockers, receivers and source geometry still affect individual sample cost, so
equal sample counts need not have equal timings.

The demonstrated amplification is mainly the **number of source records**:
hidden sources lose authored fixture grouping and use screen8x8 buckets, while
all sources in the complete constructed map remain eligible. Reducing hidden
representatives independently is a quality/performance choice, not an exact
speedup or proof that light from an inactive compartment is valid.

Timing caveat for the follow-up: rerunning the unchanged old renderer on the
same420-record packet later measured13.70ms GPU median (31 frames, first five
discarded), far below the earlier54ms under other live load. A later live panel
snapshot also showed576 records rather than420. Neither settings nor player
state were controlled by the investigation during that interval. Preserve each
capture's exact workload, use same-session interleaved comparisons, and do not
promise the earlier absolute saving as a stable live result.

### Compartment investigation

The verified ROM room055 header selects layout4 (`$0A:8047`, bytes `e5 10`;
layout flags at `$82:B5AC`). The local reference engine's
`Dungeon_AdjustForRoomLayout` (`src/dungeon.c:7967` in
`build/remaster-geometry/room-preview-engine-v21/`) makes this ordinarily
full-width/half-height. Live blast-wall flags and override bytes can enlarge
that state, so the ROM header alone cannot authorize excluding source tiles.

If ordinary southern-half state is confirmed, the nine known torch footprints
divide into six north and three south. In particular the two fixtures at tile
rows27–28 are northern despite sitting near the middle of the map. Calculated
packet counts after excluding those northern sources would be211 hidden pixels
(787-576),108 hidden Direct records, and132 total records at the current grouping
and reps3. These are conditional counts, not measured repaired output or timing.

Current capture sidecars lack live `$A6–AA`, `$40E–40F`, `$452–453`, `$FC–FD`
and `$600–60F` layout/bounds evidence. Safe generalized filtering must capture
and validate those fields, fail open for legacy/ambiguous inputs, and retain
outgoing/incoming coverage during transitions. The current emitter lifecycle
rejects submodule1, so merely disabling a settled-state filter during that phase
does not establish transition continuity. Instrumentation and a matched live
transition sequence are required before enabling compartment culling.

Acceptance for a future compartment filter: explicitly present verified live
layout/override/bounds evidence; old or inconsistent evidence preserves existing
source inclusion; source fixtures crossing a boundary remain conservatively
included; intra-room scrolling retains both sides through early and late phases;
reload/defrost/interrupt/reversal do not reuse stale membership. Normal full-room,
half-width, half-height and quarter-room layouts must share the same policy.
Camera bounds are evidence of the game's compartment, not a newly inferred
physical occluder or proof of zero light contribution across that boundary.

### Independent hidden quality implementation evidence

The independent offscreen representative setting now exists; its full semantics
and UI are documented in [Offscreen Representatives](offscreen_representatives.md).
No canonical-profile value was changed. Profile/area omission inherits the prior
effective visible quality, including existing per-area overrides. The hidden
channel includes HUD-covered emission as well as sources outside the viewport.

Packet005 renderer ABBA old/new/new/old (31 observations, first five discarded)
retained420 records and six visible patches. Direct/output buffers matched
byte-for-byte across all four legs. GPU medians were14.021/13.594/13.905/14.945ms;
this establishes compatibility, not an optimization speedup at equal quality.
Manifest: `build/remaster-tests/room055-settings-abba/summary.json`.

Setting only hidden quality1 while preserving visible3 kept six visible patches
and reduced records to156 (132 hidden +24 visible), GPU median4.901ms in a
separate31-observation run. This is not a controlled live FPS comparison. A
single-capture composite comparison changed1861/57344 pixels, RGB byte mean
absolute difference0.02194, maximum53: a small mean does not certify local
shadows or camera-transition appearance. The legacy `.direct` probe dump is
not the final transported Direct mean texture and must not be used to infer
image equivalence for this quality override. Final composite comparison artifact:
`build/remaster-tests/room055-hidden1-image-difference.json`.

Offscreen authored-fixture regrouping and same-identity self-link semantics have
not been changed. Independent quality is useful now but does not complete that
separate provenance/grouping repair or compartment filtering.

The panel wording is corrected from ambiguous `emitters` to `visible emitting
pixels`; `direct samples` still includes hidden and visible transport records.

### Layout evidence implementation and integration

`dungeon_layout_evidence.h` extracts value-owned raw verified-USA indoor state;
`gfx.cpp` samples it with the other completed-frame inputs. Dungeon JSON has
explicit `layout_evidence.available` and, when present, raw quadrant, loaded
layout, blast-wall, overrides, bounds, scroll-copy, targets and transition counter.
The first-frame capture pin owns a copy. Binary packet/ALTPDI1/RTE1 formats and
all emitter inclusion decisions are unchanged. Existing recordings do not carry
these new fields. Bounded `S9X_REMASTER_DUNGEON_SCROLL_CAPTURE` now recognizes
submodule1 as well as2, still two crossings of at most120 frames without overwrite.
This window is not guaranteed to include the preceding settled frame, every
transition phase or a second crossing while the first is armed. Inspect actual
phase chronology before treating it as complete. Existing-file checks are not
atomic across simultaneous writers. The new layout values are owned by the pin;
that does not make every older context pointer owned, nor pin the preexisting
`continuity_session` field that the JSON writer reads at write time.

The new layout regression passed Debug/O2/ASan+UBSan, checking extraction offsets,
absent evidence, copy ownership, JSON, bounded/no-overwrite capture dispatch and
unchanged synthetic packet/RTE1 bytes. Source and test evidence:
`build/remaster-tests/layout-evidence-startup/REPORT.md`.

Final arm64 Release and Debug builds passed. Final inherited renderer output
matches the old captured-packet output. Profile Controls layout checks passed10/10.
The historical opt-in `offscreen_emitters_test.cpp` held-lamp publication assertion
fails on both unchanged HEAD sources and current sources, independently compiled;
it is retained as a baseline failure, not silently repaired or called passing.
The curated full/Metal integration result is recorded separately when completed.
The stable-source full+Metal gate passed79/79:
`build/remaster-tests/20261028T221200Z-zt3i94f4/results.json`.
Afterward only the precision-sensitive Metal test was promoted to RGBA32Float;
the recompiled tracked harness passed1406/1406 with adaptive depth off and on.
Production sources stayed identical to the full-gate/build snapshot. An earlier
integration attempt was invalidated by concurrent source corrections and is not
used as a passing gate.
Independent review identified hidden adaptive-depth dependence on the visible
pipeline, and cleanup of default-only area records on returning to inheritance;
both were corrected. A supplementary RGBA32Float witness fails the pre-correction
shader in four serial/batched far-depth cases and passes the corrected shader
with adaptive depth both on and off (1406 checks per mode). The original
half-float witness rounded those differences away and was not sufficient alone.
Exact archived-first-version shader reproduction and hashes are retained under
`build/remaster-tests/offscreen-depth-sensitivity/`; authoritative run log:
`offscreen-depth-sensitivity-verified-run.log`. The tracked witness is to use
RGBA32Float for this precision-sensitive comparison; production RGBA16Float
coverage remains elsewhere in the same harness.
Profile merge remains
whole-area-record conflict detection, not automatic same-area fieldwise merging.

The running game was not restarted, moved or reconfigured. New native controls
and layout evidence require restarting `build/release/Snes9x.app`. Live UI
save/Undo/paused-preview checks and actual same-room transition capture/visual
validation remain outstanding. No runtime compartment cull was enabled.
Suggested live evaluation: keep visible representatives3; in Area Settings turn
off the offscreen control's Use profile default and choose1. Do not interpret
unchecking the approximation checkbox as turning lights off: it selects exact
per-pixel sampling and may be much slower. Recheck torch/camera/HUD boundaries
and near-field shadows before saving. A future transition-evidence run must
begin with the opt-in capture prefix set at launch and verify the phase sequence;
ordinary legacy captures alone cannot establish the requested culling policy.
An additional inherited packet-only comparison in room011_020 retained exact
Direct/composite bytes. Attempts with older room012_038/room061_079 packets
reached renderer output but failed the probe's final packet serialization; these
are not passing end-to-end comparisons or a source-preserving EndFrame corpus.
