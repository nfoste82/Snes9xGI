# Room 0x011 live CPU/GPU investigation — October 28, 2026

## Outcome

Lowering representative emitters reduces Direct GPU work, but leaves substantial
producer-thread remaster assembly. No production optimization or pacing change
was made in this investigation. No room-height coverage approval is implied.

The running Release process was sampled without restarting or moving Link.
Build label: October 28 13:47:35; framework mtime 13:47:53; process launched14:00.
The dirty working source is not independently identical to the loaded binary.
With user permission, the area representative slider was changed1 ->3 ->1;
the final value1 was verified. No profile save was performed. Directional light
was already enabled and was left unchanged; indirect bounces were0.

## Live evidence

Each A/B phase collected an8-second `sample` and20 read-only Accessibility panel
observations. Observations are latest-value snapshots, not a producing-frame
trace, and sampling itself perturbs execution. Do not interpret instantaneous
FPS readings as interval-average throughput.

| Observation | Representatives3 | Representatives1 (restored) |
| --- | ---: | ---: |
| Direct samples |288–300|104|
| Direct GPU median |12.26 ms|6.805 ms|
| GPU frame median |16.735 ms|10.065 ms|
| Scene fields median / maximum |3.52 /9.75 ms|3.405 /9.78 ms|
| Presentation queue median |0.02 ms|0.02 ms|
| Drawable median / maximum |0.02 /4.12 ms|0.02 /0.04 ms|
| EndFrame assembly / engine-thread samples |4125/6473|4129/6503|
| DrawRemasterFrame / engine-thread samples |1183/6473|1205/6503|
| S9xSyncSpeed / engine-thread samples |459/6473|483/6503|

EndFrame and DrawRemasterFrame are both nested in EndScreenRefresh; their
percentages must not be added to EndScreenRefresh. These are sampled stack
observations, not measured per-frame milliseconds or recoverable-time promises.

The initial passive reps1 sample also showed30 FPS on most panel observations,
scene fields2.995 ms median/8.63 ms maximum, negligible drawable waits, and
3703/5824 engine-thread samples in EndFrame. The later A/B panel FPS alternated
20/30/60; a clean presented-frame interval trace is still needed for throughput.

## Accounting and hotspots

`gfx.cpp` starts `emulationFrameMs` at StartScreenRefresh and samples it at the
end of EndScreenRefresh. It includes packet/room assembly, scene fields and GPU
setup. `cpuexec.cpp` calls SyncSpeed afterward, so pacing sleep is separate.
Skipped frames update refresh time too: observed refresh values range from0.19
to33.72 ms. Consequently a tiny skipped-frame latest value must not be compared
with a previous rendered frame's GPU/field values. GPU and CPU stages overlap;
their milliseconds are not an additive wall-frame budget.

Specialist review of the initial sample found2289/3703 EndFrame observations at
the visible-face call (`remaster.h:3736` in the loaded build). Within it:

- 915 observations annotating scene/main/sub/observed-BG rasters, prominently
  per-pixel ordered-map clone lookup;
- 495 in all-channel OOB visibility selection;
- 328 constructing and123 destroying the full occurrence metadata resolver;
- 173 selecting height donors for exposure.

Scene-field spikes are real in the latest-value observations, but their exact
causes are not established. Field selection currently conservatively reselects
the entire raster after composition or instance changes. Flame animation can
therefore invalidate broad work even though most room geometry is unchanged.

## Next implementation boundary

Optimize invocation-local visible-face work first: share exact visible-occurrence
policy without computing all seven channel donors for OOB visibility, use compact
effective role/height inputs instead of full metadata copies, and replace repeated
ordered-map clone lookup with an exact-key fast lookup while preserving clone
order. Do not retain instance pointers across vector growth. Validate unchanged
complete rasters, claims, HUD/depth precedence, both BGs, flips, unresolved states,
context changes and missing-versus-zero authored sources against the existing
path. Cross-frame visibility caching needs explicit camera/raster/context/owner
invalidation; existing room-template caches do not establish that equivalence.

Raw local artifacts: `build/remaster-tests/room011-live-sample.txt`,
`room011-live-reps3-sample.txt`, `room011-live-reps1-restored-sample.txt`,
`room011-live-metrics.jsonl`, `room011-reps3-metrics.jsonl`, and
`room011-reps1-restored-metrics.jsonl`.
