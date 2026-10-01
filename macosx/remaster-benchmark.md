# Live application lighting benchmark

Run from an unlocked macOS desktop session. The runner opens the specified ROM,
loads its matching profile, restores the save state on the emulator thread before
each case, and ignores physical controller/keyboard input during the benchmark.
It exits automatically with status 0 on completion, 2 on app-side failure.

```sh
python3 tools/run_remaster_benchmark.py \
  --app "$HOME/Library/Developer/Xcode/DerivedData/snes9x-atcnlahonwpbthgwbglgovfsrdlq/Build/Products/Release/Snes9x.app" \
  --output build/benchmark-slot004-right \
  --move right --seconds 3
```

Defaults use `~/Downloads/Zelda_ALTTP.zip`,
`~/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.004.frz`, and the
repository's `alttp-profile.toml`. Override with `--rom`, `--state`, `--profile`.
The output directory must be new. `--connections` overrides the profile setting.

Cases: Direct lighting only; Direct + Indirect at 0/1/8 bounces; Composite at
0/1/8 bounces. The profile supplies other lighting settings. The debug sphere is
disabled. Audio output is muted automatically, including after each save-state
restore. GPU diagnostic/readback environment variables are disabled by the runner.

`--move none|left|right` holds the direction for the measured emulated frames;
warmup (default 0.5 emulated seconds) has no input. Each case restores the state
and resets the shader random seed. End-state RAM hashes verify case consistency.
Whether movement actually scrolls depends on the selected save state's location;
the script cannot force scrolling through an obstructing wall.

`--presentation deterministic` (default) renders every emulated frame and waits
for completion: identical animation/movement frames are compared across cases.
Three emulated seconds means 180 measured frames at 60 Hz, and can take minutes
of wall time when rendering is slow. `--presentation live` keeps normal bounded
asynchronous presentation and frame dropping; its submitted frame subset depends
on performance. This mode is useful for real gameplay throughput, but not
pixel-exact comparisons. Both force frame skip 1 and disable sound synchronization.

`report.json` contains input hashes, executable/build identity, hardware, settings,
per-frame GPU and matched field-preparation timings, throughput, dropped counts,
and median/p95/p99 summaries. Emulator refresh/pacing values are explicitly latest
samples and can refer to a previous frame. Frames/s is GPU-completion throughput,
not a display-present callback measurement. GPU duration includes all command
buffer graphics. `gpu_stages_ms` additionally isolates compute encoders using GPU
timestamp counters on devices supporting stage-boundary sampling (macOS 11+).
Repeated `source_power` and `indirect_transport` intervals are summed per frame;
each stage's statistics include a sample count. Empty stage dictionaries mean
counters were unavailable. These intervals exclude render/present and encoder
gaps; sampling can itself affect performance. PNGs and `.s9xrmf` captures
are written after each case outside its timed interval. Reports are checkpointed
after each case so completed results survive interruption. Benchmark shutdown
does not save benchmark-driven SRAM.

## Verified baseline (October 1, 2026)

M3 Max, Release build, slot 004, repository profile (96 connections), right held
for 180 measured frames after 30 warmup frames. All seven cases had 180 completed
samples and matching final RAM hashes. The final image confirms Link moved to
the right and the scene scrolled. GPU median/p95 milliseconds:

| Case | Median | p95 |
| --- | ---: | ---: |
| Direct only | 230.03 | 477.31 |
| Direct + Indirect, 0 | 229.46 | 475.98 |
| Direct + Indirect, 1 | 401.73 | 693.50 |
| Direct + Indirect, 8 | 448.10 | 821.98 |
| Composite, 0 | 225.67 | 475.47 |
| Composite, 1 | 270.53 | 534.23 |
| Composite, 8 | 422.80 | 652.28 |

