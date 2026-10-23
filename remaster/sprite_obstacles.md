# Movable sprite obstacle groups

## Authoring

In Pixel Inspector's layer dropdown, choose **Obstacle Grouping**. It displays
**Select Tile Locations** (locked while grouping) and selects both BG and OBJ
occurrences, independently of background context bindings. Other layers retain
their prior selection-mode choice when you leave grouping; context controls are
hidden only in grouping and ordinary Metadata remains tile semantics.

Exact scene clicks made before switching layers are retained, including repeated
identities and individual flips. The tile-type count may be smaller than the
occurrence count; identity toggles in another layer do not erase separate clicked
cells. Grouping highlights only those exact cells/instance records, never every
use of the same hash. Cmd-click toggles individual occurrences. Changing the
background context selection dropdown preserves this independent click evidence.
New frame/capture/ROM and resume transitions discard old occurrence coordinates.

Inventory/group-type selections without scene-click evidence are not enough to
infer object locations: the inspector asks for explicit clicks, rather than
expanding adjacent same-identity objects. **Show Group Types** similarly focuses
saved identities without selecting scene occurrences. Existing explicit,
confirmed footprint-completion proposals remain separate from layer transfer.
The ordinary **Select Tile Locations** option in other layers is specifically
for BG context bindings; it intentionally does not create invalid OBJ bindings.

Command-click
each 8×8 occurrence in **one** assembled object, including repeated artwork at
different positions. Enter a unique group name and click **Create Obstacle
Group**, then **Save Profile**. OBJ selections automatically create a sprite
assembly; background selections retain the existing static obstacle behavior.
Do not mix background and OBJ members.

For the Room051 ornament, the complete footprint is **6 columns × 4 rows = 24
occurrences**, although only **16 unique tile types** are shown in the inspector.
The occurrence count, not the unique-type count, determines the saved footprint.

If an existing sprite group omitted repeated occurrences, Obstacle Grouping offers
**Complete Footprint…** when the captured motif has a unique match. Confirmation
adds observed adjacent repeats and regenerates south-boundary contacts; existing
coordinates, flips, Z and every authored asset layer remain intact. This is an
undoable, review-only editor proposal, not automatic runtime ownership. Save only
after reviewing it. Ambiguous matches are not completed. The status now reports
actual support/conflict or missing captured evidence, separately from local layers.

The footprint preview preserves relative positions and individual H/V flips.
Initial support contacts are the south boundary; review these and uncheck any
overhanging part that does not touch the floor. Contact editing, naming, deletion,
Undo and profile merge use the existing group controls. The `obstacle` tile role
is independent of group creation.

**Support Contact applies immediately to the selected occurrence members**, not
all uses of the same artwork. Cmd-click scene cells or footprint buttons to
select multiple members; a mixed checkbox means the selection contains both
contacts and noncontacts, and its first click enables all. The member dropdown
switches to a single authored member. Scene selections use translated/mirrored
footprint evidence (including individual tile flips); ambiguous repeats, stale
cells, or selections spanning groups/sources show an error and change nothing.
Use footprint buttons when captured artwork is too incomplete to disambiguate.
Removing the last contact is rejected with an alert and restores the checkbox.
Click intent comes from saved member flags, not AppKit's post-click three-state
value: an unchecked or mixed selection enables all; an all-checked selection
disables all (unless that would remove the last contact). Rejections leave the
profile unchanged, restore the checkbox and display an actionable alert and
status/tooltip. No-group and ambiguous selections disable editing with a visible
selection diagnostic; stale/empty member selections ask for reselection. Explicit
footprint/dropdown selection takes precedence over old scene clicks and stays
with its named group rather than a group's transient vector index.
**Apply Selected Z / Name** applies Z to all selected exact scene/footprint
members and updates the group name in one undo step; contacts and unselected Z
are preserved. The dropdown selects one member. Mixed Z displays a blank field
with **Mixed** placeholder: enter a signed integer (-4095…4095) to set the whole
selection; leaving a mixed field blank preserves its Z values for rename-only
edits. Missing selected repeats require explicit confirmation before addition,
just as contact editing does. Edits use Undo, live preview and Save Profile merge.

For an older footprint that omitted repeats, selecting those scene cells and
clicking Support Contact now offers **Add Missing Occurrences and Set Contacts**
when the captured completed motif is unique. Confirming adds the missing members
and applies the selected contact intent in one undoable operation. Cancel changes
nothing. Existing member coordinates/flips/Z, unselected contact flags and asset
layers are preserved; added, unselected members start without contacts. The
checkbox previews the proposed selection's mixed state and labels the pending
addition. This path does **not** reset all contacts to the south boundary (the
separate Complete Footprint action still explicitly proposes that reset). Existing
interior contacts may need to be deselected separately; saving contact flags does
not establish correct runtime floor support.

Author local heights and normals normally. Sprite members use
`supporting floor base + member Z * 8 + authored local height`. Leave member Z at
zero when local arrays already describe the complete relief. No geometry is
automatically written to profile assets. Resume gameplay to reevaluate floor
support when editing an older capture without matching sidecars. Current live
packets retain their own movement-floor evidence for immediate paused refresh;
opening captures restores it from matching complete room/terrain/actor sidecars.
Replay never borrows the currently running game's floor map.

Click-to-pause now dispatches a value-owned copy of the **same completed packet**,
including its immutable support snapshot; it does not lose that support by
reopening the pointer-free serialization. Paused captures also write complete
room/templates/terrain/actor inputs and the original profile pin sampled at that
packet's EndFrame boundary. Later reopening reconstructs support only from those
matching sidecars. Old captures with no sidecars still cannot reconstruct support.
Complete-input byte copies are bounded (~103 KB/frame when sprite support exists);
profile snapshots are shared per revision, not copied or serialized every frame.

