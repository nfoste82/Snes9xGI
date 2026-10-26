# Teleport-door outgoing geometry (2026-10-26)

Room001↔Room052, captured settled states001009/052001: **reviewable but
incomplete**. Runtime transition retention repaired and real-route regression
passes; complete-room structural approval and fresh GUI comparison do not pass.

## Evidence and cause

Both supplied packets are settled module7/submodule0, collision0, gameplay
plane1. Their own profile FNV is`6b0c8c2709253e8d`, rise50; ROM SHA256 is
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Own-input production EndFrame replay passes144,930/151,471 matched BG usages
respectively. Complete WRAM/PPU maps match in both replay wrappers. No authored
profiles, heights or normals were edited by this repair.

The actual reference route from room001 local(423,120), plane1, default chosen
entrance/context, `E300,W300`, executes ordinary handler02 in both directions.
This is a teleport-door lifecycle, **not** straight stairs and not evidence of a
generic BG1/BG2 two-floor category. There is no early destination-plane switch:
the actual incoming and outgoing plane remains1. Destination publication already
works at phase2 with independently completed collision arrays.

`Dungeon_StartInterRoomTrans_Right` / the corresponding left routine calls
`Dungeon_AdjustForTeleportDoors`: before the first handler02 snapshot it rewrites
`A0` to the destination and `A2` to its synthetic cardinal neighbor.001→052 has
`A2=051`;052→001 has`A2=002`. The outgoing map actually belongs to001/052.
The source latch formerly required its cached room to match A0 or A2 and rejected
both edges. Reproduction before repair fails`actual transition lost outgoing
geometry before load` at the first transition frame, not at scroll completion.

## Reusable repair

`S9xRemasterObserveOutgoingGeometry` additionally accepts the last published room
before ordinary loader phase2 only if A2 is the cardinal neighbor implied by the
actual scroll direction and **all16,384 source map bytes remain unchanged**.
This uses the proven handler's lifecycle, not room IDs, asset hashes or an
invented height anchor. Both BG maps freeze independently. Preload rendering with
an explicitly retained source uses that source everywhere: the destination origin
still names the old origin until loading returns. Legacy unattested scroll inputs
retain their existing ownership behavior. Later per-cell application still checks
final tile word, palette, hash and flips; local ramps are preserved.

Failure-class negative tests reject changed source maps, late claims and
incompatible direction/predecessor. Existing tests retain partial overwrite,
flipped producer, profile/ROM/reset and offscreen retirement rejection. This does
not expand grounded level-door support inference or accept unsupported handlers.

## Actual effective-height checks

`dungeon_straight_stair_route_test.cpp` now supports a`teleport` mode as well as
its existing actual straight-stair route. It runs production EndFrame twice:
first collects each destination's first settled solve, then checks incoming
per-pixel base and selected local height against that independent-time oracle.
Outgoing pixels compare both base and local source against the frozen source
applied independently as a settled room. Both BGs and separate plane arrays are
checked; producer-mismatched uploads are excluded, not counted solved.

601 actual snapshots:164 handler02 frames,128 scroll frames,160 loaded-destination
frames,3,956,736 source-coordinate checks. Incoming BG1/BG2 effective checks:
3,612,672/3,612,672 (nonzero bases2,598,108/1,253,392). Outgoing base/local checks:
3,956,736/3,733,920;2,311,500 nonzero-floor checks. Destination plane0/1 comparisons:
BG1[234688,516160], BG2[98880,479040]. All pass after the repair; these are repeated
sample counts, not unique solved geometry or independent Snes9x compositing proof.

051↔061 N400,S400 regression still passes801 snapshots,342 stair frames and120
scroll frames. Incoming checks BG1/BG2:3,383,296/3,123,456; outgoing effective
base/local checks8,888,320/8,388,864. Plane and actor separation is unchanged.

Quick suite passes16/16 after final guard/negative tests. `git diff --check`
passes. arm64 macOS Debug and Release builds succeeded. Parent independently
reran the601-frame teleport route and departure negative tests; both pass.
`build/release/Snes9x.app` embedded framework matches the build product, SHA256
`6c644b01e018dfe0d62837d5783efc5366ad595b9708be52e970c3362db3ec29`.
No GUI transition/compositing review was performed.

## Whole-room gates and corpus limits

Mandatory v2 was run against **complete own-capture** ALTPPD1/ALTPAT1 wrappers,
own pinned profile and exact runtime solver (not the partial viewport packet).
Local seed coordinates match the pinned traversal state. Each graph enumerates
the full room reachable from its observed settled plane1 seed; **unobserved
special stair edges and disconnected/other-plane components remain unsupported**.
These are not a complete all-plane/all-state doorway graph, and cannot approve
the rooms. No compiled map is generated by this runtime-lifetime repair.

Both validators exit1 and emit deterministic JSON/SVG:

| State | Required/valid observed support | Fallback valid/required | Solved/unknown walls |
|---|---:|---:|---:|
|001009|368/368|6/6|444/480|
|052001|378/378|6/6|610/8|

The broader live solver denominators remain001[0,3562] and052[278,3584] with no
missing support in their enumerated scopes;052 has2 structural conflicts and001
has unresolved disconnected-plane relationships. Do not equate these numbers or
the passing transition comparisons with complete geometry. Unknown walls, missing
special movement/plane evidence, offscreen/empty/non-geometric placement accounting
and actual GUI capture comparison remain blockers. No conflict gate was weakened.

Available native sidecar corpus audited:153 complete packets,15 distinct
room/topology/active-plane states across10 rooms(001,002,011,012,022,041,051,052,
060,061).151 packets are settled,2 are handler02;12 contain teleport collision
attributes. That inventory does not prove those other doors' lifecycles. Actual
route corpus checked is two roundtrips/four directed edges:601 teleport snapshots
plus801 straight-stair snapshots. Remaining supported room IDs out of320 and all
unobserved switch/door/water states are **not transition-validated**.

Artifacts:`build/room001052-transition/`:route.bin, route.raw.route.jsonl,
before.log, after.log, stair-regression.log, capture*-replay artifacts,
gate-{001_009,052_001}/validation-v2.{json,svg}, corpus-audit.json, quick-final/
results and Debug build logs. These are review outputs, not authoritative sources.

## Reproduction

Build adapter using`alttp_build_route_adapter.py` with existing verified engine
and an existing output directory as in the straight-stair report. Run from the
engine directory (absolute output paths):

```sh
ALTTP_ROUTE_SEED=423,120,1 ALTTP_ROUTE_ONLY=1 \
 ALTTP_ROUTE_ALLOW_ROOM_TRANSITIONS=1 ALTTP_ROUTE_RUNTIME=<out>/route.bin \
 <out>/route-renderer --linked 0x1 <out>/route.raw -1 0 0 0 E300,W300
c++ -std=c++17 -O2 -DNDEBUG -pthread \
 remaster/tests/dungeon_straight_stair_route_test.cpp -o <out>/test
<out>/test <out>/route.bin \
 "$HOME/Library/Application Support/Snes9x/Remaster/frame_room001_009.s9xrmf.profile.toml" \
 "$HOME/Library/Application Support/Snes9x/Remaster/frame_room001_009.s9xrmf.dungeon-templates.bin" teleport
```

Smallest next action: relaunch Release and compare both teleport directions.
Room approval remains blocked; obtain exact special-transition and
disconnected-plane evidence, resolve the unknown structural assemblies, and rerun
the complete-room zero-exit gate before calling these rooms successful. Fresh
interactive transition capture remains necessary to establish visible behavior.
