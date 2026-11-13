# ALttP tools

ALttP-specific scripts, portable utilities, tests, metadata, and investigation
notes live here. Run commands from the repository root, for example:

```sh
python3 -B remaster/tools/ALttP/alttp_tile_semantic_atlas.py --help
python3 -B remaster/tools/ALttP/alttp_atlas_profile.py --help
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_*.py'
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_profile_validate.cpp -o /tmp/alttp-profile-validate
```

Profiles are in `remaster/profiles/ALttP/`. Authored tile semantics are in
`remaster/tools/ALttP/alttp-tile-semantics-v1.json`; generated atlas and inspection
artifacts remain under `build/remaster-geometry/`.

## Experimental world height tuning

Semantic profile saves cache the parsed disk snapshot and parse/validate the
edited profile once, rather than repeatedly parsing unchanged geometry arrays.
The validated result is reused by subsequent refreshes; external profile changes
invalidate it. Revision checks, byte-for-byte disk checks before replacement,
obstacle-group preservation and atomic writes remain enabled. Restart the Python
server after updating it (a browser reload alone does not update server code).

Launch the existing semantic editor with an explicit profile (and your existing
atlas/cache paths):

```sh
python3 -B remaster/tools/ALttP/alttp_tile_semantic_atlas.py \
  --atlas build/remaster-geometry/alttp-atlas.sqlite \
  --profile remaster/profiles/ALttP/alttp-profile.toml --browser "Firefox"
```

In **World map**, select **Light World or Dark World**, then pan/zoom to the area
you want to inspect. **Help & details** is collapsed by default to preserve map
height; expand it for reference coordinates, exact bounds, algorithm explanations,
legends, saved-weight path and selected-placement diagnostics. A compact status
line remains visible; hover for the full message or expand help to read it.
Opening/closing help changes the visible map bounds, so recalculate after choosing
the layout you want. The open panel scrolls internally at a fixed height.

The editor automatically remembers the active **Tile grid / World map** tab and
world in browser-local storage. Each world separately remembers its map position,
zoom, reference tile, reference height and last successfully calculated area.
Refreshing the page or restarting the server restores that workspace; **recalc
heights (prev area)** is available for a saved area without first calculating the
current viewport. Calculated height arrays, tile selections and unsaved edits are
not restored, and no calculation runs automatically. These preferences are not
written into the profile. Use the same browser/profile and server address/port;
clearing site storage resets them. If browser storage is blocked, editing still
works but preferences cannot persist.

World cache v2 uses the displayed phase-0 artwork identity for animated tile
selection, shared metadata and placement counts as well as rendering. Old v1
caches rebuild on startup (requires the ROM and Pillow); restart the server and
reload the page after upgrading so its in-memory manifest and grids match.

In the height overlay, unreached cells with a numeric authored height use that
height's black-to-white shading instead of a magenta checkerboard. Their **Heights**
number is magenta rather than cyan to indicate missing traversal coverage.
Unreached cells without a numeric authored height keep the checkerboard and `?`.
Saving/removing offsets refreshes this display immediately without recalculation;
it never changes the calculation's assignment mask or fills traversal gaps.

Click a tile (preferably known ground), press **Use selected
tile as reference**, and set **Reference height** (default **0**, fractions and
negative values allowed). Enable **Heightmap (raw)** and press **Recalculate
heights**. A reference in the visible area is required: there is no invented zero
and no automatic Light World sand anchoring. Each world retains its own reference
and height for this page session; changing selection alone does not move it.
Non-floor references are allowed with a warning. A magenta cross marks the reference.
**recalc heights (prev area)**, immediately to the right of **Recalculate heights**,
starts disabled and becomes available after a successful calculation. It reuses
the last successful calculation's exact map bounds for the current world, with
current saved metadata, weights, offsets, and reference. Panning or zooming before
pressing it does not change that area. The reference must remain inside the saved
area. Failed, cancelled, or discarded runs do not replace it; a new successful
visible-area calculation does. Both calculation buttons are disabled while running.

