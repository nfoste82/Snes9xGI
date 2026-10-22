# NPC support at blocked-art overlap

`frame_room051_028.s9xrmf` records Zelda's head in OAM68 at base50 and
lower body in OAM69 at base0. Visible bounds are143,66–154,78 and
143,78–155,88. The old lower-body midpoint/bottom sample maps to blocked
cell(29,11): collision2, published plane0 support255. The old path therefore
leaves that half at zero rather than raising it with the head. It does not
actually add the obstacle's local relief: the visual discontinuity comes from
independent per-part support failure.

## Repair

With runtime movement diagnostics, generic OBJ support now samples the bottom
eight pixel rows of its visible contact footprint, accepting only observed,
nonwall movement-floor cells with solved gameplay-plane support. Structural
wall/obstacle bases cannot vote. Distinct floor bases, conflicting or unknown
floor support reject the sample; there is no nearest/highest floor search.
The existing plane selection is unchanged for generic actors (Link's plane);
independent actor-plane adapters remain necessary at ambiguous overlaps.

The follower draw routine emits body20/22 into regions beginning at48/68/92/112,
optionally after two effect entries. The repair attests both final OAM entries,
their relative pose, palette/flips, dedicated DMA addresses and canonical children,
then shares the assembled footprint's support between head and body. This is
producer-family logic, not a Zelda hash, room51 or persistent OAM identity rule.
Follower effects are not claimed. Complete OAM observations are required for
this assembly adapter; old packets without them cannot establish ownership.

Support is cached per OAM assembly per application, not resampled per scanline.
Link's reserved-block handling, carried/thrown producer logic and authored sprite
obstacle support retain their separate contracts. Local authored arrays and
generated-ramp alignment are unchanged. Legacy maps without movement diagnostics
retain their old application path. No profile or room solver metadata changes.

## Regression

`npc_floor_support_test.cpp` covers all four follower regions and both effect
layouts, signed/zero/upper bases, blocked structural relief, conflicting floors,
missing support, scanline records and immutable local arrays. Its optional
capture argument reconstructs support only from matching pinned sidecars and
the capture's own profile, checks the observed head50/body0 defect, and requires
every follower record to receive50 after applying the production adapter.
This includes scanline records with no final main-screen pixels: once assembly
ownership is attested they inherit too, rather than silently staying at zero.

Capture SHA256: `2c228bbbb9159050757120a4697fac9248b338e18ac336db7b80d4973356a7e1`.
Pinned profile SHA256: `f89aae1760a36a94f88c1164675fc4815148c066e2094dcebb89fc68da8a2fb9`.
Run the regression with the capture path as its sole optional argument; neither
the capture nor its profile is rewritten. Existing packets retain their recorded
offsets; fresh live packets are required to see this runtime correction in-app.

This is an actor support-consumer repair, not complete-room geometry approval.
The capture's recorded complete-room counts remain962/3720 required plane cells,
0/0 missing,564 solved/24 unknown walls and4 structural conflicts; no counts are
claimed repaired by this change. Fresh visual gameplay validation remains pending.
Specialist delegation was attempted but failed due to the harness's unavailable
model variant; local `tagalong.c` producer code and complete capture inputs were
inspected directly instead.

Validation: pinned capture regression passes with ASan/UBSan: all64 follower
records receive50 (previously head50/body0). Synthetic follower cases and Link
attachment regression pass. Debug and Release apps build successfully. The
terrain lifecycle fixture now explicitly supplies a walkable floor for its
unrelated screen-space actor: its prior off-room structural-only base is no
longer valid support under the new contract. Authored profile files are untouched.
After correcting that fixture's contact cell to(1,1), terrain lifecycle and
sprite-obstacle regressions both pass with ASan/UBSan. No production changes
were needed after the successful Debug/Release builds.