Scene-field medians were 4.84–6.94 ms; mesh medians 2.92–4.09 ms.
These are whole-command-buffer timings from serial deterministic presentation,
not isolated pass timings. The direct+indirect and composite one-bounce variation
needs repeated profiling before interpreting it as a view-specific cost.
The complete run took roughly ten wall-clock minutes. A separate live-mode smoke
run completed all seven cases with normal dropped frames and matching RAM hashes.

### Direct-shader specialization

The production direct pass uses a Metal function constant to compile out radial
indirect quadrature and diagnostic branches. Visibility/diagnostic/reference
passes retain the unspecialized variant and existing triangle intersection path.
Analytic flat-patch and early-triangle-rejection experiments were removed after
live measurements regressed; neither changes the final geometry path.

The same muted, right-moving, 180-frame/96-connection run then measured:

| Case | GPU median | GPU p95 |
| --- | ---: | ---: |
| Direct only | 84.81 | 168.93 |
| Direct + Indirect, 0 | 83.59 | 169.72 |
| Direct + Indirect, 1 | 125.37 | 207.61 |
| Direct + Indirect, 8 | 252.33 | 343.69 |
| Composite, 0 | 84.36 | 170.40 |
| Composite, 1 | 126.88 | 210.16 |
| Composite, 8 | 256.19 | 344.61 |

All cases completed 180 samples with matching RAM hashes. The lighting suite
passed 1215/1215 checks. Synthetic full-frame direct comparisons differed in
5–7 half-float channels per scene (of 229,376); every difference was within one
half-float step, maximum relative error below 0.00086. Specialization can change
compiler rounding, so direct comparisons enforce that explicit tolerance rather
than bit identity. Synthetic open-scene timings did not consistently improve;
this optimization is validated against the authored slot-004 workload, not a
claim of universal speedup. Earlier/later live results also include machine load
and thermal variability and are not simultaneous A/B measurements.

The app also accepts these options directly:
`--remaster-benchmark --benchmark-rom PATH --benchmark-state PATH
--benchmark-profile PATH --benchmark-output PATH --benchmark-seconds 3
--benchmark-warmup-seconds 0.5 --benchmark-move right
--benchmark-presentation deterministic --benchmark-connections 8`.

## Session handoff: resume GPU optimization

Read `AIDocs/00-index.md` first, then this note and `remaster/lighting.md`.
The user wants GPU frame time addressed before CPU scene fields. Benchmark mode
and direct-source preparation/specialization are implemented; CPU field caching,
buffer reuse, and material-resolution optimization have not yet been implemented.
The prior preparation/specialization work is committed as `dc3fafc1`; the
visibility/counter follow-up below is currently uncommitted. Inspect `git status`
and diff before continuing, and preserve the current working tree.

Relevant code:
- `tools/run_remaster_benchmark.py`: launch/setup and report summary.
- `macosx/mac-os.mm`: benchmark state machine, save-state reset, input sequence,
  report output, and automatic audio mute.
- `macosx/remaster-benchmark.h`: producing-frame tags and completion sample ABI.
- `macosx/mac-render.mm`: GPU source preparation, specialized pipeline creation,
  frame-tagged timing collection, draining, screenshots, and mesh/wait metrics.
- `macosx/shaders.metal`: source preparation and direct function constant;
  `remasterVisibility` and `remasterMeshHit` are the next profiling targets.
- `macosx/remaster-lighting-test.mm`: production shader regression suite.
- `macosx/remaster-lighting-benchmark.mm`: synthetic direct comparison (`--direct`).

Next steps:
1. Use the new isolated GPU stage timings for the authored room, and obtain finer
   visibility traversal/intersection counters or a controlled captured-frame
   microbenchmark before further algorithm changes. Verify traversal,
   triangle intersection, and emitter enumeration costs before another algorithm
   change. Direct currently enumerates every emissive sample for every receiver;
   zero indirect bounces does not avoid direct transport.
2. Reduce receiver × emitter visibility work, preserving fractional opacity,
   geometric endpoints, emission-depth energy, and current mesh semantics. Consider
   tighter conservative acceleration or source reuse; do not assume analytic flat
   patch paths are faster (the attempted shortcuts regressed and were removed).
