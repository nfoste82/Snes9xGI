# Room 0x055 optimization opportunities

Investigation: October 28, 2026. This work targets current costs, not attribution
to earlier versions. The owner subsequently authorized implementation in the
order below. Implementation results are recorded separately from prototype
measurements; no quality reduction or authored-geometry change is authorized.

## Implementation sequence

1. Integrate the exact indexed HUD helper into actual scene-color preparation;
   independently review production wiring and parity before changing cache policy.
2. Add conservative local field reselection, with complete candidate dependencies,
   independent emission refresh, and full fallback for global/unsupported changes.
3. Measure visible-face substages and optimize the demonstrated hotspot, preferring
   invocation-local reuse before more complex cross-frame caches.
4. Freeze source changes, run full/Metal correctness gates and native builds, then
   validate measured gains against the implementation-start baseline. Keep the
   running diagnostic app and original save/profile inputs intact until a
   coordinated live measurement is ready.

### Step 1 implemented and independently reviewed

`RemasterHeldLampHudLookup` and `S9xRemasterPrepareHeldLampSceneColors` now provide
the C++14-compatible production implementation in `held_lamp.h`. The actual
renderer scene pass uses it. The field-cache consumer constructs its index only
after the unchanged-input early return. The original scalar predicate remains
unchanged as an independent oracle. No selection/invalidation policy changed in
this step.

Construction-inclusive integrated production-helper medians were 1.476 → 0.132 ms
for capture006 and 1.473 → 0.139 ms for capture007. Complete predicate/color-byte
parity, contrasting-color synthetic cases, malformed inputs, first-match duplicate
negative controls, C++14, Debug and ASan/UBSan tests passed. The arm64 Debug app
built successfully. The repair expert independently compiled and reran focused
production-helper, field-consumer, selection and common-consumer tests and accepted
the patch. Final full/Metal gates and live measurement remain pending.

Evidence: `build/remaster-tests/room055-hud-integration/REPORT.md`. Subsequent
selection benchmarks use this integrated step as their baseline, so gains are
not double-counted. Prototype-only statements below describe the earlier
investigation, not the current integration status.

### Step 2 implemented and independently reviewed

The production field cache now reselects offsets affected by changed old/new
references across all seven rasters, four depth planes or changed candidate
instances, including unselected fallback candidates. Global shape/count,
ROM/area/scene/scale, material and first-match HUD-policy changes retain full
selection. Emission refresh remains independently global; untouched geometry
retains renderer-written derived normals.

Paired construction-inclusive field-update measurements against the accepted
step 1 baseline, for explicitly synthetic localized mutations to real captures:

| Capture | Selected pixels, baseline → new | Field update, baseline → new |
| --- | ---: | ---: |
| 006 | 57,344 → 49 | 4.144 → 2.671 ms |
| 007 | 57,344 → 56 | 4.162 → 2.703 ms |

Unchanged overhead was approximately 1–6 microseconds in this run. Global/cold
work remains full and was slightly slower; no universal gain is claimed. The
frozen baseline fails the deterministic local-work regression. Complete donor
triples, claims, arrays, samples, derived normals and mesh match warmed frozen
and fresh full consumers, except two named preexisting duplicate-snapshot
negative controls. Retained metadata now records the actual last-match field
producer, fixing those stale switchbacks without changing producer precedence.
The frozen reference remains unchanged. First-match HUD policy is tracked
separately and tested with active width256 transport cases.

Fresh independent C++14, sanitizer and related-consumer regressions passed; the
repair expert accepted the scoped patch. The legacy field-cache test's historical
single-source oracle fails identically on baseline and new code for006/007 and
remains recorded, not weakened. Full patch-byte/GPU comparisons and live
animation benefit remain final validation tasks. Evidence and exact matrix:
`build/remaster-tests/room055-local-selection/REPORT.md`.

### Step 3 measured target and bounded candidate

