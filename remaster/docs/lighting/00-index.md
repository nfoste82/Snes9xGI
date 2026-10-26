# Lighting and Performance

Renderer contracts, emitter sources and deferred performance work. Commands and
inline paths use the repository root unless stated otherwise. Return to the
[primary index](../../../AIDocs/00-index.md).

## Lighting and source contracts

- [Triangle Mesh Occlusion](lighting.md): Connected surfaces, finite shells, wall closure, source transport, acceleration and diagnostics.
- [Emission Depth and Wall Closure](../../../AIDocs/10-emission-depth-wall-closure.md): Emission authoring and historical jail/table regression evidence.
- [Held Lamp Scene](lamp_scene.md): Lamp-free scene capture, HUD preservation, replay and recapture requirements.
- [Offscreen Room Emitters](offscreen_emitters.md): Complete-room physical source discovery, held-lamp state and source continuity.
- [Area Directional Lighting](area_lighting.md): Room/world directional lights, defaults, Area Settings and capture state.

## Performance and deferred plans

- [Indirect Lighting Replacement Plan](../../../AIDocs/09-indirect-lighting-replacement-plan.md): Sampled transport, reflected-light boost and the deferred 5 ms target.
- [Live Lighting Benchmark](../../../macosx/remaster-benchmark.md): Actual application benchmark setup, scripted movement, CPU/GPU reports and captures.

The 5 ms indirect-lighting target still stands, but correct heights for every
dungeon room and overworld area take priority (owner confirmed 2026-10-26).
Historical timings and proposed optimizations are not instructions to resume
performance work ahead of coverage. Benchmark documentation remains beside its
macOS implementation; local-only plans remain under `AIDocs/`.