3. Compare identical slot-004 movement sequences and captured outputs; retain
   reference visibility comparisons and the documented half-float tolerance.
4. Address scene fields after the GPU work: distinguish waits from actual work,
   reuse scratch storage, pre-resolve materials, remove redundant gradient normal
   work, then consider geometry caching with correct movement/edit invalidation.

No room-height metadata was generated or approved in this performance work. If
future changes involve ALTTP room-height/floor/wall placement generation or
validation, follow the repository's required room-height-expert delegation.

Validation commands (temporary parent already exists):

```sh
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-test.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-fast-test
xcrun clang++ -std=c++17 -O2 -fobjc-arc -Wall -Wextra \
  macosx/remaster-lighting-benchmark.mm -framework Foundation -framework Metal \
  -o /var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark
/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visibility-specialized-benchmark --direct
xcodebuild -quiet -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination "platform=macOS,arch=arm64" \
  CODE_SIGNING_ALLOWED=NO build
git diff --check
```

Raw reports/images/captures from this session are local temporary artifacts:
- Baseline: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-slot004-right-3s/`
- Final specialization: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-direct-specialization-only-3s/`
- Rejected visibility experiments: `/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/live-benchmark-gpu-specialized-right-3s/`

Temporary artifacts may be cleared by macOS; the tables and reproduction
instructions above are the durable record. `AIDocs/` is intentionally local-only
and ignored, while this note and the benchmark tools can be tracked normally.

## Visibility follow-up (October 1, 2026)

Retained changes:
- 4×4 height-envelope blocks instead of 8×8. All allocation/build/query paths,
  including standalone tests, use the same size. Pixel DDA rounding is preserved.
- Stream two successive triangle-edge vertices instead of dynamically indexing
  an array of four. Same triangles/test order, no analytic geometry shortcut.
  The reference macro retains the original array expansion.
- Benchmark-only stage-boundary Metal timestamp counters; each producing frame
  owns its sample buffer through GPU completion.
- `tools/compare_remaster_benchmarks.py` checks input identity, final RAM, original
  pixels, measured counts, PNG byte identity, and prints median/stage comparisons.

Short screening runs (6 warmup + 15 right-moving measurement frames): original
Direct median 153.78 ms; 4×4 alone 145.88; 4×4 + streamed edges 119.69; streamed
edges + 8×8 143.64. Smaller 4×8/8×4 transport threadgroups did not convincingly
improve performance; default stays 8×8. 2×2 blocks regressed to 196.62 ms and were
removed. Explicit four-edge loop unrolling made no convincing difference and was
also removed. Short initial-room timing is not comparable to the full scrolling
sequence below.

Full right-moving run: 30 warmup + 180 measured frames, 96 connections, muted,
deterministic, M3 Max. Same inputs as the preceding specialization benchmark.
All seven PNG files are byte-identical to that benchmark and to the 8×8 streamed
edge control; final RAM and original RGB555 hashes match across runs.

| Case | Earlier specialization median | 8×8 streamed-edge control | Final 4×4 + streamed edges median/p95 |
| --- | ---: | ---: | ---: |
| Direct only | 84.81 | 79.05 | 66.55 / 134.75 |
| Direct + Indirect, 0 | 83.59 | 79.58 | 67.21 / 131.54 |
| Direct + Indirect, 1 | 125.37 | 123.88 | 105.20 / 163.03 |
| Direct + Indirect, 8 | 252.33 | 250.56 | 226.38 / 306.44 |
| Composite, 0 | 84.36 | 79.69 | 67.93 / 126.35 |
| Composite, 1 | 126.88 | 123.70 | 104.85 / 180.85 |
| Composite, 8 | 256.19 | 269.35 | 230.30 / 349.76 |