Isolated timer-only copies separated the visible-face substages. Annotation was
the largest warm substage: approximately 2.37 ms of 4.94 ms for006 and 3.24 ms of
5.86 ms for007. The candidate therefore reuses dead exposure scratch to memoize
only invariant placement identity checks within the invocation, keyed by BG,
room cell and actual occurrence ID. Coordinate/flip correspondence, masks,
plateau sources, door conflicts, selected donors, publication and clone order
remain per-pixel. No persistent graph cache or new heap allocation is introduced.

Final frozen-output and allocation checks passed with fingerprint
`18198169831120924552` and 941,568 allocated bytes (the existing <950,000 bound
is unchanged). The quiet uninstrumented warm repeat measured006 4.880 → 4.776 ms
and007 5.698 → 5.665 ms (24 alternating pairs each; paired median savings
0.093/0.049 ms). These are modest offline gains; cold/changed tails are mixed.
Repeated identity checks fell from7,168 to18 in the deterministic fixture.

Complete EndFrame packet/field/donor/normal/mesh differential passed21 captures.
Exact native hidden records remain192/192 and408/408 for006/007 cold and warm.
Supplemental cases establish same-input differential, not exact live palette
reconstruction. Room011_001 did not invoke this function and is not a pass for
its coverage. The optional pinned research gate passes32 cases then encounters
missing authored candidate content, identically on the frozen baseline; it is
partial, not a passing gate. See the complete matrix and source pins in
`build/remaster-tests/room055-visible-optimization/REPORT.md`.

The final source hash is
`7fd1cf218aeb90d08ae45797406024cbcbbaf9c1a51fe785c033f114a290e854`.
Final full/Metal gates, native builds and renderer-level sequence checks are
running against frozen source; no live gain is established yet.

### Final native build status

