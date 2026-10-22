# Dynamic OBJ obstacle runtime: pinned Room051 review

**October21 follow-up:** the missing-local whole-group gate and sticky editor
refresh described below are superseded by
[`sprite_obstacle_authoring_report.md`](sprite_obstacle_authoring_report.md).
Floor support and local coverage are now separate; unresolved OBJ absolute
placement no longer discards authored local normal directions. Historical input
and complete-room evidence limits below remain unchanged.

## Producer-backed continuation (supersedes the actor-adapter gap below)

Implemented exact USA castle mantle producer ownership, not a room/hash whitelist:
`Sprite_EE_MovableMantle` (`sprite_main.c:1322`) calls `MovableMantle_Draw`
(`sprite_main.c:13760`, USA9A:FCB3). Constant tables at lines144..147 emit six
large16x16entries in reverse order. `Sprite_PrepOamCoordOrDoubleRet`
(`sprite.c:1835`,86:E41E) subtracts actor camera and spriteZ. `sprite_floor`
is WRAM$0F20+k, not Link's current floor. Sprite load reads this from the room
sprite record (`sprite.c:3653`); it is actual actor ownership.

Narrow integration additions:
- `frame.h`: value-owned16`ObstacleActor{x,y,type,state,z,plane}`, presence flag.
- `gfx.cpp`: read complete sprite arrays at $0D10/$0D30,$0D00/$0D20,$0E20,
  $0DD0,$0F70,$0F20. Raw plane retained, values>1rejected, not masked.
- `sprite_obstacle.h`: actor typeEE/state9/Z0, exact six-entry position/name/
  flags/size signature, all24canonical children and OAM slot memberships must
  agree. Duplicate actor ownership conflicts. Actor plane restricts contact
  evaluation; the plane must STILL have a common solved floor/component.
  No inference from palette/priority alone, no nearby-actor association.
- `remaster.h`: needsSpriteSupport allows runtime solving beside compiled BG
  review maps. Call passes only current publishable/valid runtime support map.

Independent freeze `~/Library/Application Support/Snes9x/Freezes/Zelda_ALTTP.005.frz`
contains room051actor0EE/state9/plane0/Z0/anchor778,2595, camera640,2560.
All16,384final map bytes and8,192collision bytes exactly match capture003.
Its OAM116..121signature is exactly the capture footprint translated by camera3.
`freeze005-research-manifest.json` pins freeze SHA256 and explicitly labels
`freeze005-reprojected-producer.bin` as **independent research actor state**,
NOT historical003actor attestation. No input sidecars were overwritten.

`producer-research.json` proves exact producer support **plane0/base50** after
test-only local layers; before those layers native geometry remains unresolved.
Negative-control tests prove changing only the attested actor plane selects
plane1/base0, not Link plane0. Duplicate actor, misaligned anchor, overwritten
OAM and inactive state reject; restoring evidence resolves. All results capture
round-trip. Ordinary room v2 failures below are unchanged.

New additive evidence reader `S9xRemasterReadSpriteObstacleEvidence(path,c)` uses
ALTPSA1\\0 + LEactorScrollX/Y +16records of LEuint16x/y and bytes type/state/z/plane,
140bytes total. Camera must match existing terrain evidence. Native captures now persist
the v2 value-owned `.sprite-input.bin` sidecar and include it in collision-safe naming;
old ALTPDA1 has no mantle actor observations and cannot be retrospectively promoted.
Probe opt-in `ALTTP_SPRITE_PRODUCER_EVIDENCE=path` runs research producer tests.

Remaining generality: exact producer family dispatch is currently EEcastle mantle
only. Sanctuary mantle/other moving obstacles require their own verified draw
contracts, not a generic artwork guess. No mirrored producer is invented; group
mirror matching remains supported independently. Actor position is observed at
EndFrame, but mantle draws BEFORE `Sprite_MoveXY`; on frames where movement crosses
an integer coordinate the final actor anchor can differ from emitted OAM by1px.
Those frames safely reject until draw-time producer anchor snapshots or proven
pre-move reconstruction are supplied. Current tests require exact pose ownership,
not tolerance-based proximity. This final motion gap is now addressed for EE:
`Sprite_MoveX`86:E932 adds signed velocity*16 to8-bit subpixel/worldX. Adapter
accepts exact current anchor OR mathematically inverted single xVel2 step only
when live motion observation exists and yVel0. Six OAM entries and all24children
must still match exactly. Regression crosses a whole-pixel step and resolves50;
the same1px anchor discrepancy without motion evidence rejects. New ALTPSA2
supports this: header/camera unchanged,16records12bytes each (original8bytes,
xSubpixel,xVelocity,yVelocity,hasMotion),204bytes total. Native captures write v2;
v1research evidence remains readable. Fresh live capture/gameplay remain pending.