The activity indicator next to **Recalculate heights** reports preparation,
worker calculation, verification and rendering with elapsed seconds. **Cancel**
stops the active request without replacing the previous overlay. A successful
calculation automatically enables **Heightmap (raw)** and reports **Complete —
heightmap shown**; it remains possible to toggle the overlay off afterward.
Worker/result-version and rendering errors are displayed rather than silently
leaving a nonworking overlay. Reload the page after updating editor scripts;
the server sends HTML/JavaScript with `Cache-Control: no-store`.

**Scope: Visible area** snapshots the current viewport after pending saves finish,
snaps outward to whole 8×8 cells, and intersects the 512×512 world grid. The UI
shows exact end-exclusive cell bounds; only these cells are sent to the worker.
There is no hidden southwest/full-world computation or automatic pan/zoom.
Cyan dashed edges mark the calculated rectangle, even when it belongs to a previous
view. Outside it is unknown/transparent, not zero. Opacity blends grayscale with
artwork; the legend gives the raw numeric black/white endpoints without clamping.
Select a tile to see its raw height (or **UNSET**) and configured placement-flipped
cardinal-axis contributions. Diagonal crossings use one signed weight instead.
Missing heights inside the rectangle are a **magenta/dark-purple checker**, not
grayscale zero. **Orange** marks conflicting arrivals; the first height is retained.
The status reports assigned, missing and conflicting cell counts separately.
**Wall weights** labels walls at zoom ≥3× independently of the height overlay.
**Height traversal arrows** is a separate checkbox, visible at zoom ≥2× and
independent of the grayscale overlay. Recalculate once after loading this update
to record the incoming steps. Arrowheads point from the source tile toward the
destination: **cyan** marks the first arrival that assigned its height, **white**
marks additional agreeing arrivals, and **red** marks rejected conflicting
arrivals. Multiple incoming neighbors, including diagonals, can point to one tile.
Opposing arrows are offset so both heads remain visible. Directions are actual
source coordinates, not the logical N/S/E/W traversal heading through a detour.
Repeated arrivals from the same neighbor/category are merged; this is not an
animated chronological trace. Blocked/unresolved targets have no computed arrival
arrow. References have no invented parent, though real later arrivals can target
them. Selecting a tile lists incoming source directions for all three categories.
Arrows draw only the visible part of the calculated rectangle, remain scoped to
their world/snapshot, and fade with an explicit STALE legend after edits/view changes.

The right panel's **Experimental wall height weight** accepts integers 0–32
(0 disables the contribution), shared across canonical artwork aliases.
The full/reset button explicitly sets 8; the half button sets 4. Three user-confirmed half-slope families
(`437727f0595cb424`, `a8ec34c0c0e99095`, `bcd3a49452020d15`, plus aliases)
default to 4 only when no override exists; all others default to 8. No optimized
v18/v19 weights are used. Weights save atomically to
`<selected-profile>.height-weights.json`, or `<annotations>.height-weights.json`
without a profile. `--height-weights PATH` overrides that path. The UI displays
the exact save path and save failures. This versioned, ROM/profile-bound sidecar
is **not runtime geometry**, and never adds unsupported fields to the profile.
Defaults are not written on GET/launch. Concurrent/stale writes are rejected;
reload after external changes or profile-marker changes. Existing role/facing
edits retain their normal profile/semantic-sidecar save path.
Weights now use format `alttp-experimental-height-weights-v2` and JSON integers.
Legacy v1 sidecars are interpreted at 8× their original values, exactly once;
the next explicit save writes v2. Reads never rewrite those files. The canonical
sidecar has been converted to v2. Authored placement height offsets are not scaled.

Floor metadata includes a **Water** checkbox, defaulting to false. It remains
`role = floor`, shares the normal alias-family save behavior, and does not change
height traversal or lighting. The flag persists as `water` in semantic JSON and
`tile_semantic_water` in profile assets; native profile load/save preserves it.
Changing away from Floor removes the flag. No existing tiles are automatically
classified as water.

