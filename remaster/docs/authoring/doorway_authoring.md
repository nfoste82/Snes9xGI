# Doorway and door authoring

`doorway` describes the fixed frame around an opening. `door` describes the
actual door leaf or closed/locked barrier; it is not a synonym for the frame.
Existing legacy `door` annotations are migrated to `doorway`, preserving their
authored height, normal, occlusion and frame-mask data.

## Door facing

For the **door** role, choose **Canonical wall / door facing** in Metadata,
click **Apply Metadata to Selected Tiles**, then save the profile. Door supports
one cardinal direction (north, east, south or west), stored in the same
`tile_semantic_normals` field used by walls. The browser atlas offers N/E/S/W
buttons too. Leaving facing unset remains valid for existing doors; no migration
guesses a direction. Diagonal and multiple Door facings are rejected.

This is canonical artwork metadata, not a gameplay-space override or a doorway
support contact. It does not generate geometry, change placement offsets, or
infer assembly ownership. Height/normal layers remain independent.

## Directional support contacts

In the inspector's **Metadata** layer:

1. Choose the **doorway** role.
2. Enable **Doorway: support contact** only on members that contact the floor
   supporting the frame.
3. Choose the cardinal direction **from that tile toward its supporting floor**.
4. Click **Apply Metadata to Selected Tiles**, then save the profile.

The direction is authored in the canonical, unflipped tile orientation. Runtime
H/V placement flips transform it. It is not necessarily the direction through
the opening, the visible normal, or the exterior-facing side of the doorway.
Support contacts require a direction; non-support members do not.

For recognized three-band frame handlers, a support establishes the landing
datum. Structural bands add their handler-relative rise above that datum;
opening pixels keep the landing datum. Non-support members inherit through
proven assembly ownership, including disconnected sides and valid split-BG
members. Their masks distinguish frame/opening pixels, not assembly membership.
See `room061_doorway_propagation_report.md` for capture010 evidence.

Current support solving requires a directly adjacent, proven supporting floor
with a flat zero-local-height contact edge. It does not search through arbitrary
tiles or bridge a gap to a nearby floor. Unmarked members inherit a base only
through proven recognized frame ownership. A marked contact does not bypass an
overwritten or unrecognized assembly; those ownership gaps remain separate work.

An independently matched surviving two-band inner column may now attach its
marked foot and exact next member even when an outer frame band was overwritten.
This narrower proof does not establish whole-frame ownership or attach the
replacement outer band. See `room051092_geometry_report.md`.

Later capture098 investigation identifies a separate complete lower inter-room
stair writer: its terminal cap row is intentionally written on both BGs. When
that complete writer is proven, both actual cap writes receive the assembly
datum; BG1 is not chosen merely because the inner jambs use BG1. The earlier
Room051 row58 overwrite diagnosis was incomplete. See
`room051_098_103_geometry_report.md` for corrected ownership evidence.

Only annotate genuine contacts. Hidden floor artwork on another side must not
become a competing support simply because it touches the frame. BG isolation
is an inspection aid, not evidence of gameplay-plane identity or runtime support.
Shared graphics used with different meanings need contextual ownership; do not
choose a direction solely to make a single occurrence look correct.
These support annotations currently belong to the asset's Metadata layer, not
to the per-location height/normal context layer.

Migration does **not** guess support contacts or directions. An unchecked flag
means that the member supplies no explicit directional support; it does not mean
the member has no geometry or cannot inherit proven assembly support.

The Doorframe pixel mask remains a separate layer for **Doorway** assets.
Painting a mask does not establish a support contact. Marking an asset **Door**
does not supply closed-door dispatch, animation ownership or locked-state inference.

## Manual acceptance checks

- Select an existing frame tile: its role is Doorway, and its old mask and local
  geometry remain intact. Doorframe remains available for it, not for Door.
- Enable a support contact without a direction: Apply must reject the edit.
- Apply a cardinal direction, save, reload and inspect again: both fields persist.
- Undo/redo the metadata edit and copy/paste the Metadata layer: both support
  fields follow the operation without changing the height or normal layer.
- Check that the shared action row shows both Rotate buttons and Reset Layer
  separately, with no overlap above Reset Tile. Layout regressions include reset
  and animation-apply buttons, not just clipboard/transform buttons.
- Turn off support or switch away from Doorway: saved metadata must not retain
  an active support direction for a non-support tile.
- Inspect mirrored placements using BG isolation and Height view; the canonical
  contact direction must follow the placement's H/V flips. Do not judge a complete
  room's lighting from the isolated layer alone.

These checks require fresh live frames. Historical captures retain baked instance
heights; loading one alone does not recompute support after a metadata edit.

## Room051 inspection starting points

The previously unresolved southern jambs are `(30,56,BG1)`, `(33,56,BG1)`,
`(30,57,BG1)` and `(33,57,BG1)`. The replaced outer band is
`(30..33,58,BG1)` and the same coordinates on BG2. All have local height data;
support/ownership evidence, rather than missing height arrays, was the blocker.
These are inspection coordinates, **not automatically authored support choices**.

See `doorway_support_report.md` for implemented runtime scope, tests and remaining
complete-room evidence gaps.