Authoring is progressive: missing local height on one member does not disable
another member or authored normals. Floor-support status is independent of local
coverage. Members without local heights do not become flat shells at the floor
base. With unresolved/conflicting support, authored OBJ normals remain visible,
but absolute height and shadow geometry remain absent (no guessed zero floor).

## Persistence and isolation

Profile schema30 adds optional `sprite_assembly = true` to an obstacle group.
Omitting the key retains schema22 background behavior. Old profiles need no
migration; saving a sprite group requires the new binary. Repeated hashes remain
legal within and across groups. Identity is reusable artwork, not ownership:
complete translated/mirrored footprints establish ownership. Two complete claims
sharing any occurrence are rejected in full, even if their Z/contact data agree;
neither profile order nor the larger footprint breaks a tie. This is a validation
relaxation of the existing grammar; no new field or profile migration is needed.
The TOML serializer, three-way merge and Python atlas/semantic preservation retain
the field. Both native runtime and independent background audits exclude sprite
groups from fixed room footprints and floor constraints.

Clicking a member focuses an existing group only when one complete visible
footprint contains every selected occurrence. Selecting cells outside that
footprint returns to **Create Obstacle Group**, including cells with artwork
shared by the old group. Start with a plain click on the new object to replace
the previous selection, then Cmd-click its remaining cells. No locations are
auto-added, and creation does not clone another group's contacts or Z values.
Competing complete groups or stale click evidence disable creation/editing and
show an ambiguity diagnostic. Partial/occluded footprints cannot auto-focus a
group; inventory-only focus works only for an identity present in one group.
Explicit footprint-button/member edits retain their selected group until the
next scene click. Save Profile, merge and Undo still operate per named group.

## Runtime contract

`sprite_obstacle.h` matches complete translated or mirrored OBJ footprints using
canonical owned pixels, coalescing repeated scanline records. It does not use OAM
slot numbers or room-specific artwork hashes as persistent object identity.
Ambiguous, overlapping and incomplete claims are not promoted to solved geometry.
Local asset and placement arrays are immutable. Shared floor support replaces,
rather than accumulates with, the legacy per-OAM offset.

Contacts use independently solved gameplay-plane floor components at the
transformed underside and immediately outside row. Upper artwork cannot vote
for the wall behind the head. Conflicting support or two eligible gameplay
planes remain explicit conflicts, not a Link-plane or highest-floor guess.
The castle movable mantle additionally uses its verified game actor's own plane
and exact complete OAM draw signature to disambiguate overlapping floors. This
includes exact reconstruction of its draw-before-movement horizontal step.
Actor/camera observations are pinned at frame start with the DMA-latched PPU
entries; next-frame WRAM staging data is not the displayed frame's evidence.
See [`sprite_obstacle_movement_report.md`](sprite_obstacle_movement_report.md)
for the scrolling ownership failure and production movement regression.
Other producer families need equivalent verified adapters when contacts alone
cannot distinguish the gameplay plane; grouping itself is not family-specific.
Vertically culled mantle entries reproduce the USA producer's exact Y=$f0
correction, retaining actor-derived logical positions for hidden identity only.
See [`sprite_obstacle_culling_report.md`](sprite_obstacle_culling_report.md) for
the exact `_056` recovery and unchanged `_059` control; wholly clipped children
are not lighting receivers.
See the header for exact current evidence limitations. In particular, grounded
settled dungeon support is the implemented scope; airborne props, wholly hidden
members and ambiguous overlapping-plane ownership need additional producer data.

## Checks

Portable checks can be built with `c++ -std=c++17 -O2` and run directly. Pass
`~/Library/Application Support/Snes9x/Remaster/frame_room051_003.s9xrmf` as the
optional argument to the editor/runtime tests for the capture fixture; these
tests do not create a group in your profile.

- `obstacle_group_editor_test.cpp`: background/sprite source isolation, repeated
  occurrences, H/V reconstruction, schema30 roundtrip; optional Room051 capture
  yields 24 members, six default contacts, 1,518 visible OBJ pixels.
- `sprite_obstacle_test.cpp`: motion, all mirrors, repeated artwork/instances,
  bases0/50/negative, support conflict/absence, local-array preservation and
  no additive drift. Optional Room051 capture proves assembly matching, not
  complete-room floor correctness.
- `dungeon_authored_obstacle_test.cpp`: sprite groups cannot claim fixed BG
  geometry; legacy background obstacle regressions retained.
- `profile_merge_test.cpp` and Python authored-obstacle/atlas tests: persistence
  and independent BG-audit exclusion.
- Existing animated-background footprint, terrain lifecycle, obstacle invariance,
  geometry-cache, context binding, layer-editor and inspector-height tests remain
  passing. The native layout checks also verify OBJ occurrence routing and
  schema promotion; they do not replace an interactive click/save/Undo check.
- The broad `remaster_test.cpp` smoke currently stops at its hard-coded
  `assetGroups.size() == 11` assertion: the user's current profile has 12 groups
  including `npc_workbench`. No authored profile or expected count was changed
  to bypass that unrelated fixture assumption.

New captures preserve actor/motion observations in `.sprite-input.bin` (ALTPSA2),
alongside the existing complete room and terrain sidecars. Old captures remain
usable for authoring but do not retrospectively gain actor-plane evidence.

These tests are not approval of every room's floor geometry. Complete-room
validation v2 and pinned producer/plane inputs remain required for that claim.
The pinned Room051 audit and reproducible validation artifacts are recorded in
[`sprite_obstacle_report.md`](sprite_obstacle_report.md).