Timings are milliseconds, sequential runs rather than interleaved A/B; thermal
and machine-load variation remains. PNG identity checks the final case image,
not every measured frame's float radiance. The independent lighting suite passes
1215/1215, including 456 visibility comparisons in float/half formats. Synthetic
direct checks still have 5–7 half-channel differences, all within the documented
one-half-step tolerance. Synthetic timing remains variable and is not evidence
of universal speedup. Release build and `git diff --check` pass.

Direct transport's final median stage duration is 66.49 ms. At one bounce, direct
is 66.04 and sampled indirect 36.88 ms; source-power construction is 0.049 ms,
envelopes 0.027 ms, preparation 0.006 ms, compositing 0.008 ms. At eight bounces
direct is 67.57 and summed indirect 154.90 ms. Thus receiver/source transport
remains the next target, rather than power-tree reductions or compositing. Even
zero bounces remains far above the 16.7 ms 60 FPS budget.

Raw full reports/captures are in the local temporary parent listed above:
- `gpu-edge8-right-3s/`: streamed-edge 8×8 control.
- `gpu-edge4-right-3s/`: final 4×4 + streamed-edge full sequence.
- `gpu-stage-8x8/`, `gpu-stage-4x8/`, `gpu-stage-8x4/`, `gpu-stage-block4/`,
  `gpu-stage-block4-unroll/`, `gpu-stage-block2/`, `gpu-stage-block4-edge/`,
  `gpu-stage-block8-edge/`: short screening runs.

Compare two completed sequences:
```sh
python3 tools/compare_remaster_benchmarks.py BEFORE_OUTPUT AFTER_OUTPUT
```

Optional benchmark-only execution-layout experiment:
`S9X_REMASTER_TRANSPORT_THREADS=8x4 python3 tools/run_remaster_benchmark.py ...`.
The report records the requested override; shader/source enumeration is unchanged.

Final-build repeat (including the independent array-based reference and 512-entry
counter buffer) reproduced the improvement, with all 180 frames/case and all seven
PNGs byte-identical to the preceding specialization baseline:

| Case | Final-build median | p95 |
| --- | ---: | ---: |
| Direct only | 66.39 | 126.06 |
| Direct + Indirect, 0 | 66.32 | 123.97 |
| Direct + Indirect, 1 | 105.38 | 163.22 |
| Direct + Indirect, 8 | 225.54 | 300.26 |
| Composite, 0 | 66.57 | 128.84 |
| Composite, 1 | 105.00 | 162.59 |
| Composite, 8 | 227.91 | 299.25 |

Raw final-build repeat: `gpu-edge4-final-right-3s/` under the same temporary parent.
All stage counts are 180. Direct stage median 66.31 ms; one-bounce indirect 37.52;
eight-bounce summed indirect 157.35. Use this report as the next baseline.

## Parallel transport follow-up (October 1, 2026)

The user's live screenshot showed 671.64 ms GPU, 323.87 ms drawable wait, and
37.54 ms CPU lighting with 9 connections/zero bounces. Its old panel text did not
include the newer mesh/wait/emitter rows. Process inspection found the running
app at the DerivedData **Debug** path; the earlier measured app was **Release**.
The screenshot's exact scene/build/environment was not captured, so it is not
an A/B measurement against the benchmark. Both configurations are rebuilt now;
the running process must be restarted to load them.

Retained changes: eight-element source/connection GPU batches plus float32 partial
reduction, existing geometry/visibility and sampling seeds, 64 MiB per-slot cap
with serial fallback, and removal of gradient-normal calculations always replaced
by mesh normals. Direct still enumerates every authored emitter even at zero
bounces; the connection slider controls indirect work. Stage counters now also
run when the live metrics checkbox is enabled. The panel reports build timestamp
and measured direct/indirect GPU intervals.

