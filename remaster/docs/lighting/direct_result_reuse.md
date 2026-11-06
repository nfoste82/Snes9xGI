# Exact Direct result reuse

Each completion-gated Metal resource slot retains its most recent deterministic
Direct result. Unchanged frames skip Direct transport, reduction and in-place
directional addition. Emission preparation and indirect lighting still run.
The indirect accumulation seed is explicitly reset on a hit; otherwise an earlier
even bounce could leak accumulated radiance into the next frame.

Reuse keys cover every effective input:

- geometry, occlusion, height/known state, surface and finite mesh;
- emission, participation, opposite-facing and reflectance fields;
- ordered effective source records, emitter patches, hidden samples and lights;
- receiver RGB albedo and Direct-affecting uniforms, source layout and serial mode;
- dimensions, device and resource slot.

Uniform identity conservatively includes presentation settings; only the RNG
seed is excluded because exhaustive prepared Direct is deterministic. Sampled
Direct, GI measurement and diagnostic views bypass reuse. Allocation/dimension
changes invalidate it. Validity is revoked before encoding and published only
after successful command-buffer completion; abandoned or failed work cannot
publish a reusable result. `S9X_REMASTER_DISABLE_DIRECT_REUSE=1` forces the exact
recompute control. There is no temporal averaging or approximate source removal.

## Owner-accepted performance tradeoff

The owner explicitly accepted reuse on 2026-11-04 despite native dense reps1
GPU p95 increasing **13.83→17.48 ms** and refresh p95 **37.69→44.65 ms**.
Native dense GPU medians dropped approximately **12.73→0.186 ms**, **19.12→0.128
ms**, and **34.20→0.088 ms** at representatives 1/3/8. Recompute frames still
exceed the 5 ms worst-case CPU+GPU target; these are not additive CPU/GPU timings.
Tails coincide with nine-frame source-animation invalidation. Offscreen continuous
and periodic recompute controls do not reproduce the native penalty, so clock/
scheduling attribution remains unproved. Historical baseline-v2 is immutable;
an owner-accepted new baseline must be recorded separately through the stress
runner, not created by editing a prior report.

The owner-accepted record is local `build/performance-stress-suite/baseline-v3-reuse.json`,
created through the normal runner for all 18 pinned workloads with two processes
each. `baseline-v2.json` remains unchanged. Production revalidation passed 31
captures/eight outputs, both synthetic and pinned 23-case sanitizer regressions,
and the 52,016-check focused oracle. The clean curated gate passed 126/128;
only the known `visible_face_work` allocation and `tile_type_overlay` assertions
failed. The additional Metal regression accounts for the new test count.

## Rejected follow-up: incremental keys and complete-state LRU (2026-11-05)

A 16-entry per-slot LRU keyed by incremental 128-bit SipHash row digests (no
purge on geometry change) reached 92% hits on the dense `.008` route and cut its
reps1 GPU p95 18.35→13.25 ms with exact parity. It was reverted: screen-space
fields change every row on scroll, so digest maintenance raised CPU setup p95
about 1.0→2.7 ms on movement/exit routes (15 pinned stress regressions) while
those routes rarely hit. Evidence: local `build/performance-stress-suite/reuse-keys/`.
Per-ray/room-space visibility caching is the investigated successor.

Regression: `remaster/tests/direct_reuse_metal_test.mm` exercises the actual
renderer with 23 fixture-free input/bypass comparisons and optional pinned capture
arguments. The curated `--metal` gate includes it. Historical evidence and frozen
proposals remain under local `build/performance-stress-suite/many-lights-opt/`.
