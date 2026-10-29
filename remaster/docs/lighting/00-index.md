# Lighting and Performance

Renderer contracts, emitter sources and deferred performance work. Commands and
inline paths use the repository root unless stated otherwise. Return to the
[primary index](../../../AIDocs/00-index.md).

## Lighting and source contracts

- [Triangle Mesh Occlusion](lighting.md): Connected surfaces, finite shells, wall closure, source transport, acceleration and diagnostics.
- [Emission Depth and Wall Closure](../../../AIDocs/10-emission-depth-wall-closure.md): Emission authoring and historical jail/table regression evidence.
- [Held Lamp Scene](lamp_scene.md): Lamp-free scene capture, HUD preservation, replay and recapture requirements.
- [Offscreen Room Emitters](offscreen_emitters.md): Complete-room physical source discovery, held-lamp state and source continuity.
- [Offscreen Representative Quality](offscreen_representatives.md): Independent default/area hidden-source quality, inheritance, HUD scope and capture compatibility.
- [Area Lighting and Scene Settings](area_lighting.md): Room/world directional lights, per-area reflected-light boost, defaults, Area Settings and capture state.

## Performance and deferred plans

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