Both arm64 Debug and Release Xcode builds succeeded after all three changes,
using the normal `snes9x` scheme with `CODE_SIGNING_ALLOWED=NO`. Logs:
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room055-optimization-debug.log`
and the sibling `room055-optimization-release.log`. Neither product was launched.
Full/Metal and renderer-sequence validation remain outstanding at this checkpoint.

Independent step 3 verification accepted the frozen patch: freshly compiled O2
frozen-output, annotation-work and runtime regressions passed. All 50 final
source pins and 256 artifact pins matched. The independent runs retained the
original fingerprint, allocation bound and 18 identity checks in each of the
16 transformed annotation cases. This is scoped optimization acceptance, not
complete-room geometry approval or a live-performance result.

### Frozen full/Metal gate

The final curated full suite, including Metal slot and production shader checks,
passed **83/83** on frozen sources. Manifest:
`build/remaster-tests/20261029T035634Z-fsdwi38i/results.json`.
The actual-renderer differential tool's immutable closures, exact four-file
baseline difference, input/state/profile pins and binary hashes were verified
before GPU execution. Strengthened CPU sequence validation passed both captures
across nine states, including effective mutations and exact switchbacks; GPU
sequence comparisons subsequently passed: both captures across nine states
matched the implementation-start baseline and the forced-full candidate
byte-for-byte, including actual scene uploads, fields, mesh, transported Direct
and Composite. Evidence: `build/remaster-tests/room055-optimization-final/gpu-results/summary.json`.
This closes the scoped renderer correctness gate, not live-performance validation.

## Current measured work

The lighting-enabled ordinary-session CURRENT A/D traces in
`build/remaster-tests/room055-matched-apps/state-hook-v2/acquisition-20261028-194058/`
each contain 187 renderer-completed observations. Exclusive CPU medians:

| Stage | A (ms) | D (ms) |
| --- | ---: | ---: |
| Visible faces | 6.331 | 6.206 |
| Placement application | 1.929 | 1.917 |
| Frame finalization | 1.827 | 1.783 |
| Scene colors / HUD restoration | 1.576 | 1.588 |
| Field update, unchanged inputs | 0.862 | 0.861 |
| Field update, reason 368 | 6.200 | 6.246 |
| Field update, reason 880 | 6.061 | 6.119 |
| Offscreen sources | 1.079 | 1.054 |
| Mesh comparison / update / result copy | 0.322 | 0.314 |
| Mesh emission patch | 0.039 | 0.039 |
| Derived-normal copy | 0.032 | 0.032 |
| GPU resource uploads | 0.078 | 0.088 |

These stages are not independently additive to inclusive refresh or GPU time.
Reason masks are correlated input differences, not causal isolation. Changed
groups select all 57,344 pixels; geometry updates are 98 or 3,707–3,709 pixels.
Small geometry update counts do not establish unchanged donors elsewhere.

## Recommended order

### 1. Index invocation-local HUD classification

`macosx/mac-render.mm:631–636` copies scene colors and checks every pixel with
`S9xRemasterHeldLampHudPixel`. For valid non-BG3 occurrences that helper performs
a linear metadata search (`frame.h:5848`) and material search (`held_lamp.h:117–121`).
The field-cache pixel loop repeats the same classification when fields/emission
change. Index original metadata and material decisions once per invocation, then
classify occurrences without repeated searches. Preserve original first-match
semantics, empty-selector fallback, BG3 early acceptance, ROM/dimension guards,
invalid occurrences and out-of-range tile pixels. Do not substitute resolved
geometry metadata: this predicate currently consumes original asset metadata.

This is the lowest-risk first candidate: it requires no cross-frame cache and
does not alter source quality, geometry, ownership or lighting. A test-only
prototype must include index construction in its timings and compare complete
scene-color outputs on real captures plus adversarial synthetic cases.

The test-only prototype in `remaster/tests/held_lamp_hud_lookup_test.cpp` now
passes that experiment. Two 256x224 captures, 300 alternating-order pairs each,
including fresh index construction/destruction and full scene-color preparation:

| Capture | Original median | Indexed median | Saved in this pass |
| --- | ---: | ---: | ---: |
| room055_006 | 1.480 ms | 0.127 ms | 1.353 ms |
| room055_007 | 1.418 ms | 0.126 ms | 1.292 ms |

An independent repeat saved 1.283–1.356 ms. All predicates and all 114,688 output
bytes per capture matched; ASan/UBSan and existing held-lamp checks passed.
Duplicate-key negative controls reject last-match indexing. Real captured HUD
colors already equal their replacements, so contrasting-color synthetic cases
separately prove actual replacement behavior. Index storage is about 59 KB for
3,703 instances plus temporary maps. Evidence and complete reproduction:
`build/remaster-tests/room055-hud-lookup/REPORT.md`.

This was a measured offline prototype improvement, not a live frame/FPS result.
At that point neither consumer had changed. Production integration subsequently
passed the step 1 review recorded above; matching live validation remains needed.

### 2. Reduce invocation-local visible-face annotation work

`dungeon_visible_face_runtime.h:506–567` walks scene, main, sub and observed BG
rasters. It repeats placement checks and clone-key lookups per eligible pixel.
Both CURRENT traces publish exactly 1,568 clones on every completed frame.

Memoizing invariant checks for an instance/room-cell pair within one invocation
is safer than retaining ownership across frames. Pixel-coordinate and flip
correspondence, band/door masks, selected donors and publication still require
per-pixel checks. Preserve traversal and exact clone creation order; vector
growth must not invalidate borrowed references. First measure annotation versus
graph construction in a bounded differential replay benchmark.

The existing candidate cache already hits on every measured frame. Recognition
alone is about 0.058 ms, so a bigger recognition cache is not the main target.

### 3. Exact local field reselection and independent emission updates

`lighting_field_cache.h:84–209` turns changes to instance/context/geometry inputs,
metadata presence, any of seven rasters, or observed depths into whole-frame
selection. The per-pixel geometry test runs only after that selection. This is
a concrete source of avoidable work if unaffected dependencies can be proven.

A conservative first design can compare old/new references at every offset and
reselect only offsets with changed references or changed candidate instances.
It need not initially build a persistent reverse index. Dependencies include
unselected fallback candidates, not merely cached donors: either BG can supply
missing channels, and HUD depth ordering can change the visible occurrence.
Identity changes, resolved channel presence, terminal OOB claims, material/HUD
policy, context changes and old/new index reuse must all be covered.

Separate selection dependencies (presence/order/identity) from channel contents
and emission/color dependencies. Updating a height value need not inherently
choose a new donor; animating emission must still refresh its actual pixels.
Retain full recomputation on global/unsupported changes. There is no justified
blanket exemption for animation, palette changes or small updated-pixel counts.

### 4. Retain static visible-face graph preparation with a complete key

Even on candidate-cache hits, wall bands, regions and contacts are reconstructed
(`dungeon_visible_face_runtime.h:253–350`). Static relief scans and neighbor
relations are candidates for reuse, including duplicated wall-edge sampling.
Their individual cost is not yet measured.

The existing candidate-equality key is insufficient. Any retained subgraph must
cover every consumed placement/neighbor field, including presence, relief,
transforms, ambiguity, templates, obstacle/diagonal flags, door direction/masks
and candidate/section identity. Keep exposure, occupied-floor ownership, plane
offsets, signed solving, contradictory bases, OOB visibility and annotation
frame-local unless separately proven. Missing proof means full reconstruction.

## Lower priorities and non-solutions

- Mesh result copying and unchanged-sample scanning are real, but the combined
  measured stage is only about 0.3 ms; emission patching and normal copying are
  about 0.04/0.03 ms. Prioritize larger costs before changing their ownership ABI.
- Resource uploads already use exact changed-row caches per in-flight slot.
  A single global upload generation would be unsafe when a slot lags frames.
- Placement application and finalization remain secondary profiling targets:
  projection already caches cell decisions, but still assembles frame outputs;
  finalization allocates/copies raster planes and applies profile data each frame.
  Do not bypass current ownership or borrow mutable frame state asynchronously.
- GPU visibility traversal remains a separate candidate from earlier exact
  replay probes. It already has conservative 4x4 block bounds. Disabling shadows,
  dropping hidden sources or reducing chosen quality is not an exact optimization.
  Further acceleration needs shadow/output parity and cadence-controlled timings.

## Validation contract

The available capture pin manifest contains 28 files for four captures, not 28
captures. All 28 file hashes were rechecked during implementation preparation.
Historical parity manifests locate inputs; their results do not validate a new
patch. Source-preserving EndFrame replay must restore observed BG/depth planes
and prove palette witnesses before checking exact hidden sources. The generic
`LightingReplayInput::run` currently omits those observations and is insufficient
for that assertion.

Final renderer checks must read actual transported Direct, not the emission seed
in the legacy `.direct` probe dump. The existing actual-renderer wrapper
`build/remaster-tests/room055-perf006-engineer.mm` reads `directMean[0]` for the
supported zero-bounce, directional-off, non-GI-metrics path. It handles immutable
packets, not EndFrame reconstruction or changed-frame sequences; those require
additional differential validation. No unsupported case may be counted as a pass.

Implementation-preparation checks also passed the 17 general Python tool tests,
including expected negative controls in the performance-comparison fixture.
Profile Controls layout checks passed 12 cases with one native AppKit case
skipped in the default invocation; that is not a fresh native-layout validation.
No broad ALTTP authoring-tool discovery or height-coverage approval is implied.

Compare against forced-full fields and the unchanged visible-face path, including
complete ordered occurrences, claims, contexts, all rasters/depths, donor triples,
all field arrays, normals, mesh and final renderer outputs. Tests must cover both
BGs/planes, transforms, authored zero versus absence, fallback appearance/removal,
HUD/OBJ boundaries, depth ties, door masks, profile edits, camera motion, artwork
switchback, index reuse and vector growth. Include cache/index construction in
benchmarks; report cold, unchanged and changed cases separately.

Existing selection-cache mutation and common-consumer integration regressions
were rerun successfully during this investigation. The repair expert also reran
visible-face source/precedence/frozen-output checks and exact006 source-preserving
replay (192/192 hidden sources). These validate the existing path, not a proposed
optimization. Slot12 lacks per-frame packet/palette snapshots; a realized live
gain and animation correctness still require post-implementation measurements.