Use **Heights (zoom ≥3×)** to display numeric heights on visible tiles. This
checkbox and **Wall weights** uncheck each other. Saved authored heights appear
immediately in cyan and take precedence over calculated values, including after
an offset edit without recalculating. Otherwise labels show calculated heights,
or gray `0` where no calculation exists (display only, not traversal coverage).
Unreached calculated cells and nonuniform authored fields display `?`; calculated
conflicts display the retained height in orange, and stale calculated labels fade.

**Cmd-click** adds/removes exact map locations. Adding a second location switches
the map highlight from all matching artwork to selected locations only; a normal
click returns to single-tile/type highlighting. Multi-selection replaces the room
preview/provenance/details with compact role/facing and weight controls. Mixed
roles have no highlighted button; mixed weights show `–`. Editing these controls
applies to every selected tile type, preserving independent notes/masks/support
unless a role change makes them inapplicable. Role/weight metadata is still shared
with all copies/aliases of that artwork, not location-specific. Required facings
and corner metadata must be completed before changing selection. Height offsets
apply to every selected location (not other copies of the artwork); mixed scalar
offsets show `–`. Save, **Enter/Return**, and Remove offset operate on the whole
selection in one atomic profile update, preserving independent terrain offsets.
Press **Enter/Return** in that input to
save, exactly like **Save offset**.

**Shift-click** selects the inclusive rectangle between the most recently
selected location and the clicked location, replacing the selection.
**Cmd+Shift-click** adds that rectangle while retaining other selected locations.
The clicked endpoint becomes the anchor for the next Shift-click.

**Option-drag** keeps the map stationary instead of panning.

To isolate context-specific uses of shared artwork (for example, a blank tile
used for water and also non-water surfaces):

1. In **Drag: Pan map** mode, click the desired tile type to highlight its copies.
2. Choose **Drag: Keep selected in box** and drag around the area to retain, or
   **Drag: Remove selected in box** and drag around copies to exclude.
3. The first box converts the highlighted alias-family copies in the **current
   world** into an exact-location selection, then filters it. Other tile types
   inside the rectangle are not added. Further boxes filter only the remaining
   locations; endpoints are inclusive and either drag direction works.
4. Refine with Cmd-click/Cmd+Option-drag as needed, then save an offset or use
   **Bake Current Offset** on those exact locations. Choose **Drag: Pan map** to
   resume normal panning and click a type again to start a new type selection.

Box filtering itself changes no authored data. It does not create contextual
role overrides: role/facing/weight edits still affect shared artwork globally.

**Cmd+Option-drag** toggles exact locations along the mouse path, preserving other
selected locations. Each crossed location toggles only once per drag, including
the starting tile; moving back over it does not undo the change. A new drag can
toggle those locations again. Selection changes still wait for pending edits to
save, and normal dragging, Cmd-click and Shift-click keep their existing behavior.

**Bake Current Offset** (beside Remove offset) saves each selected location's
own numeric **Heights** label as an authored offset in one atomic profile update.
Baking is explicit human approval: conflicting retained values and stale calculated
values are accepted, and successive selections can be baked without recalculation.
The exact display precedence applies: authored height, otherwise calculated height,
otherwise display-default zero. Even default zeros can be explicitly baked; cells
showing `?` cannot. Every scalar must be representable as an integer in
−32767…32767; any invalid cell prevents the whole bake. Independent terrain is preserved: uniform
terrain is subtracted from the scalar being saved to avoid double-counting;
nonuniform fields show `?` and cannot be baked. Saving marks the calculation stale
but does not prevent baking another selection. Snapshot/revision checks still
reject external profile edits or changes occurring while a bake is being prepared.
Clicking Bake with unusable values reports the reason beside the offset controls.

**Adjust by** and **Apply adjustment** add a signed integer to each selected
location's own numeric **Heights** label, then save the new value as an authored
offset in one atomic update. For example, heights 16 and 12 adjusted by `-4`
become 12 and 8, rather than both becoming one common value. Enter/Return in
the adjustment field also applies it. Subsequent adjustments use the newly saved
authored values without requiring recalculation. The same display precedence,
stale/conflict acceptance, `?` rejection, scalar range and terrain preservation
rules as baking apply. This changes only selected locations, not shared artwork.

