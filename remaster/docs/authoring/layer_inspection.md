# Background layer inspection

macOS View > Inspect Layers (Cmd–Shift–K) cycles All layers → BG1 only → BG2
only → All layers. Remaster Lighting moves from Cmd–Shift–L to Cmd–Shift–S.
The existing Remaster Debug surface-diagnostic cycle remains in the View menu
without a shortcut; GI debug remains Cmd–Shift–D.

Isolation is a real stock-rasterizer redraw, not filtering the already-visible
ownership packet. The chosen BG is drawn on the main inspection target if enabled
on either PPU main or subscreen; OBJ and the other backgrounds are excluded.
Color math is disabled for raw background artwork. User BG_Forced toggles are
honored, never overwritten. Default mode uses the unchanged ordinary renderer.
The independent ALTTP HUD/lamp scene pass is disabled while isolated so stale
scene ownership cannot substitute a different layer during inspection.

Layer changes latch at frame boundaries. While paused, cycling renders one
emulated frame and remains paused; this can advance animation/game state by one
frame. Picks are blocked during the refresh, old selections are cleared, and
the existing paused producer-owned capture path supplies the new ownership.
Cycling invalidates queued old-layer capture callbacks; picks remain blocked
until the producer has rendered the latest requested layer, including rapid cycles.
Layer visibility is independent of lighting/debug selection: Cmd–Shift–D cycles
Height, Normals and other GI views on the selected background, and Cmd–Shift–S
toggles lighting there too. Changing layers preserves the active diagnostic and
lighting enable. Live click-opened packets preserve surface diagnostics as well.
The overlay identifies the active isolated layer and GI view. Profile edits are published
before generating a new frame but no authored metadata is changed by cycling.

This is for a running game or an inspector packet retained from that live game.
Opening an arbitrary file replay cannot expose hidden layers; cycling is disabled
there. Resume the game first. Switching games resets inspection to All layers.

There is no 33% translucent mode: it requires separately retained complete layer
targets and matching picking, which existing captures do not store. No guessed
hidden colors or duplicate geometry are synthesized.

SNES modes provide up to four BGs plus OBJ. This initial cycle deliberately
offers BG1/BG2 only; it is not a claim those correspond to gameplay floors.
Mode7 raster isolation follows the existing renderer and EXTBG enable, but its
tile identity/picking remains unsupported, as do existing hires/other unsupported
ownership paths. No new generalized Mode7 or BG3/BG4 authoring is implied.

Background diagnostics use `(x, y, BGn)` with zero-based 8px room/world tile
coordinates. Dynamic sprites use OBJ/OAM instances; BG coordinates do not
identify them. Some moving objects are background tilemap updates, so motion
alone does not determine the source layer.

## Tests

Selection highlighting uses the same composed main/subscreen tile lookup as
click picking. In All layers, a BG1 tile can be a subscreen contributor through
main-screen backdrop; it must still highlight. Hidden subscreen tiles and the
independent lighting scene do not override the clicked visible occurrence.
This applies to both tile-type and Tile Locations selection.

- `remaster/tests/layer_view_test.cpp`: exhaustive enable/forced-mask combinations,
  subscreen-only backgrounds, OBJ exclusion, default equivalence and frame latch.
- `remaster/tests/tile_selection_highlight_test.cpp`: both BGs on main/subscreen,
  visible-pick equivalence, hidden/invalid ownership rejection and lighting-scene
  independence (also checked with ASan/UBSan).
- `macosx/remaster-inspector-layout-test.py`: shortcut assignment, fresh-frame
  ownership/picking guards and disabled independent scene pass.
- Opt-in native production raster test: environment
  `S9X_REMASTER_LAYER_INSPECTION_TEST=0|1|2` with the existing live benchmark.
  Isolated modes require nonempty chosen-BG main ownership, no other-layer/OBJ
  winners, no subscreen contributors and no independent scene pixels.

This is an inspection feature, not a dungeon-height repair or coverage approval.
Height/Normals describe the isolated layer's available geometry; lighting,
occlusion and indirect transport operate on a partial scene and must not be
interpreted as physically complete-room lighting.
Fresh interactive shortcut/pause/click/context-assignment review remains required.

Initial validation: 22 inspector source/layout tests, 14 tooling Python tests,
exhaustive layer-mask regression and its ASan/UBSan build pass. arm64 Debug and
Release builds pass (`build/layer-inspection-{debug,release}-build.log`).
The optimized layer-mask regression and ASan/UBSan context-location publication
regression also pass. The canonical authored profile remains unchanged.
Existing context binding and rendered-scroll regressions pass as well.
The opt-in native freeze/raster regression could not complete on this desktop:
the application launch timed out after120seconds before producing a report.
No live raster/GUI success is claimed from that attempt.
The first full+Metal gate passed33cases before the final lamp-recovery exclusion
changed a source fingerprint; that run is invalid, not a complete pass. The
stable final gate is rerun separately under `build/layer-inspection-final-tests/`.
The final stable full+Metal gate passes **36/36**:
`build/layer-inspection-final-tests/20261023T215919Z-fey8hqoy/results.json`.
Native interactive verification remains pending despite the passing suite.

Follow-up debug coordination: removed the UI's isolated-layer lighting guards
and layer-cycle resets. Layer changes now preserve GI enable, GI view and surface
debug mode; live retained-packet opening preserves the surface mode too. Added
source regressions for these independent controls (23 inspector checks pass).
Follow-up arm64 Release/Debug builds both pass
(`build/layer-debug-coordination-{release,debug}-build.log`); layer-mask and
sanitized context-publication regressions plus14tooling Python tests pass.
Native interactive verification remains pending; core raster/shader sources were
unchanged by this follow-up.

## Manual acceptance checklist

1. Open a live ALTTP room with a profile, lighting off. Cmd–Shift–S toggles
   lighting; Cmd–Shift–L no longer owns this action.
2. Cmd–Shift–K exposes BG1 without BG2/OBJ/HUD; overlay says BG1 only. Click
   exposed artwork and confirm source Background/0 and Room tile BG1.
3. Cycle again; only BG2 remains. Click and confirm Background/1 and BG2.
4. Select Height/Normals with Cmd–Shift–D and cycle all layers; the diagnostic
   and lighting enable persist. Cmd–Shift–S works in isolation. Returning to All
   restores the original composition without resetting the chosen debug view.
5. Repeat while paused, including from a live click-opened inspector. Each
   change remains paused after one frame; old selection is cleared and a new
   click belongs to the new layer. Context assignment/Undo still work.
6. Open a file-only capture and confirm layer cycling is disabled rather than
   silently inspecting an unrelated live state. Resume to regain live cycling.
7. Open another game and confirm All layers is restored. Existing user background
   disable toggles must not be overwritten by the inspection cycle.
