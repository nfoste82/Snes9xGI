# Experimental authored overworld placement offsets

Schema38 adds optional additive per-pixel terrain heights and support-normal
rotation; see [terrain offsets](overworld_terrain_offsets.md). The scalar remains
one value per placement. New native saves omit identity zeros; the explicit-zero
editing/save descriptions below document the older schema37 tooling.

Opt-in profile schema 37. This is an experimental static placement mode, not an
automatic geometry solver or approval of the castle/world's geometry.

```toml
[overworld_placement]
mode = "authored_overworld_offsets" # absent / "legacy": existing behavior

[[overworld_offsets]]
world = "light" # or "dark", independent maps
background = 1 # zero-based BG2 for the verified USA ALTTP live adapter
x = 192 # global 8px world tile coordinate, NOT area-local or physical VRAM
y = 192
offset = -40 # one signed integer scalar, -32767..32767
```

All five record keys are required. Duplicate coordinates, unknown keys/worlds,
arrays, out-of-range values and unsupported schema versions fail parsing. Maximum
1,048,576 entries; 16-bit pixel coordinate domain (tile 0..8191). Background 0/1
can be stored independently, but only BG2 has a verified USA live world transform.
No external paths/references. Missing entries are exactly zero in both worlds.
No identity guard is implemented: expected words/hashes in proposal JSON are
diagnostic base-ROM metadata, not runtime rejection checks. Map replacement at a
saved coordinate continues to use that coordinate's origin.

Runtime uses frame-start WRAM camera hints to unwrap actual renderer per-line
10-bit scroll, including renderer +1 vertical convention. It does not use Link
position, capture filename, or tilemap physical addresses as world coordinates.
Mode1 normal 8px BG2/64x64 tilemap/non-hires/noninterlace with stable world identity
is supported. Mid-line raster changes are rejected. Conflicting per-occurrence
positions yield no location and thus zero offset, not stale geometry. Distinct
world positions sharing a physical tilemap address remain distinct keys.

Origins are compiled once on profile publication into a bounded hash lookup;
each existing visible occurrence gets one lookup and scalar assignment. Authored
local arrays/presence, transforms, normals and masks are unchanged. Final height
is local sample + signed scalar, with no premature unsigned clamping. Incremental
CPU lighting depends on selected location and composed base. Existing dungeon
and actor-support paths are unchanged. Actors are NOT moved by their graphic
coordinates; outdoor actors are not newly grounded on the static map.

Native serialization/save merge and Python atlas compilation preserve the offset
records. The native inspector supports scalar placement editing (below); it never
paints per-pixel placement arrays.
Loading/saving the experimental profile must target its copied path. Canonical
profiles do not auto-enable this mode.

## Read-only Height Offsets debug view

**Command–Shift–W** / **View > GI Debug: Height Offsets** toggles the saved
placement-coordinate data view. Press again to restore the previous lighting/view
state. It is also appended to the existing Command–Shift–D GI cycle (stable ID12).
Paused toggling redraws the last completed packet without stepping emulation or
adding recording frames. Ordinary Command–W still closes the window.

The fixed grayscale window is **−128 black, 0 midgray, +128 white**, saturated
outside that range. In particular −40/−32/−24/−16/−8 are distinct. It does not
auto-normalize when scrolling and ignores Height range/exposure, local authored
height ramps, normals, actor origins and dungeon bases. **Purple means not
applicable/unavailable**, not a stored zero: unsupported coordinates, non-BG2,
HUD, sprites, dungeon, disabled placement mode and old captures are masked.
Existing observed main/sub/scene visible-occurrence selection chooses the BG2
placement; missing local asset channels never elect another offset donor.

The Height inspector adds a read-only frame-evidence line with Light/Dark,
zero-based world tile XY, signed saved scalar and explicit-entry/default-zero
status for the selected occurrence. It does not add a brush or change any asset
editing/save controls. Native scene controls have no GI-view selector; use View.

Frame schema41 adds a bounded 5-byte saved scalar/status to each occurrence,
separate from composed `heightOffset`, with existing coordinate identity in
`contextLocations`. New recordings retain it through their ordinary embedded
packet serializer. Frames through40 default to unavailable, even in current-profile
recording preview; no historic coordinate is reprojected through current RAM.
The scalar is the value selected from the active profile at the completed frame
boundary (possibly unsaved editor publication), not a new disk read. Timeline
Recorded Evidence continues showing recorded placement values. Current Profile
Preview may update those verified coordinates from the loaded profile's scalar
map without reconstructing coordinates or doing a full geometry solve.
No bank calibration or automatic art/profile changes are included.

## Height Offset inspector authoring

