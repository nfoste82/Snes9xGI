# Per-frame profiling

## Capture ordinary gameplay

In a profiling-enabled build, choose **View → Capture Per-Frame Performance
(10 Seconds Max)**, then reproduce the stairs, pickup, chest or other action.
There is no restart, settings change, animation recording or GPU readback.
The capture stops at 600 emulated frames or a 10-second wall timeout. Another
request while a capture is owned is rejected rather than overwriting evidence.

JSONL output goes to:

`~/Library/Application Support/Snes9x/Remaster/performance-<UUID>.<ID>.jsonl`

Completion and the exact destination are printed to the app console.
The ordinary on-screen message overlay shows **“10-second perf capture started”**
when accepted and **“Capture completed.”** after successful file writing. Write
failure displays an error instead. Messages are consumed on the emulator thread;
if paused/stopped, the notice appears on the next live frame. They follow the
existing on-screen message visibility/timeout settings.

Pausing,
closing a window, unsupported presentation paths or stopping the game can leave
unfinished rows; those remain explicit evidence, never zero-cost frames. The
menu requires a running, unpaused game; lighting/profile activation is unchanged.
Without remaster presentation, CPU refresh/pacing still records, but renderer
evidence is absent. Resume from a save immediately before an action for repeats.

The older environment trigger is retained:

```sh
S9X_REMASTER_LIVE_TRACE=/absolute/path/stairs \
S9X_REMASTER_LIVE_TRACE_ONLY=1 \
S9X_REMASTER_LIVE_TRACE_FRAMES=600 \
build/debug/Snes9x.app/Contents/MacOS/Snes9x
```

Then request the existing animation capture command (Command-Shift-F). In this
explicit trace-only mode it starts profiling **instead of** recording animation.
Do not use ordinary animation recording as a gameplay baseline: tracking and
serialization are measurable workloads.

## Read the result

```sh
python3 remaster/tools/summarize_live_trace.py /path/to/capture.jsonl
python3 remaster/tools/summarize_live_trace.py /path/to/capture.jsonl \
  --chrome /path/to/new-timeline.json
```

The summary accepts old schema1 and new schema2 captures. It reports worst frames,
median/p95/p99/max timings, stage call counts, room-input invalidation reasons,
classification reuse/rebuild evidence, actual presentation intervals and hitch
counts (>25/50/100 ms). Optional Chrome trace JSON shows refresh, pacing, GPU and
presentation tracks; CPU steady-clock and GPU/display host-uptime clocks are
labelled separately, so do not interpret their absolute alignment as latency.

Each row joins one producing frame ID across:

- Refresh start-to-start period and inclusive refresh duration, including frames
  the emulator deliberately skips rendering.
- Audio synchronization wait, frame-limiter phase, requested sleep and total
  pacing elapsed time. The limiter phase includes bookkeeping/scheduler time,
  not exclusively sleep. A 70 ms audio wait and a 70 ms requested limiter sleep
  are now distinguishable.
- Exclusive CPU scopes and invocation counts: finalization, context location and
  binding, terrain, room classification, door recognition, solves, projection,
  visible faces, actors, offscreen sources, fields, mesh and render setup.
- Input-change reason masks, classification disposition, projection disposition,
  instance count/size, solve epochs, artwork restores and field invalidation.
- Submission queue and drawable waits, GPU duration/status, actual drawable
  presentation timestamp, slot drops, emulator skips and submission failures.

`refresh_inclusive_ms` contains nested CPU scopes: **do not add them together**.
CPU durations are elapsed wall spans (including preemption), not sampled thread
CPU. CPU/GPU overlap and cannot be summed to obtain a wall frame period. A reason
flag proves a compared input changed/reuse was rejected; it does not prove that
input independently caused the whole cost. Existing short circuits are preserved,
so later comparisons may be unobserved: masks are not exhaustive change lists.
No per-frame replay packet is saved.
An incomplete or failed GPU row must not be interpreted as valid zero GPU work.
Presentation FPS is an interval statistic over captured callbacks, not guaranteed
whole-window throughput. The first frame period is absent (zero) by definition.

## Overhead and build switch

`S9X_REMASTER_PROFILING=1` is the development default. Collection is request-only:
bounded in-memory rows, logarithmic ID lookup, CPU scopes only while tracing,
no per-frame disk I/O, no synchronous GPU waits added, and no frame copies for
profiling. A worker writes after collection freezes, outside the callback mutex.
Active tracing still adds timers, mutex joins and callbacks; compare trace-on/off
on the same workload before claiming an optimization or an overhead bound.

GPU stage-boundary counters can significantly perturb performance, so bounded
captures disable them by default even when the metrics panel is enabled.
Opt in with `S9X_REMASTER_PROFILE_GPU_STAGES=1`; resulting per-frame stage timings
are stored separately. `S9X_REMASTER_DISABLE_STAGE_COUNTERS=1` always wins.
Whole-command-buffer GPU duration remains available without stage counters.

For a profiling-free baseline build, define **consistently for every target/TU**:

```sh
xcodebuild -project macosx/snes9x.xcodeproj -scheme snes9x \
  -configuration Release -destination 'platform=macOS,arch=arm64' \
  CODE_SIGNING_ALLOWED=NO \
  'GCC_PREPROCESSOR_DEFINITIONS=$(inherited) S9X_REMASTER_PROFILING=0' build
```

The switch disables capture activation and timer scopes and compiles out the new
dungeon reason comparisons and pacing instrumentation. Existing latest-value
metrics and benchmark instrumentation remain independent controls. Optional
`S9X_REMASTER_PROFILE_REVISION` may contain a quoted build/revision label; rows
also retain compilation date/time. Record source revision, dirty diff, app binary,
profile, save-state identity and settings separately for controlled comparisons;
the label alone does not pin a dirty worktree.
