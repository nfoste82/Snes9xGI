# Out-of-bounds crest plateaus

Choose **Metadata → Tile role → out-of-bounds** in the native tile inspector,
then apply/save the metadata as usual. The ALTTP browser semantic workbench also
offers **Out of bounds (crest plateau)**. Its annotation store persists the named
role into the source profile; compile/reload when using generated profiles.
No assets are automatically assigned this role. The metadata overlay uses sage.

This is visible, unreachable geometry, **not Floor**, walkability, or collision.
Only complete opaque explicitly marked placements with proved visible cardinal
edge contacts form a sheet. A solved wall or complete door-frame/closed-leaf wall band
can anchor the sheet at its uphill terminal geometric edge. Every retained edge
sample of each individual terminal must agree. The sheet defaults to the **lowest
independently accepted absolute wall crest**; reused art can have different tops.
Different occupied floor IDs are not sheet ownership. Gameplay plane agreement
and each wall's valid signed contact equations remain mandatory. Underpasses are
not merged based on screen position.
Cross-BG sheet-to-sheet continuation requires the same exact visible-edge proof;
a wall on either BG can supply a proved crest to a sheet on either BG.

## Units and seam convention

Heights are source-pixel sample heights, not extrapolated vertices. The last
transformed authored wall edge sample plus solved visual placement base is the
crest. Base **16** plus local edge **7** gives **23**, not 24 or 31. A clipped cap
at local 5 gives base + 5, not an invented eight-unit rise. There is **no +1/+8**
at the wall-to-sheet seam and no tile-count increment within the sheet.

A role-only sheet explicitly declares a deterministic flat plane: solved
occurrences receive a semantic zero-local height and geometric +Z normal if
these channels are absent. These are occurrence layers, not authored arrays;
the inspector's selected-channel provenance labels them **explicit-role semantic
plane / geometric up normal (not authored)**. Authored height and normals are
never overwritten. Constant authored local 5 at crest 23 receives base 18.
Authored zero is valid. Nonflat authored relief is preserved and makes the sheet
Unsupported rather than silently flattening it. Missing/ambiguous crest evidence
is Unanchored; incompatible planes or invalid wall equations remain Conflict.
Unequal accepted crests are nonfatal (diagnostics retain minimum/maximum/count).
Choose the absolute minimum before subtracting each sheet tile's uniform local
relief. This never minimizes contradictory wall-graph equations. These statuses
are separate from independently available authored channel values.

The role appends enum ID 11; previous IDs remain unchanged. Existing profile
named-role and frame role-byte formats already support extension, so no wire
layout/version bump is needed. Older readers do not know this new role and may
reject a profile/capture containing it; older files remain loadable.

## Validation and limits

`dungeon_out_of_bounds_test.cpp` exercises the actual visual graph and CPU field,
transforms, BGs/planes, connected sheets, authored constant origins, missing
channels, ambiguity, semantic provenance and capture/profile persistence. The
curated full gate includes it. `REMASTER_OOB_CAPTURE` can write its isolated
fixture and feed the production Metal Height parity test.

Native interactive authoring, directional-light appearance and complete-room v2
validation of a newly authored real-room sheet
still require review/evidence. Passing targeted tests does not approve a room.
## Continuation safety contract

Role-only out-of-bounds art is admitted from an observed BG1 subscreen sample
with its palette artwork, not inferred from neighboring ink. It cannot receive
a physical floor, border, mixed-floor, preview or legacy upper-floor datum.
Missing height is not an authored zero. Until a valid crest is published, the
shared height selector and lighting/mesh consumers report unknown absolute
height, even if local authored relief exists; they do not borrow the lower BG
or manufacture provisional ground. Original channel arrays remain available
unchanged as metadata, including authored zero normals/masks.

Plateau connectivity requires two exact visible opaque tile edges, with matching
placement identity and effective local source. Such proved edges may cross BGs.
Every visible certified wall crest is examined, including a peer-BG crest beside
an OOB neighbor. Gameplay planes must agree; different floor owners and accepted
crest heights may contribute to the same proven sheet. For OOB terminals only,
a numerically consistent anchored wall graph on one compatible observed plane
is not vetoed solely by distinct floor IDs. Wall publication's separate owner
contract is unchanged;
unresolved contacts cannot be ignored in favor of a convenient solved anchor.
Offscreen, partially exposed, transparent and alternate/context-dependent
sources do not establish continuation. They remain unknown/unsupported pending
pixel-segment and placement-source proof. No physical floor solve is changed.

A solved sheet has a separate component domain, zero mesh thickness and no
vertical wall closure. It never welds to walls, underpasses or independent equal
height components. Semantic +Z normal presence is separately labeled in the CPU
field cache (`semanticNormals`); the historical `authoredNormals` renderer bit
continues to mean an explicit normal override, not an authorship inventory.

The atlas compiler preserves all present **and absent** OOB channels, with native
explicit roles taking precedence over stale sidecars/groups. Context-dependent
OOB labels persist but do not certify a placement datum. No source or canonical
profile is rewritten by these feature regressions.
