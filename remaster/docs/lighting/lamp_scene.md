# Pre-cone lamp scene capture

## Dialogue-state continuity

The USA adapter also accepts indoor module `$0e`, submodule `$02` (RenderText),
with saved world module `$010c == 7`. Both the held-lamp producer and raw-color
capture use this context; the independent scene pass accepts every RenderText
subsubmodule stage. Normal dungeon scene capture still requires settled module
7/submodule 0/subsubmodule 0. Inventory/map/other interface states and overworld
dialogue are not newly enabled. This follows the local reference engine's
`messaging.c` interface dispatch and `variables.h` saved-module address.

Previously all three integration gates required module 7, so opening dialogue
removed both the physical lamp and its cone-free scene. BG3 HUD/dialogue pixels
still bypass lighting using stock ownership/color; Original is unchanged.
Held-lamp context tests cover all 256 interface substates and 256 text stages,
and plotter/recovery tests cover dialogue BG3 ownership with cone-free room color.
Portable tests (including scene ASan/UBSan), arm64 Debug/Release builds and
`git diff --check` pass. Fresh live dialogue opening/closing has not been checked.

`S9xUpdateScreen` performs an independent bounded scene pass for packet-active,
SHA-verified USA ALTTP, settled indoor module 7, lamp `$0458` enabled, ordinary
Mode 1 without interlace/hires or BG2/BG3 mosaic. It reuses the actual tile/OAM
rasterizer with separate RGB/depth/owner buffers, rendering BG2/BG3/OBJ and
backdrop from the union of main/sub enables. BG1 is omitted entirely: its scrolled
cone never enters scene geometry. The pixel plotter writes `RealScreenColors`
instead of color math; independent full-width clip segments remove darkness
windows, restricted to active spans for mid-line re-rendering. Palette, scroll,
flips, BG/OBJ depth and transparency follow current partial-flush renderer state.
Stock buffers and PPU registers are untouched. Working renderer targets and
palette state are restored after the pass.

Scene buffers reset at BeginFrame, redraw affected spans, and publish only when
every displayed pixel has supported ownership. Unsupported modes/transitions,
forced blank or partially captured frames discard the optional scene. Existing
placement/actor metadata is applied to scene ownership before publication.

Frame v28 adds an optional trailing `SCN1` appendix: per displayed pixel RGB555,
owner, shared-table instance ID and canonical tile pixel (11 bytes/pixel).
No appendix means stock fallback; the reader still accepts v1..v27. Metal field,
mesh, albedo and original-scene contribution use the independent scene when
lighting is enabled. Original presentation uses stock RGB; visible BG3 HUD ink
and authored UI pixels throughout the screen retain stock RGB and zero transport
participation. Room pixels behind transparent HUD artwork remain lit; the HUD
exemption must never cover that entire rectangular band.

Scene-color restoration and changed field preparation use an invocation-local
`RemasterHeldLampHudLookup` to avoid repeated per-pixel metadata/material scans.
It preserves the original predicate's first-match asset/material behavior, BG3
ink guard and canonical selector/fallback decisions. It borrows an immutable
frame and must not survive metadata, instance or raster mutation; no cross-frame
HUD cache is introduced. The original scalar predicate remains the test oracle.
See [optimization results](room055_optimization_opportunities.md) for scoped
parity checks and construction-inclusive CPU measurements.

Dialogue letters on BG3 receive the same exemption as HUD ink, including unknown
font hashes. Transparent spaces between letters retain the room's lighting.
Future scene-light accents on dark UI/text borders should use a separate
screen-space UI effect with independent receiver/occluder masks; the current UI
bypass neither casts world shadows nor depends on room heights.

The exact legacy `frame_lamp_light.s9xrmf` is v21, 256x224: main BG2 has 55,083
pixels (48,462 black), sub BG1 cone has 7,224 pixels. It contains neither a raw
palette nor pre-math scene, so it remains readable but needs recapture to recover
missing room colors. This path does not guess colors from other tiles.

## Checks and limits

`lamp_scene_test.cpp` exercises the actual normal-width plotter with dark math,
stock isolation, depth/transparency, scene finalization, unsupported coverage,
optional appendix round trip/malformed rejection and exact legacy lamp evidence.
Build with `-std=c++17 -D__MACOSX__ -DHAVE_STDINT_H -Wl,-dead_strip` (the test
includes the plotter implementation; dead stripping removes unused renderer
variants). Optimized and ASan/UBSan variants pass. Core and macOS frontend syntax
checks pass. The integrated arm64 Debug Xcode build and Metal harness (1,308
checks) pass. GUI validation has not been performed.
The shared `remaster_test.cpp`, held-lamp and offscreen-emitter tests pass.

This adapter excludes all BG1 artwork while the lamp is enabled, removes layer
windows, and retains PPU palette brightness/fades. It targets the verified dark
room composition, not arbitrary alternate BG1 room compositions. Unsupported
render modes and transition packets fall back to stock. Extra pass cost has not
been measured live. It adds viewport scene capture, not full-room occluders.
No room-height generation or validation is performed.

## Missing-scene recovery (room 041)

The supplied v28 `frame_room041.s9xrmf` contains one held lamp but no `SCN1`
appendix: its main ownership is complete BG2/BG3/OBJ, with the 7,224-pixel cone
on subscreen BG1. Falling back to post-math RGB therefore retains the hard cone.
Lamp-active packet frames now also retain winning main pre-math colors. If the
independent scene is missing, verified lamp state and complete main ownership
restricted to BG2/BG3/OBJ/backdrop allow publication of those raw colors with
matching ownership. BG1-owned or unsupported pixels reject recovery. Ordinary
room, Original, and HUD RGB are preserved. Mosaic-enable bits with size 1 are
no-ops and no longer reject the independent pass. The existing room041 packet
has no raw palette and needs recapture; changing contribution alone cannot repair
its already-blackened artwork.

## Reverted ordinary-room world pass

The HUD experiment that extended this pass to ordinary indoor rooms was reverted
after live room061 showed missing torch pedestals/west door and a blue checker
floor. Combining main/sub layer enables and removing color math does not preserve
ordinary ALTTP room composition. Stair submodules disabled the experiment, which
explained the reported artwork changes on stair traversal. The lamp-only gate,
BG2/BG3/OBJ mask, HUD field exemption and original shader presentation are restored.
Room060 captures 052/053 showed the earlier HUD sphere fallback does publish
sources, but its approximation changes illumination. Exact hidden-source capture
must preserve stock layer/window/color-math composition and be validated against
actual ordinary-room frames before another world pass is enabled.

The replacement repair now uses source-only `hiddenEmitters` and prepared Direct
samples; it does not enable another world pass. See `offscreen_emitters.md` for
the exact HUD capture pair, source-only appendix, GPU checks and remaining
hidden-geometry/representative-clustering limits.