**Apply to all instances in this world** is unchecked by default and is not
persisted. When checked, Save offset (including Enter), Remove offset, Bake and
Apply adjustment expand their targets to every matching artwork/alias-family
location in the current world, including offscreen copies and other contexts.
For multiple selected types, all copies of each type are included. A confirmation
dialog identifies the action, world and total location count; Cancel saves nothing.
The other world is untouched and terrain remains preserved. Bake/adjust still
use each target's own displayed numeric height and reject the entire operation
if any target is unknown or unrepresentable. Uncheck it to resume exact-location
edits, including box-filtered selections.

Select a world-map cell to edit its **Authored height offset**, then **Save offset**
or **Remove offset**. These are exact Light/Dark World BG2 coordinates, not shared
artwork metadata. Saves use the same `[[overworld_offsets]]` profile records as the
native tile inspector, preserve all local geometry/terrain/normals, and enable
`[overworld_placement] mode = "authored_overworld_offsets"` (schema ≥37).
Unlike the native scalar control, saving **0** keeps an explicit zero entry so it
can reset traversal; **Remove offset** removes that scalar entry. Native profile
units are used directly, with no conversion to wall-weight units.

When traversal reaches an authored coordinate, its saved height replaces the
incoming calculated height and traversal continues from there. Different incoming
heights do not conflict at that coordinate. This does not seed unreachable cells,
bypass unresolved facings/wall-boundary rules, or reconcile conflicts elsewhere.
The authored value also wins if the reference is that coordinate. Uniform per-pixel
terrain heights combine with the scalar; nonuniform fields remain preserved but
stop traversal as unresolved (a tile-level traversal cannot represent 64 heights).
Removing a scalar does not remove independent terrain offsets. Reload after edits
in Snes9x; stale profile revisions reject writes and calculation results.