Rejected experiments: barycentric early rejection offered little gain; a fan-plane
prefilter regressed; reversing rays failed 10 sphere/reference checks (including
offscreen endpoints). All three were removed. Batch screening: 32 sources/batch
about 50 ms initial-room Direct, 8 about 45 ms, 4 about 43 ms but twice the partial
storage. Eight is retained. Full scrolling workload is cheaper than the initial
room, so do not compare screening medians to the full sequence.

Full Release, 30 warmup + 180 right-moving frames, 96 connections, deterministic:

| Case | Previous serial median | Batched median | Batched p95 |
| --- | ---: | ---: | ---: |
| Direct only | 66.39 | 22.00 | 50.91 |
| Direct + Indirect, 0 | 66.32 | 21.88 | 50.24 |
| Direct + Indirect, 1 | 105.38 | 37.97 | 74.50 |
| Direct + Indirect, 8 | 225.54 | 101.60 | 157.43 |
| Composite, 0 | 66.57 | 22.69 | 52.66 |
| Composite, 1 | 105.00 | 38.06 | 84.89 |
| Composite, 8 | 227.91 | 107.86 | 168.54 |

All cases completed 180 samples, matched final RAM/original pixels. PNGs are exact
for five cases. At eight bounces, Direct + Indirect differs in 14 8-bit channels
and Composite in 3, all by exactly one byte value. Synthetic direct differs in
42–53 half-float channels per 229,376-channel scene, all within one half-float
step (maximum relative difference below 0.000975). Serial and batched regression
suites each pass 1215/1215. Summation grouping changes rounding; later power-tree
proposals can respond to small radiance changes. Do not claim bit-identical GI.

Release **live/asynchronous** mode, 9 connections, three emulated seconds/case:
GPU medians 53.03, 48.20, 60.14, 62.25, 53.38, 62.01, 60.46 ms; p95
145.92, 138.73, 157.08, 127.10, 152.51, 146.93, 118.48. Completed throughput
24–35 frames/s with drops. Direct stage medians 23–30 ms. The remaining elapsed
time needs queue/scheduling profiling: inter-encoder gaps and concurrent workloads
may contribute; stage sums need not equal whole-frame elapsed time.
Debug short deterministic screening at 9 connections measured Direct 45–48 ms,
CPU fields about 35 ms (mesh about 23 ms); Release CPU fields about 3–6 ms.
This does not achieve a 16.7 ms total budget in either mode.

Next targets: control asynchronous encoder/queue scheduling overhead, parallelize
or accelerate the remaining visibility work (without failed ray reversal), then
reduce CPU mesh allocations/material-resolution cost. Profile both deterministic
and live modes. Avoid measuring concurrent standalone GPU tests alongside the
app; other running GPU apps also introduce variability.

Artifacts under the same local temporary parent:
- `gpu-batched-final-right-3s/`: new deterministic baseline, both transports batched.
- `gpu-batch8-right-3s/`: Direct-only batching; later cases had machine-load noise.
- `gpu-batched-live-9/`: asynchronous live report, 9 connections.
- `gpu-batched-debug-smoke/`: rebuilt Debug screening, 9 connections.

Verification and comparisons:
```sh
S9X_REMASTER_TEST_BATCHED=1 /path/to/remaster-lighting-test
S9X_REMASTER_TEST_BATCHED=1 /path/to/remaster-lighting-benchmark --direct
python3 tools/compare_remaster_benchmarks.py BEFORE_OUTPUT AFTER_OUTPUT
swift tools/compare_remaster_images.swift BEFORE_OUTPUT AFTER_OUTPUT
```
`S9X_REMASTER_SERIAL_DIRECT=1` / `S9X_REMASTER_SERIAL_INDIRECT=1` force serial
transport for controlled A/B. Reports record these overrides. Changes remain
uncommitted; preserve the working tree. Restart the tested Release app at
`~/Library/Developer/Xcode/DerivedData/snes9x-atcnlahonwpbthgwbglgovfsrdlq/Build/Products/Release/Snes9x.app`
for gameplay performance comparisons.

## Resolved geometry cache follow-up (October 1, 2026)

