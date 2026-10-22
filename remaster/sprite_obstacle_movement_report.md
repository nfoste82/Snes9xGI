# Sprite obstacle scrolling ownership (October 22)

## Cause

Production EndFrame read WRAM `$e2/$e8`, actor state, and OAM staging
`$0800/$0a20` after the game had already prepared its next frame. The pixels and
PPU OBJ slots still described the displayed frame. With left movement the trace
first showed PPU/background camera640 and WRAM camera638; the staging OAM was
two pixels ahead, and the final-PPU comparison correctly invalidated its size.
All24 footprint occurrences still matched; losing exact actor ownership exposed
both valid gameplay planes, so the assembly became Conflict and Height purple.
Stopping movement let the producer/display observations coincide again.

An initial frame-start WRAM-OAM snapshot was insufficient: the producer clears
the staging buffer to y240 after DMA. Only the latched PPU OBJ entries are the
displayed OAM. This was reproduced in the real emulator with freeze005, not
inferred solely from settled captures025/026 (both restore1518 pixels correctly).

## Repair

- `gfx.cpp` pins the actor/camera observations at `S9xStartScreenRefresh` and
  records the DMA-latched PPU entries, not next-frame staging RAM. EndFrame
  consumes these value-owned observations and still checks final PPU identity.
  Terrain actors share the same epoch as their camera/OAM. No ownership radius,
  guessed Link plane, cached previous solution, hash or room exception is added.
- `macosx/mac-os.mm` opt-in `S9X_REMASTER_SPRITE_MOVEMENT_REGRESSION=1` validates
  every actual completed movement frame: complete resolved claims, no unclaimed
  members, and exact effective height/normal fields after paused refresh.
- `remaster/sprite_obstacle.h` opt-in `S9X_REMASTER_SPRITE_OBSTACLE_TRACE=PATH`
  records assembly and producer/camera/OAM evidence without changing matching.
- `macosx/sprite-obstacle-movement-probe.mm` drives the existing native benchmark
  through the built framework, avoiding AppDelegate workspace restoration. Only
  inspector-control refresh is stubbed (no inspector exists in the driver).
  ROM, emulation, renderer, EndFrame, matcher and Metal remain production paths.

Profile SHA256 remains
`f89aae1760a36a94f88c1164675fc4815148c066e2094dcebb89fc68da8a2fb9`.
No profile or floor-solver changes and no new complete-room approval are claimed.

## Reproduction

```sh
clang++ -std=c++17 -fobjc-arc -framework Cocoa \
  macosx/sprite-obstacle-movement-probe.mm -o build/sprite-obstacle-movement-probe
S9X_REMASTER_DEBUG_PROBE_LIVE_VIEW=height \
S9X_REMASTER_SPRITE_MOVEMENT_REGRESSION=1 \
S9X_REMASTER_SPRITE_OBSTACLE_TRACE="$PWD/build/movement-trace.txt" \
python3 tools/run_remaster_benchmark.py --app build/sprite-obstacle-movement-probe \
  --state "$HOME/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.005.frz" \
  --output build/new-movement-run --seconds 0.6 --warmup-seconds 0.1 \
  --move left --composite-only
```

Use `--move right` for the other direction and
`S9X_REMASTER_MOVEMENT_APP=build/release/Snes9x.app` for Release.
Build each configuration before running its framework. Outputs include the
complete benchmark report, final packet, PNG and production Height `live.rgba`.

```sh
c++ -std=c++17 -O2 remaster/sprite_obstacle_movement_test.cpp \
  -o build/sprite-obstacle-movement-test
build/sprite-obstacle-movement-test build/new-movement-run/case-05.s9xrmf \
  build/new-movement-run/live.rgba
```

Before-fix native trace: `build/blocker-native-detail-trace.txt`; stationary
frames1..8 resolve, subsequent scrolling frames lose actor ownership and conflict.
After-fix investigation trace: `build/blocker-native-secondfix-trace.txt`; all24
frames retain actor0/plane0/base50 through the same camera movement.

## Validation

- arm64 Debug and Release apps build successfully. Logs:
  `build/blocker-movement-{debug,release}.log`. A concurrent Debug attempt was
  rejected by Xcode's shared build-database lock; its serial rerun passed.
- Both configurations pass left and right scripted movement:6 warmup+36 measured
  production EndFrames each, every frame checked for resolved ownership and
  same-packet refresh invariance. Debug outputs `build/blocker-final-debug-left`
  and `build/blocker-final-debug-right`; Release outputs
  `build/blocker-final-release-left2` and `build/blocker-final-release-right`.
- `sprite_obstacle_movement_test.cpp` checks actual final production Metal Height
  readbacks: left1518 and right1470 visible authored height/normal pixels,
  resolved and grayscale rather than purple, in both configurations. Right's
  denominator differs because of final visible occlusion, not lost support.
- Earlier standalone native-driver runs completed frame assertions/reporting but
  sometimes crashed during `exit()` static destruction under the emulation
  thread. The opt-in regression now uses `_Exit` only after report/readback and
  presentation drain, avoiding an unrelated driver shutdown race. Latest Release
  left/right runs return0. Normal application shutdown behavior is unchanged.
- `git diff --check` passes and the user profile SHA is unchanged. No manual user
  visual acceptance is claimed.
- Full ASan/UBSan + Metal gate passes **23/23** in158.89s, including paused
  handoff, sprite grouping, terrain lifecycle, complete solve/cache lifecycle and
  512/544KiB stack regressions. Result:
  `build/blocker-movement-validation/20261022T173107Z-ji2kb9w2/results.json`.