In an overworld inspection packet, choose **Height Offset** immediately below
Height. This category is hidden for dungeon/cave/NPC packets, using the retained
scene context rather than current RAM. It is enabled only for the loaded matching
profile with `authored_overworld_offsets` already enabled and verified BG2 world
coordinates. Disabled mode, old/unverified captures and Recorded Evidence remain
noneditable with a reason; mode is never silently enabled on canonical.

Use the existing **Select Tile Types / Select Tile Locations** control. Types
(the initial offset mode) selects all actually visible, verified BG2 occurrences
of the clicked canonical TileID in the current scene, including H/V flips.
**Command-click** adds/removes types. It does not expand animation/asset variants,
offscreen world/profile records, other worlds, weather BG1, OBJ or HUD. A tile
merely present in the frame's occurrence vector but wholly hidden is not visible.
Locations keeps exact click / Command-click occurrence selection. Both modes
select whole tiles, never pixels; storage remains one scalar per world/BG/XY.
Changing the mode clears selection, like other categories, without editing data;
other categories' mode preference is independent. Replacing the inspected scene
packet clears picks, like their existing selection semantics. Within a packet,
refresh and edit re-evaluate current picker visibility so hidden old targets do
not leak into the next explicit edit. There is no implicit edit on camera change.

Highlights identify only eligible expanded occurrences/coordinates, never every
hash use regardless of visibility/support. The status reports the actual unique
visible location count, selected type count in type mode, and mixed values.
Repeated instances at the same world coordinate are deduplicated. Mixed values
show a blank **Mixed** field. **−1/+1** increments/decrements each selected
coordinate's own current scalar; typing a signed integer and **Return/Enter**
sets all selected coordinates to that value. Bounds −32767…32767 are strict:
malformed text or one overflowing member rejects the entire transaction. Writing
zero creates an explicit zero entry; missing entries read as default zero.

No pixel selection/brush is active in this category. The lower tile preview shows
the representative tile's authored local height; upper shows that local height
plus its one scalar (fixed signed −128…+128 window). Missing local height is
purple, never invented flat relief. The label explicitly identifies representative
preview semantics for multiple selected placements. Original Height editing is
unchanged after leaving the category.

The representative is the most recently added **selected occurrence**, including
its world coordinate, layer and H/V transform. Removing it chooses the lowest
remaining supported instance ID; removing the last selection clears both previews
and disables editing. Asset Previous/Next navigation is disabled in this category
so it cannot replace the preview hash without replacing the placement target.
Both local previews resolve the selected occurrence's retained context/flip
variant, not a hash-wide representative. Offset-category highlights use precisely
the same scene/stock occurrence policy and HUD exclusions as the click target;
other categories retain their existing selection behavior.

Current Profile Preview removes only the prior saved scalar when placement mode
is disabled or the profile target mismatches, exposing unavailable/purple rather
than an available zero. Independent non-placement datums remain intact. Verified
packet coordinate provenance is retained in memory for explicit re-enable; old
captures never acquire eligibility from coordinates alone. Enabled sparse missing
entries remain available default zero. Recorded Evidence is never rewritten.

Each batch is one Undo transaction and one runtime publication, updates current
profile preview immediately without stepping emulation, and marks the loaded
profile dirty. **Save Profile** persists through the existing three-way merge to
the currently loaded path; no autosave or canonical redirection is introduced.
Recordings' immutable original evidence is preserved; uncheck Recorded Evidence
to edit using recorded verified coordinates in Current Profile Preview. Current
profile placement preview updates are not a complete room geometry re-solve.

## Preview build and profile

This run's explicit paths:

* `build/overworld-height-preview/Snes9x.app`
* `build/overworld-scalar-offsets-20261030-173510/alttp-scalar-preview.toml`
* `build/overworld-scalar-offsets-20261030-173510/preview.html`

No application is launched by generation/build/tests. When you choose to review,
open the isolated app explicitly (same bundle ID as the ordinary app; do not assume
Finder selected the intended running copy), open the matching USA ROM, then use
**View > Load Remaster Profile…** and select the copied TOML above. Save Profile
targets that loaded copy. Do not select the canonical profile or change defaults
to enable this feature. Loading a profile may follow existing app workspace
persistence behavior; no defaults are changed by this tooling.

Offline regeneration, using the already-installed Pillow/numpy environment:

```sh
/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/visual-context-venv/bin/python -B \
  remaster/tools/ALttP/alttp_scalar_castle_preview.py \
  --output build/overworld-scalar-offsets-20261030-173510
```

Input is that directory's immutable fresh `current-profile.toml`, not historical
V2 arrays. All 16,384 castle BG2 coordinates are explicit, including zero. Dark
World is empty/default zero; no Dark World castle interpretation was generated.
The fixed PNG LUT is -40..160. `authoring-gaps.json` records mixed bank/deck
origins and missing local face gradients. No per-pixel placement ramps or
averaging of V2 per-pixel offsets is permitted. The scalar candidate intentionally
retains shortcomings that require local context authoring, not placement repair.