**Grouping and producer-backed floor support are implemented. Actual local
geometry still awaits user authoring; fresh gameplay verification and complete
room approval remain pending.** The historical capture alone lacks actor-plane
evidence; the independent research continuation above validates the new adapter.
No authored profile was changed.

## Runtime rule and local sources

`sprite_obstacle.h` matches complete authored dynamic footprints from owned
canonical pixels, not OAM ordering, priority or hash-only membership. Repeated
scanline records collapse to physical 8x8 origins. H/V mirrors, repeated hashes,
translation and independent copies are supported. Ambiguous/overlapping/incomplete
claims fail for the entire assembly. A wholly hidden member cannot be proved.

All explicit contacts require a common observed gameplay-plane movement-floor
component and base at their transformed underside/immediately outside pixel row.
Wall artwork/structural offsets are not support. Multiple valid gameplay planes
are a conflict even when their bases agree; Link's plane is never a tie-break.
Settled current-room support only; airborne displacement and producer ownership
remain unsupported. A resolved record gets `floorBase + member.z*8`, replacing
the earlier legacy OBJ fallback/ramp datum, without changing local arrays.

Local-source availability now uses `S9xRemasterResolveFrameGeometry` for each exact
instance: placement layers, H/V variants, facing variants and named context
selection. It does not mistake base-only metadata presence for a local source.
Prior invocation's unresolved output does not suppress rechecking authored layers.
Diagnostics additionally expose `missingLocalInstances` and `supportPlaneMask`.

**Missing-local semantics:** clear the guessed offset, set explicit Unresolved
(or Conflict when support itself conflicts), and preserve all authored arrays.
This is not flat local0 geometry, not support base0, and not solved coverage.
Unresolved/Conflict instances keep placement role Unknown and deactivate occurrence
normal/occlusion overrides (including normal-ignore-transform), as required by
`S9xRemasterValidOccurrenceGeometry`. Underlying array bytes remain immutable;
normal/occlusion flags must be restored by a later profile/context application,
not inferred from stale bytes. Placement-height data remains stored but inactive
under explicit unresolved geometry. Regression tests round-trip both statuses and
all seven actual post-adapter packets through capture serialization.
The renderer must respect the existing occurrence-geometry status. Manual local
authoring plus member Z is necessary, but insufficient when support planes overlap.

`S9xRemasterBuildDungeonHeightMap` creates `diagnosticStorage` unconditionally
inside its placement-aware solve and publishes it as `roomDiagnostics`; this is
not gated by requested debug overlays. The real probe asserts its availability
on both BGs without requesting an overlay. Legacy compiled maps do not retain
this runtime evidence, so selecting one leaves sprite support unresolved. The
parent can require a current placement-aware solve when sprite groups are active;
this change does not fabricate component evidence for compiled maps.

## Exact immutable evidence

Capture prefix:
`~/Library/Application Support/Snes9x/Remaster/frame_room051_003.s9xrmf`.
The native complete sidecar is ALTPDI1, not ALTPPD1. Existing
`alttp_replay_live_height_evidence.py` losslessly wraps its pinned VRAM/maps and
collision planes into solver-only ALTPPD1/ALTPAT1 (no invented gameplay pixels).

Supported ROM SHA256:
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Profile byte FNV1a64 `19b52d36745a7714`; full SHA256 input pins are in `gate.json`.
Actual state: room81, previous97, settled submodule0/collision0, Link763/2656,
plane0, scroll643/2561, both graphics bases16384, rise50. Terrain sidecar loaded.
**110,809 BG pixel identity/flip/canonical-camera samples match complete inputs.**

Pinned two-BG solve, using BG1 support relations in BG2:

| Layer | Required planes | Missing planes | Solved/missing walls | Structural conflicts | Equality records |
|---|---|---|---|---|---|
| BG1 | 962 / 3720 | 886 / 3720 | 0 / 288 | 419 | 48 |
| BG2 | 962 / 3720 | 0 / 0 | 564 / 24 | 4 | 0 |

BG2 counts reproduce the captured state exactly. BG1 deficiencies are retained,
not imported into the independent BG2 sprite support lookup.

## Actual assembly result: two blockers, not a solved floor

