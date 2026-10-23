# HUD-covered world geometry

The verified USA ALTTP independent ownership pass now covers ordinary dungeon
gameplay (including stairs/door submodules) and dungeon dialogue as well as the
existing settled overworld contexts. Interface/menu/map modes remain excluded.
Unsupported PPU modes/mosaic and forced blank still discard the scene plane.

The geometry-only pass uses stock main-screen enables, windows, priority,
transparency and scroll, removing only BG3. It does not union main/sub enables
or publish its raster colors. `sceneRgb555` remains exactly `originalRgb555`;
the optional SCN1 ownership plane supplies the world underneath HUD ink. The
existing settled dark-room lamp path retains its cone-free colors but now also
omits BG3 from its independent world pass. HUD presentation stays stock.

Existing field/mesh preparation retains underlying geometry as blockers and in
Mesh view while HUD pixels remain nonparticipating receivers/sources. No height
arrays, normals, floor solvers or profiles are authored/changed. Unknown relief
still has its existing provisional/unknown semantics. This is not complete-room
height approval, offscreen geometry, or recovery from older captures without SCN1.
Old captures require recapture.

Sprite assemblies now consume main, sub and scene ownership observations together.
This makes the temporary EndFrame main/scene swap and paused profile refresh
equivalent; otherwise the new independent draw records left stock instances with
different support from scene instances.

## Validation (October 22, 2026)

- Debug arm64 app builds: `build/hud-geometry-debug.log`.
- `hud_shadow_test.cpp`: BG1/BG2 signed placement/relief, canonical flipped pixels,
  hidden/uncovered exact mesh, cache updates, unknown/BG3 rejection and provisional
  ground. Optional capture arguments assert identical scene/stock RGB, nonzero
  recovered HUD geometry and byte-identical hidden/uncovered complete mesh.
- `lamp_scene_test.cpp` (compile with `-ffunction-sections -fdata-sections
  -Wl,-dead_strip`): dungeon/dialogue/menu gates, stock color/depth isolation,
  scene lifecycle, capture round trip and lamp color recovery. Held-lamp and
  field-cache tests also pass.
- `sprite_obstacle_test.cpp`: distinct stock/scene records resolve identically
  before/after plane swap, alongside existing assembly/support regressions.
- Production Metal test: 1393/1393 checks, 456 reference comparisons and 65,536
  patch probes; `build/hud-metal-lighting-test.log`.
- Native production framework benchmark with freeze005, current profile,
  `S9X_REMASTER_SPRITE_MOVEMENT_REGRESSION=1`, 3 warmup/9 measured frames:
  stationary `build/hud-geometry-native-fixed`, left movement
  `build/hud-geometry-native-left`. Both pass per-frame sprite/paused-refresh
  invariance. Stationary capture recovers 1,814 HUD-covered world pixels and
  exactly matches uncovered mesh/stock RGB. These short instrumented runs are
  correctness checks, not representative performance measurements.
- `git diff --check` passes. Manual Mesh-view, actual stair/door/dialogue traversal,
  and fresh dark-lamp visual review remain pending. Specialist delegation was
  attempted twice but unavailable due to harness model-variant configuration.