`diagonal-propagation-v37-1` replaces count/skip with paths crossing explicit diagonal
faces. Cardinal and diagonal walls apply **one saved weight**: climbing adds the
destination wall's weight on entry; descending subtracts the source wall's weight
on exit. Doors and doorways participate as cardinal walls, including saved weights,
placement flips, no perpendicular launches, and cardinal/diagonal boundary stops.
They expose exactly one N/E/S/W facing in the editor (no diagonals or multiple
selection). Changing/saving either role requires that facing. Older entries with
no facing remain loaded and retained, including their masks/support/geometry;
they block height propagation as unresolved rather than being guessed or deleted.
Doorway facing is independent of its existing floor-support direction.
From floor, descending entry leaves the first wall at the floor height;
the final wall's subtraction happens on exit onto floor. Traversal
stops each candidate before crossing directly between a cardinal-facing wall and
a diagonal-facing wall (in either direction). This boundary rule applies to all
four logical headings and wall corners too; it does not block floor entry/exit
or same-type wall crossings. No height or arrival arrow is assigned by a blocked
step; the source tile records a cardinal/diagonal boundary stop. Independent
arrivals and sibling candidates can still proceed. **Eastward target selection**
branches to NE (+1,−1) for a SW/NE-facing wall and SE (+1,+1) for a NW/SE-facing
wall, **plus directly E (+1,0) unless both source and target are diagonal walls**.
Westward checks reciprocally branch to NW (−1,−1) for a SE/NW-facing wall and
SW (−1,+1) for a NE/SW-facing wall, plus directly W unless both tiles are diagonal.
Diagonal-to-diagonal horizontal traversal must move diagonally, never laterally.
Direct-E/W entry from ground/cardinal walls and exit onto non-diagonal tiles remain
allowed. If lateral travel is blocked and no diagonal branch matches, that path
stops; selecting its tile reports the specific blocked-continuation reason.
This applies on floor/cardinal tiles as
well as diagonals, and checks placement-flipped facings. Each matching branch
continues independently; a blocked/conflicting branch does not stop its siblings.
A breadth-first directional worklist expands outward from the reference, retaining
the deterministic first-arrival height. References initially launch N, S, E, W;
eastward candidate order is NE, SE, E; westward is NW, SW, W.
Each coordinate/heading is scheduled at most
once (at most four states per visible cell), avoiding exponential re-traversal
or recursive call-stack growth.
Direct E/W is used regardless of the current tile's facing, subject to the lateral
restriction above. Height changes use the logical traversal axis and the flipped
facing, not the geometric diagonal step's dot product.
Selection never skips a candidate merely because it is assigned or conflicted.
Out-of-bounds candidates are ignored, never wrapped to another row. This change
uses reciprocal destination checks in both horizontal directions; vertical target
selection remains source-facing as before. For northward traversal,
NW/SE faces step NW (−1,−1); NE/SW faces step NE (+1,−1). Eastward west-facing
diagonals rise and east-facing diagonals fall; northward south-facing diagonals
rise and north-facing diagonals fall, each by one weight. All use flipped facings.
North/south paths follow the current diagonal tile's crossing heading;
once on a non-diagonal tile those paths resume their original axis on the new
row/column. Climbing facings: N travel on S-facing walls, S on N-facing walls,
E on W-facing walls, W on E-facing walls; opposite facings descend. For each
step, add the destination's positive signed contribution and the source's negative
signed contribution. Evaluate endpoints independently, including facing transitions;
there is no mutable pending-weight state. Reversing the same edge negates its
height change. A 1/2/1 north-wall sequence between floor heights 0 and 4 therefore
has heights `[0,1,3,4,4]` whether crossed southward or northward from those references.
Different movement routes can still disagree. There is no automatic north-wall
2× multiplier; any larger magnitude comes from the authored saved weight.
Horizontal candidates depend on the destination facing, not assignment state;
conflicting branches still stop without overwriting an existing height.
There are no skipped diagonals, terminal overrides or hidden out-of-bounds reads.
The manual reference supplies an arbitrary height origin, not a physically solved
terrain level. A reached horizontal traversal offers north/south traversals if
the immediately above/below neighbor is unassigned; a reached vertical traversal
offers west/east traversals toward unassigned left/right neighbors. This applies
to westward/southward paths too. The new traversals start at the reached coordinate
and use the normal crossing rules, rather than directly assigning the neighbor
the same height. Travel parallel to cardinal wall faces is **straight-only**:
E/W on N/S-facing walls suppresses both perpendicular launches and diagonal
destination checks; N/S on E/W-facing walls suppresses perpendicular launches.
This is determined from semantic facing, including zero-weight walls, not from
whether a height delta happens to be zero. It applies to walls/corners whose
normals are exclusively on that cardinal axis, not mixed-axis corners or floors.
No wall or wall corner launches perpendicular traversals, including cross-wall
cardinal travel and mixed-axis corners. A crossing keeps its originating heading
through intermediate walls, then resumes perpendicular propagation on reaching a
non-wall tile. This prevents southward entry onto a north-facing wall from
launching west/east along its raised surface (and applies in all four directions).
Existing same-heading crossing candidates and height contributions are
unchanged. Accepted continuation is enqueued before its perpendiculars.
Queued heading states are deduplicated even before processing. More paths can
now compete, so heights may change from the former vertical-first sweep; raw
disagreements remain orange and are not reconciled. Paths stop at
the viewport boundary, unresolved sources/roles/wall facings, ambiguous multiple
diagonal facings, a repeated coordinate/traversal-direction state, or conflicting
arrivals. An already assigned tile with an agreeing height can continue in a new
logical traversal direction (N/S/E/W, retaining that axis through diagonal detours).
An arrival disagreeing by more than 1e-9 flags that tile as a conflict and stops
that path without replacing its height. Agreement retains the existing value,
not the incoming floating-point approximation. Unvisited
tiles remain NaN with an explicit assignment mask; no horizontal/vertical fallback
fills them. The manually pinned reference remains set even if its metadata is unknown.
Selecting a tile reports processed traversal directions and recorded stop reasons:
view boundary, direction already processed, unresolved metadata, or conflicting
height. Several reasons can coexist. An unset tile with no recorded stop was not
reached; that does not imply a metadata defect at that tile itself.
Traversal is **path-dependent**: changing reference placement/column can change
the field, not just its offset. There is **no smoothing,
fitting, reconciliation, clamping or plateau/sand repair**. Raw negative values
and discontinuities are intentional inspection output. Multiple normals cancel
by sum-sign, and artwork reuse/context-dependent roles are unresolved limitations.
Unknown roles/sources/facings are counted and block propagation, not treated as
validated zero-height surfaces. Known nonwall roles retain zero contribution.
Dark World uses its own placements/flips and explicit reference, never Light World
anchor coordinates. The pure worker retains an explicit `legacy-sand` mode for
the original reference preset (Light World only, original sand rows492–511 in the
southwest); it is not the editor's default or an implicit fallback.

