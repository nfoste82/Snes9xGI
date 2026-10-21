# Dungeon dialog placement-height lifecycle repair

## Cause and shared fix

`frame_room002.s9xrmf` records module14/submodule2 (RenderText), complete
room inputs, but `publishable_inputs=false` and `applied_map_room=65535`.
The module7-only gate stopped applying both runtime placement maps, exposing
raw authored relief. This is a dungeon-dialog lifecycle class, not room002
metadata or a profile authoring problem.

The live adapter now copies USA WRAM `$010c` (saved world module). Only verified
indoor module14/2 with saved module7 and complete maps/graphics/attributes may
use the ordinary exact-input height path. Menus, overworld text, unknown saved
modules, unsupported collision modes and partial room construction still reject.
Consumed room-entry support is preserved across text suspension, subject to
existing ROM/profile invalidation. Text handler2 never becomes a door-scroll
observation. Authored arrays and solver/classifier rules are unchanged.

Capture JSON adds `saved_module`; old evidence defaults to unknown, not7.
The existing held-lamp adapter already documents and uses this RenderText state.

## Verification

`dungeon_dialog_height_test.cpp`, C++17 O2 and O0 Debug: passes production
EndFrame application for distinct BG1/BG2 bases24/40 and local relief, complete
map invariance, entry-support preservation, dialog return, collision edit
invalidation, and negative lifecycle gates. Synthetic application values are
explicit test output, not proposed room geometry.

`dungeon_geometry_cache_capture_probe.cpp` adds opt-in
`ALTTP_DIALOG_HEIGHT_REGRESSION=1`. On room002's own profile and complete inputs:

- serialized profile FNV1a64 `e917f8d19e770143`, unchanged after replay;
- 83,413 BG pixel observations match exact maps, VRAM, flips and camera;
- dialog/settled/reentry effective BG geometry and complete two-BG inventories equal;
- 21,559 pixel usages receive nonzero bases or placement-local geometry;
- BG1 walls solved566/unknown96/conflicts0; BG2 solved750/unknown0/conflicts0;
- each BG reports required support planes[0,3272], missing[0,0].

The historical capture omitted `$010c`. The probe explicitly supplies saved7
for the controlled lifecycle comparison and labels this assumption; it does not
rewrite or pretend to attest the historical capture. New native captures retain
the byte. Artifacts: `$TMPDIR/opencode/room002-dialog.{json,inventory,s9xrmf}`.

The comparison intentionally excludes module-specific actor/emitter behavior;
it compares each BG pixel's actual base and effective local source and complete
map inventories, rather than requiring whole-frame serialization equality.

All78 available local indoor evidence sets were inventoried:66 passed the
controlled dialog/settled/reentry BG-geometry and complete-map comparison,
10 were rejected by the existing exact packet/maps/camera check, and2 lacked
complete inputs. These are lifecycle ablations on available captured inputs,
not observed dialog traces or exhaustive game-room/state geometry approval.
Detailed records: `$TMPDIR/opencode/dialog-corpus/summary.json`.

Existing transition-cache, entry-lifecycle, departure-geometry, geometry and
held-lamp regressions pass. arm64 macOS Debug build passes.

## Limits / approval boundary

This repairs actual runtime height application, not only the debug overlay.
No complete-room validation-v2 approval is claimed: matching ALTPPD1/ALTPAT1
and a pinned complete movement graph were not constructed for room002 here.
The runtime support counts above are not a substitute for that gate. BG1's
96 unknown walls remain an independent shared assembly/support evidence gap;
the dialog repair does not invent geometry for them. Fresh GUI dialog testing
and a new capture with saved-module attestation remain necessary for interactive
confirmation. No ROM, profile, freeze or user capture is changed.