Six OAM slots116..121 contain **24 physical 8x8 members / 192 records** at
origin135/35. Test-only group uses the observed footprint, six explicit bottom
contacts, Z0; no group or arrays are written into a profile. All192records lack
effective local height. Test-only nonflat placement arrays let us independently
exercise contact solving; they are NOT proposed/validated object geometry.

At room row8, contact cells33..38 reproduce:
- plane0 floor component8665, floor/plane base50;
- plane1 floor component12352, floor/plane base0.

Cell39 has no plane0 floor but does have plane1 component12352/base0. Enough
plane0 contact support survives for all six members, and every plane1 contact is
also witnessed. Therefore the whole assembly reports **Conflict**, no chosen
base, with and without test local arrays. All192records are claimed,0unclaimed.
There is no producer-backed assembly-plane attestation in the current generic
context adapter. Existing terrain producer evidence only covers its own supported
actor kinds, not this moving assembly. Correct next shared capability is exact
actor/OAM producer ownership and gameplay-plane evidence, not a room/hash/Link
plane or lower-base exception. This remains a runtime-height gap.

## Available Room051 packet audit

All seven available packets match the same complete24-member footprint:

| Packet suffix | Origin | Claimed visible records | Unclaimed |
|---|---|---:|---:|
| none | 121,35 | 191 | 0 |
| 001 | 119,-2 | 180 | 0 |
| 002 | 135,35 | 192 | 0 |
| 003 | 135,35 | 192 | 0 |
| 004 | 135,35 | 192 | 0 |
| 005 | 138,35 | 192 | 0 |
| 006 | 138,35 | 192 | 0 |

Total **168 member placements / 1,331 visible instance claims**, zero unmatched
visible candidate records; no native resolved assemblies. Only003has matching
complete sidecars. Other packets are matching-only audits: the probe deliberately
withholds003's context/map rather than treating a different camera/state as pinned.
001proves clipped negative origin recovery. This seven-packet corpus is not a
hundreds-of-rooms height audit.

## Mandatory validation v2

Ran over complete pinned ALTPPD1/ALTPAT1, own unchanged captured profile, signed
ALTPHM3 map, runtime effective local placement inventory and verified ROM.
**Exit1 / success=false.** No historical/reference directed stair playback was
provided, so reachable-support graph uses the actual pinned seed and no invented
stair edges. Full room is still the geometry input; its reachable plane
denominators are explicitly **[76,0]**, not all potential solver support.

- reachable support76/76; missing placements0; plane overlap0;
- fallback24/24, fallback conflicts0;
- BG2 walls564solved /24unknown;
- one runtime structure conflict record with count4;
- authored BG obstacles32/48solved,16missing,0conflicting; contacts8/12witnessed;
- doorframe marked placements8/12solved;
- compiled map0missing /52mismatches;
- directional edges7008:1930continuous,360incompatible-facing boundaries,
  2292independent-support/occluder,108lateral,168missing-local,
  1694missing-base,456unresolved-contact ownership; aggregate missing2318,
  reported conflict0.

These preexisting failures and absent stair playback prevent room approval.
The BG validator does not cover OBJ footprint support; the separate native probe
retains its two-plane conflict instead of counting it as solved BG coverage.

## Reproduction and artifacts

Artifact directory:
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room051-sprite/`.

- `probe.json`: actual contact evidence, both-BG counts, all7packet matching audit.
- `replay-manifest.json`, `input-comparison.json`: immutable pins/complete map parity.
- `live-solver-input.raw`, `.attr`, `live-runtime.json`, `review-map.bin`.
- `live-state.json`, `live-graph.json`, `validation-v2.json/.svg`, `gate.json`.
  `gate.json` retains exact validator command, exit/stdout/stderr and all input SHA256.

```sh
c++ -std=c++17 -O2 remaster/sprite_obstacle_probe.cpp -o build/sprite-obstacle-probe
c++ -std=c++17 -Wall -Wextra -pedantic -fsanitize=address,undefined \
  -fno-omit-frame-pointer remaster/sprite_obstacle_test.cpp -o build/sprite-obstacle-test-sanitize
```

Probe usage: `probe pinned-capture [other-captures ...]`, JSON stdout. It reads
the pinned profile/templates/terrain/complete inputs and asserts actual003's
192missing-local and192claimed/two-plane-conflict regression. Existing replay
helper supplies validator inputs; `sprite_obstacle_gate.py --capture ... --output
... --rom ...` runs the complete-room gate without invented transitions.

Portable tests and ASan/UBSan pass, including variant-only flip/facing/context
local-source availability and recovery from prior unresolved status. No frontend
build or fresh interactive gameplay performed in this delegated scope.