User confirmed Release timings and set a target of approximately **5 ms total
lighting overhead**, with emulator and older-PC headroom. Implemented CPU geometry
reuse and per-instance material-selector resolution. `surface_mesh_cache.h` compares
resolved samples; dirty outputs expand three pixels and 16×16 output tiles rebuild
with an additional three-pixel input halo through the original mesh builder.
More than half the tiles dirty, dimension changes, or quantum changes use a full
build. Cache access is serialized by `renderMutex`. Emission depth/authored shading
normals refresh every frame, and GPU uploads remain complete for each resource slot.

This requires no static metadata: moving walls change effective heights/domains/
coverage at their old/new positions and invalidate naturally. Current viewport
boundaries match full reconstruction. Retaining whole meshes offscreen is deferred:
the frame lacks stable world-space object mapping and authoritative offscreen
changes. Retained history must not invent blockers or weld against offscreen pixels.

Sequential full three-second deterministic runs, 96 connections:

| Case | Cached GPU median/p95 | Cached fields/mesh | Full-build fields/mesh |
| --- | ---: | ---: | ---: |
| Direct | 21.76 / 49.09 | 2.09 / 0.78 | 3.14 / 2.13 |
| Direct + Indirect 0 | 22.08 / 49.28 | 2.06 / 0.77 | 3.25 / 2.20 |
| Direct + Indirect 1 | 36.26 / 71.68 | 2.26 / 0.83 | 4.68 / 3.18 |
| Direct + Indirect 8 | 96.35 / 152.78 | 2.49 / 0.92 | 4.45 / 3.00 |
| Composite 0 | 22.00 / 49.15 | 1.76 / 0.67 | 3.15 / 2.12 |
| Composite 1 | 36.11 / 70.19 | 2.24 / 0.83 | 3.17 / 2.13 |
| Composite 8 | 97.97 / 157.54 | 1.73 / 0.67 | 5.37 / 3.63 |

All 180 samples/case, matching end RAM. All seven final PNGs are decoded-pixel
identical both to the previous batched baseline and forced full-mesh control.
GPU timing differences reflect run/load variation; **no GPU acceleration retained**.
CPU cache versus forced-full uses the same material pre-resolution implementation.

Rejected visibility experiments (both passed 1215 regressions): a second 16×16
envelope carried in RGBA alongside 4×4 bounds cost ~0.15 ms to construct and
raised Direct to ~25 ms; checking the last interior cell against the empty-block
proof measured ~23.4 ms versus ~21.8 after removal. Both removed. Future hierarchy
work should avoid repeated coarse reductions and per-ray 15-step boundary math.

Final live run at 9 connections: GPU medians in case order **31.40, 33.59, 40.24,
54.13, 31.79, 41.16, 55.15 ms**; CPU fields 2.90–3.17 ms, mesh 0.98–1.08 ms;
throughput 27–39 completed frames/s. Sample counts 144/146/136/104/148/139/106.
Live timings are variable and are not a controlled cache-versus-full live A/B.
Still well above 5 ms. No manual moving-wall-room playthrough performed.

Artifacts in the existing temp parent:
- `gpu-cache-final-right-3s/`: retained CPU changes, original GPU traversal.
- `gpu-full-mesh-control-right-3s/`: `S9X_REMASTER_FULL_MESH=1` control.
- `gpu-cache-final-live-9/`: final asynchronous 9-connection run.
- `gpu-cache-hierarchy-right-3s/`, `gpu-cache-fine-right-3s/`: rejected GPU screens.

Verification: mesh suite 35/35 (240-frame exact full/cache differential sequence),
serial and batched Metal suites 1215/1215 each, Debug/Release builds, diff check.
Reports now record `full_mesh_override`. New cache header and prior comparison
tools are untracked; preserve them along with all other uncommitted work.
Next: more efficient visibility representation/traversal, CPU scratch reuse,
then authoritative world-space cache only if placement/state inputs support it.