Recalculation snapshots saved semantic annotations/weights and runs in a local
Web Worker; navigation and editing remain responsive. Edits immediately mark an
existing result **STALE**. Pan/zoom/resize marks stale only if the snapped bounds
change; repeated redraws do not. Changes during a calculation discard its result;
external revisions are checked before accepting it. Recalculate after edits.
No experiment directories, external libraries or CDNs are production dependencies.

Focused regressions (temporary profiles/sidecars only):

```sh
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_tile_*.py'
```

Keep local profile backups out of Git; the canonical profile is versioned already.
Historical snapshots removed during cleanup remain available with `git show`:

```sh
git show e99b2841:Profiles/ALttP/alttp-profile_backup8.toml > build/alttp-profile-backup8.toml
git show e99b2841:Profiles/ALttP/alttp-profile.toml > build/alttp-profile-backup9.toml
git show 15f012b4:Profiles/ALttP/alttp-profile.toml > build/alttp-profile-pre-artwork-emission.toml
```

Backups 9 and 10 were identical. The emission migration tool and its tests are
still supported; its `.toml.bak` safety snapshots are local-only.

Standalone C++ tests and audit tools are not app build inputs. Keep assertion-bearing
regressions and shared replay/corpus harnesses even when they are outside the curated
`remaster/tools/run_remaster_tests.py` suite. One-off hardcoded diagnostic dumpers can be
retired without deleting their historical reports. For profile canonical round-trip
checks, use `alttp_profile_validate.cpp` rather than a separate diagonal checker.

`dungeon_static_assembly.h` is offline declaration/effect research shared by
`alttp_static_assembly_inventory.cpp` and its ROM regressions. It is deliberately
not included by the live frame/runtime headers and cannot authorize geometry.

For assembled NPC poses, facing/animation browsing, canonical tile editing and
image-derived default normals/heights, see [NPC-WORKBENCH.md](NPC-WORKBENCH.md).

Historical notes and generated reports may refer to the former `remaster/tools/alttp…`
paths. Use `remaster/tools/ALttP/alttp…` for those files now. Python module names remain
unchanged; their sibling imports resolve when scripts are run directly.
# Explicit boundary gradients

In the world editor, expand **Author gradient between two boundaries**:

1. Select the tiles along one boundary; click **Capture selection as A**.
2. Select the other boundary; click **Capture selection as B**.
3. Enter their absolute traversal heights (for example **48** and **32**).
4. Optionally select a target region and enable **Fill current selection only**.
5. Click **Preview gradient**, inspect the purple cells/height labels, then
   **Save preview** and confirm. Recalculate afterward to inspect arrivals.

The tool interpolates by Euclidean distance to the captured tile samples and
rounds to integer heights. It fills only their convex hull, optionally intersected
with the target selection. Capture dense boundaries; points are not automatically
connected as polylines. Concave shapes should use a target selection. This is
explicit surface authoring, not automatic conflict reconciliation or traversal
coverage. Existing scalar offsets are preserved unless **Replace existing offsets**
is enabled; use replacement to correct boundary values as well as the interior.
The preview reports how many existing cells are preserved. Nonuniform terrain
rejects the preview atomically; uniform terrain is subtracted to avoid doubling it.
No shared artwork semantics, weights, or per-pixel fields change. Capture/preview
state is temporary and is not restored on refresh. Clear/cancel never writes.
