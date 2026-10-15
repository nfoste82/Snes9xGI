# Room071 own001/007/008 — reviewable but incomplete (static fixes implemented)

**Final current update:** see `dungeon_wall_projection071_report.md`. The256
unapplied BG1 wall pixels are repaired and all four BG2 false transparent-border
conflicts are eliminated. Current v2 conflicts are zero; exits remain1 because
unknown walls and incomplete declared traversal scope still block room approval.
The historical conflict/application-gap numbers below describe the prior pass.

This follow-up repairs actual runtime geometry; not instrumentation alone.
No room/hash patches, profile/rule cleanup, authored sample changes or global
configured-rise changes. Door recovery assets/reports remain untouched.

## Proven causes and fixes

### Northern001 missing wall adjustments

The recorded BG1 graph witnesses node2545/2546 versus support6 were at row63,
not the northern stairs. Those two flat cells are at x14/17, beside the south
room boundary doorway. Floor-pattern artwork outside legal Link body support
joined two independent movement components with rise0, despite a complete
authored wall path proving rise24. The false cycle poisoned the entire linked
BG1 room assembly. Reference provenance confirms pattern writer0 at that row,
with separate doorway/wall writers above; final collision on plane1 is00 but
the complete body cannot occupy the purported floor connection. A single
attribute00 did not prove legal support or wall geometry.

Shared fix in `frame.h`: enumerate nonoccupied semantic-flat components,
collect reproduced movement contacts, defer their flat seams until independent
authored wall relations have been examined. If contacts prove different levels
(or the independent graph conflicts), that art cannot short-circuit the rise.
Same-level and one-owner continuation remain available. No pixel value is
normalized or changed; the invalid edge is rejected at its evidence stage.

Own001 BG1: solved walls0→564, unknown594→30, structural conflicts564→0.
North x12/y11..13 now has bases16/8/0 plus local0..7; it no longer stays raw.
The remaining30 BG1 walls are not silently filled. Own007/008 already had
different floor/door metadata and keep their solved BG1 geometry.

### Southern007/008 upper50 and over-high far walls

ROM subtype2/31 and the related intra-room stair handlers use template0x10c8.
In room071 the intact final 4x4 assembly survives on BG1 at(14,19) V-flipped
and(14,43) unflipped. Later BG2 writes clear those stairwells (word01ec), so
looking for geometry only on BG2 missed the assembly. The source template and
completed stair attributes establish orientation and plane-changing ownership.
The authored outer rail profiles, not rough interior treads, provide matching
cross sections from local23 at the high seam to0 at the low seam, i.e. rise24.

Shared solver now validates all16 final template words (priority writes allowed,
identity/flips exact), all local sources, explicit Stair rail endpoints, both
uniform monotone rail cross sections, agreeing signed rise and two zero-local
landing samples per end on reproduced components. It adds the relation only
where no authored wall path already relates the landings; established wall
relationships keep precedence. Partial overwrites, missing/incompatible local
sources and disagreeing rail profiles retain the existing configured fallback.
BG1 exports the pre-datum signed endpoint relationship through the existing
plane-qualified cross-BG transport. No upper/lower plane collapse or copied
visible floor base. Per-pixel stair ramps and all authored local geometry remain
unchanged. The global configured fallback50 remains50.

Actual application audit also counts6336 adjusted BG1 wall-pixel usages in001
and008. Own007 has8736 adjusted and256 other wall usages whose solved base is
not applied (outside the reported north entry, e.g. cell26,47); these remain an
explicit projection/producer-policy gap, not silently counted correct. Targeted
north entry checks exclude neither bad placements nor raw-profile offsets.

All three OWN-profile captures now export two authored24 relationships and use
zero configured fallback constraints (previously2). Upper support50→24.
Southern far-wall bases66/58/50→40/32/24. In own007,384 actually captured main
pixels change effective50..73→24..47 with the same authored gradients. These
are real effective packet heights, not debug colors. Complete source/placed
maps preserve each wall's local profile instead of flattening it to the landing.

Read-only current-profile research against both001/007 was also run, using a
pinned copy `current-profile-readonly.toml`. It produces the same two authored
relations and removal of50 fallback. It is NOT substituted for own capture
profiles. The current profile source was not written.

## Deterministic reference traversal gathered, not deferred to user

Built `route-renderer` via `alttp_build_route_adapter.py` against verified v21
engine objects. The adapter now supports explicit diagnostic room-local
`ALTTP_ROUTE_SEED=x,y,plane` after settled construction. This relocates Link,
never changes room construction or anchors height, and logs the relocation.
Two200-frame scripted routes S100,N100 from(120,120,1) and(120,310,0) captured:

- north: (1,120,135)→(0,120,175),58frames; reverse
  (0,120,168)→(1,120,125),62frames.
- south: (0,120,326)→(1,120,369),62frames; reverse
  (1,120,361)→(0,120,320),58frames.

No room exit in accepted routes. A first S150 trial left the room and was
rejected, not used. Reference engine used entrance4's graphics context borrowed
from061; this is reference behavior, not fresh Snes9x parity. Full attributes
differ from live snapshots in unrelated state, so transfer checked EVERY body
cell throughout each stair traversal:20/22/22/22 plane-qualified samples, zero
differences against each001/007/008 pinned collision snapshot. No blanket
whole-room equality claim. Both gameplay planes remain independently explored.

## Tests, corpus and mandatory v2

New `dungeon_stair_assembly_support_test.cpp`: north/south V transform,
authored rise24/32, configured50 unchanged, priority-only overwrite,
cross-BG transport after BG2 clear, partial overwrite/missing local/nonuniform
rail/explicit incompatible role rejection, false flat-bridge regression,
single-owner and agreeing same-level continuation. Passes O2 and O0 Debug.
Actual `ALTTP_STATIC_STAIR_REGRESSION=1` EndFrame tests001/007/008 check
north16/8/0 and southern40/32/24, both authored relations and zero BG1 conflicts.
Authored-stair application, cross-BG wall support, geometry cache,
classification-plane cache, animated torch footprints, obstacle assemblies,
signed pit and clipped-corner regressions passed. Existing geometry test's
construction fixture was corrected from subtile1 to supertile2, consistent
with the earlier lifecycle fix; it now passes. An attempted nonexistent
`dungeon_floor_height_test.cpp` invocation is not counted as a test.

Paired corpus relative to previous runtime fix: all233 indoor packets inventoried;
62 replayed,162 missing complete evidence,9 stale packets refused. Only001
and historical060016 summaries change (060016 fewer BG1 conflicts432 vs504,
unknown346 unchanged). Summaries do not detect a solved50→24 change with
unchanged counts, so full geometry digests were also compared across346 offline
contexts/319 rooms: only071 and10d differ. Aggregate BG2 solved/unknown/conflicts
unchanged89638/256672/2019; no new conflict-count regression. Classified-local
versus missing-local denominators unchanged:5,668,864 placed BG/plane records,
1,992,842 missing local,2,949,674 known-local unknown-base,726,348 base+local
not geometry approval. No empty/transparent placement is counted solved merely
for existing in this inventory. Room017 and unenumerated dynamic states remain
outside corpus scope. Offline metadata pinned to own007 SHA256
`8ef2a4e57a55ba03a68dcfff8230eca980f475d83b482920a985edaa95ac3cf4`.

Full ALTPPD1/ALTPAT1 pinned wrappers, generated ALTPHM3 maps, profile, effective
sources/fallback directions, graph, v2 JSON/SVG and exact command are retained.
No validator weakening. With observed stair edges, reachable denominators grow:

| capture | valid/required support | plane denominators | BG2 walls solved/unknown | v2 exit |
|---|---|---|---|---|
|071001|297/297|52,245|828/270|1|
|071007|465/465|52,413|834/264|1|
|071008|297/297|52,245|834/264|1|

Zero missing support in that declared graph; each gate retains a conflict
record. Runtime all-support scope remains[492,3289] for001 and[492,3279] for
007/008. Internal subtile doorway special edges and other disconnected entries
are not yet exhaustively enumerated, so these do NOT constitute complete-room
approval. BG2 four structural conflicts and264/270 unknown walls remain; BG1
00130 unknown walls remain. Fresh Snes9x camera/plane visual review is absent.
No room is successful or capture-ready approved.

Existing fixtures:055439/439, walls1670/16, exit1;060711/711, walls1030/0,
exit1;061 v2 exits2 at preexisting directional diagonal independent-ROM proof
gap layer1(2,38). arm64 macOS Debug build passed, `git diff --check` passed.

## Artifacts / reproduction

Root:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/runtime-height-bugs/stair071-followup/`
contains paired `corpus.json`, static recomputed packets/maps/pixel diagnostics,
`north.raw`/`south.raw` with complete maps/planes/provenance/routes, current
read-only profile, each capture's gate directory and `summary.json`.
ROM SHA256 and captured profile pins remain those in the prior report.
`alttp_stair071_followup_review.py` reconstructs pinned graphs and v2 artifacts;
its commands and explicit graph-scope gap are serialized in each `gate.json`.

Remaining work is room-wide residual assembly/doorway evidence and Snes9x
comparison, not the two reported static causes above: those now have runtime
repairs and failure-class regressions. No commits or authoring changes.
