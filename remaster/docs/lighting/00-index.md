# Lighting and Performance

Renderer contracts, emitter sources and deferred performance work. Commands and
inline paths use the repository root unless stated otherwise. Return to the
[primary index](../../../AIDocs/00-index.md).

## Lighting and source contracts

- [Triangle Mesh Occlusion](lighting.md): Connected surfaces, finite shells, wall closure, source transport, acceleration and diagnostics.
- [Conservative Visibility Atlas](direct_visibility_atlas.md): Exact finite-mesh rejection and packed visibility fields, independent GPU oracle and native timing evidence.
- [Emission Depth and Wall Closure](../../../AIDocs/10-emission-depth-wall-closure.md): Emission authoring and historical jail/table regression evidence.
- [Held Lamp Scene](lamp_scene.md): Lamp-free scene capture, HUD preservation, replay and recapture requirements.
- [Offscreen Room Emitters](offscreen_emitters.md): Complete-room physical source discovery, held-lamp state and source continuity.
- [Offscreen Representative Quality](offscreen_representatives.md): Independent default/area hidden-source quality, inheritance, HUD scope and capture compatibility.
- [Area Lighting and Scene Settings](area_lighting.md): Room/world directional lights, per-area reflected-light boost, defaults, Area Settings and capture state.

## Performance and deferred plans

- [Performance Stress Suite](performance_stress_suite.md): Pinned freeze/script scenarios, representative sweeps, observed workload coverage and opt-in Release regression baselines.

- [Inspection CPU Work](inspection_cpu_work.md): Lightweight running inspection, full-consumer boundaries and producer-owned packet capacity reuse.

- [Stair Classification Retention](stair_classification_cache.md): Exact per-artwork placement caching independent of plane/support solves, replay parity and limits.

- [Per-Frame Profiling](per_frame_profiling.md): Bounded gameplay captures, CPU/GPU/presentation lineage, cache reasons, pacing splits and compile-time disable switch.

- [Indirect Lighting Replacement Plan](../../../AIDocs/09-indirect-lighting-replacement-plan.md): Sampled transport, reflected-light boost and the deferred 5 ms target.
- [Live Lighting Benchmark](../../../macosx/remaster-benchmark.md): Actual application benchmark setup, scripted movement, CPU/GPU reports and captures.
- [Room011 Live CPU Profile](room011_live_cpu_profile.md): October28 live representative1/3 comparison, exact-output visible-face CPU optimization, captured-state parity and remaining timing limits.
- [Room055 Live Performance](room055_live_performance.md): Hidden/offscreen depth samples dominate despite fewer visible emitters; live evidence, offline representative comparison and replay limitations.
- [Room055 Optimization Opportunities](room055_optimization_opportunities.md): Current CPU costs, indexed HUD classification, visible-face work and exact local field reselection; safety and validation requirements.
- [Exact Scrolling Construction](room055_scrolling_optimization.md): Native switchback causality, occurrence presence and exact connected-vertex welding.

The 5 ms indirect-lighting target still stands, but correct heights for every
dungeon room and overworld area take priority (owner confirmed 2026-10-26).
Historical timings and proposed optimizations are not instructions to resume
performance work ahead of coverage. Benchmark documentation remains beside its
macOS implementation; local-only plans remain under `AIDocs/`.
