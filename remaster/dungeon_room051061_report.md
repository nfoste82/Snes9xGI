# Room051 own061: complete east/west authored wall stacks

**Room051 / pinned own061: reviewable but incomplete.** The reported complete
east/west wall stacks are repaired: all 72 previously unknown outer-band
placements now have contextual bases, not only the 24 feet. The room-wide gate
still fails on existing conflict/rail/BG1 issues. No authored arrays, profiles,
room masks, room/hash elevation lists or commits were changed.

## Pins and actual outcome

Input `~/Library/Application Support/Snes9x/Remaster/frame_room051_061.s9xrmf`,
SHA256 `966f12496cf874cb9d4bbcc7a7476b0ef9c3948ec5e8b5cfeb015f9d180faed9`.
Profile `044e25516d1a2ae6384da27a2a331929e18f88ef645eef87005ca83ff0e90f67`,
serialized FNV `eef8e3acedf3300d`; complete ALTPDI1
`5b28d886a06a4ef14ce9c791cc979230a307ce1eca46615836bc6f2944650101`.
USA ROM `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Settled module7/submodule0, Link world(760,2760), plane0, scroll(640,2652),
both graphics bases16384. Configured rise50 remains unchanged; the independently
authored support relation is24. Complete WRAM/PPU maps agree; production packet
comparison verifies169,128 BG hash/flip/canonical-pixel observations. The current
profile equals the pinned profile byte-for-byte.

At west x16/17/18/19 and mirrored east x47/46/45/44, y16..19,22..25,28..31,
the bases are now **40/32/24/24**. All96 placements have supported structural
plane0 claims; none acquires a lower-plane wall claim. The three outer bands
retain the user's0..7 ramps, producing effective40..47,32..39,24..31. The foot
retains its irregular0..5 relief at24..29. Production EndFrame and CPU lighting
fields verify **6,144 scene samples across all eight visible wall columns**,
each actual authored source plus its solved base; no generated replacement local
array. Complete BG2 solved/unknown walls588/72→660/0, conflicts unchanged4.
Both complete BGs' local arrays are unchanged in the before/after inventory.

## Failure class and reusable repair

Two independent bugs were hidden by the previous irregular-foot diagnosis:

1. The tangent consumer demanded that an entire decorated obstacle tile be a
   one-dimensional ramp. The updated pillar interior is sculpted, **but its
   actual contacting edge remains an exact full eight-sample section** for the
   inner and middle wall bands. Contact semantics constrain that owned edge,
   not unrelated interior relief. The solver now compares every transformed
   contact sample while retaining the complete one-axis requirement on wall
   nodes and independently solved, unambiguous per-plane obstacle ownership.
2. The outer band's pillar edge really is incompatible with the new relief;
   it cannot directly inherit24. The ROM subtype1 61/62 handler selects7aa/7ca,
   emits an ordered4x4 section and advances two rows. Exact complete final words
   identify the surviving section; normal-axis continuation inside that unique
   section uses the existing signed authored boundary slope rule. This proves
   the outer bases40/32/24, rather than inventing an8-unit rise at the foot.
   Attributes6e/6f are admitted as structural art **only inside this complete
   owned wall section**; their special gameplay behavior is not reclassified
   as ordinary movement/support.

Classification accumulates ownership before publication, rejects distinct
template/origin claims, requires every local source and final word, transforms
H/V layout and flips, preserves priority-only writes, and rejects tile/flip
overwrites or floor/obstacle/stair ownership. A partially overwritten section
receives no new section ownership. The preexisting tangent pass runs first;
contradictory larger extensions cannot erase independently proved smaller
surfaces. Missing/flat/incompatible actual contact edges, missing sources,
underpasses, hidden distinct BG1 usage and competing anchors remain rejected.
No seam tolerance, array averaging, nearest/highest floor choice or change to
the direct-foot rule was needed. The irregular foot edge is still NOT a wall
stack seam or an extra rise anchor.

## Regression, evidence, and mandatory gate

`dungeon_wall_stack_test.cpp`:176 cases covering complete/transformed ROM
sections, priority-only and tile overwrites, absent sources, competing template
ownership, sculpted endpoint interiors, axes/HV/BGs, special bands without ROM
ownership, independent planes, signed local origins and competing datums.
The existing144 tangent cases now change actual contact edges in the negative
test instead of an arbitrary interior pixel. Existing160 direct-foot cases
retain their original strict negatives. All three narrow suites pass ASan/UBSan.
`dungeon_background_ownership_test` also passes. Validator unit suite26/26 passes.
The final unchanged-source portable full gate passes33/33
(`tests-final/20261023T170912Z-pn3h6nvx/results.json`); arm64 macOS Debug build
succeeds (`debug-build-final.log`), and `git diff --check` passes. An earlier
full run correctly rejected in-flight source changes and is not the final gate.
All seven own061 input pins and the user profile remain unchanged.

Available corpus:169 packets,95 complete bundles (74 Application Support,
21 retained build bundles),74 without complete sidecars. Exact solver replay
accepts94 complete settled states across seven room IDs; one room002 input is
rejected for inconsistent complete state/maps. No solved-wall losses. Own057..061
gain72 walls each; own056 gains12, totaling372 new claims. Historical051034..055
retain all solved outputs but expose four additional larger-section conflict
records each; own056 exposes ten additional records. These competing datums
are explicitly unresolved, not averaged or overwritten. Exact production
EndFrame replay accepts25 complete states and likewise has372 gains, **zero
solved losses and zero changes to previously solved bases**. Seventy historical
bundles fail serialization-profile pins; their solver-only results are not
misrepresented as production-packet proof. Per-state results/rejection logs are
retained in `production-corpus.json`, `solver-corpus.json` and subdirectories.
This is the available corpus, not320rooms/all states.

Mandatory complete-room validation v2 ran on exact ALTPPD1+ALTPAT1, pinned state,
complete observed-seed static doorway graph, pinned profile, exact runtime solve
and compiled ALTPHM3: **exit1, not room approval**. Reachable supports886/886,
plane denominators[886,0], overlap0, missing0; fallback26/26 with explicit
direction/ramp/source records. BG2 walls660solved/0unknown, four existing runtime
conflicts, two compiled missing/two mismatches at stair-rail endpoints remain.
Runtime structural support scope[962,3720] is distinct from doorway reachability;
the independent overlapping support arrays and active-plane semantics survive.
BG1 remains0solved/288unknown walls. All8192 placements are inventoried:
BG1 nonempty missing-local2, unknown role86; BG2 missing-local0, unknown role118,
empty460 explicitly accounted. Unknown/transparent art is not counted as solved
geometry merely because an array exists. Thus zero unknown classified BG2 walls
does NOT establish complete-room coverage.

055/060/061 fixtures reran with pinned own061 metadata:055 support439/439 but
exit1, one conflict/16unknown walls;060 rejects corner face seam disagreement;
061 rejects a directional diagonal source lacking independent ROM proof. These
are blockers, not passing rooms. No fresh interactive gameplay comparison was
performed; the packet/CPU checks are automated proof, not visual approval.

Artifacts: `build/room051061-repair/` contains pinned input/evidence manifests,
complete raw/attributes/state/graph, runtime JSON, ALTPHM3, validation-v2 JSON/SVG,
before/after production packets and complete per-BG placement inventories,
corpus records and narrow/full/build logs. The next general work is resolving
the explicit remaining rail endpoint/conflict/BG1 classes and obtaining fresh
matching gameplay review, not another bottom-only wall repair.

## Independent final verification

Parent-session full portable plus production Metal gate passes **34/34** on the
final unchanged sources, including complete wall-stack regression. Results:
`build/room051061-parent-verification/20261023T171742Z-0dg4kpzj/results.json`.
The arm64 Release build succeeds; updated app `build/release/Snes9x.app`, log
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room051-061-release.log`.
All seven original capture inputs and the canonical user profile remain
byte-identical to the initial baseline; whitespace checks pass. These checks
do not constitute whole-room approval or fresh interactive gameplay review.
