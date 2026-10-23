# Doorframe placement runtime repair (2026-10-07)

Verdict: **room061/current-profile research replay: reviewable but incomplete**.
No room approval, no fresh gameplay comparison, no application restart/build.
Parent completed incremental arm64 Release and Debug builds after the final
corner-band and visible-plane fixes; both passed, as did `git diff --check`.
Parent independently reran the portable Doorframe runtime and region-audit tests;
both passed. Interactive gameplay comparison remains pending.
Protected profiles/semantic JSON were not edited.

## Inputs

Immutable working copies/artifacts:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/door-runtime-research/`.
Current `remaster/profiles/ALttP/alttp-profile.toml`: schema21, SHA256
`afea8d8e9cbce9c45393aa789e47c1fb2048e7e953ee3a0c076e1b0efd0ccf2c`.
Backup: schema19, SHA256
`7de9e8a208efab563d802e84e60650d302352ae0389e45dba8c9bc4b7910a5b7`.
Eight Door assets have present nonzero masks; current profile is newer than
available complete room061 sidecars. Last complete room061 packet is `_019`:
room97/0x061, settled submodule0, plane0, Link(763,3411), scroll(643,3304),
both graphics bases16384, map bases[4096,0], sizes[3,3], rise50.
Latest capture overall at inspection is room041_006; complete room041_005
predates the latest authored profile. Do not relabel room060 packets as061.
The corpus manifests preserve packet/state/input/profile digests.
ROM SHA256 is `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.

## Failure class and repair

Door mask metadata existed but was explicitly authoring-only. Door placements
were excluded from structural solving/fallback; application could still apply
an ordinary support offset to their entire tile. An opening may be a stair,
so a tile-wide offset is not permissible.

Placement compilation now retains canonical mask presence/value and recognizes
the twelve final column-major words of ordinary ROM door templates. All tile
identities/flips must match; priority-only changes survive. Partial overwrites,
ambiguous assembly ownership, and split/copy/lower operations are rejected.
Source offsets follow `RoomDraw_OneSidedShutters_North/East`,
`RoomDraw_NormalRangedDoors_West`, `Door_Draw_Helper4` and their ROM source
tables, not room/hash whitelists.

After the existing signed floor/wall solve, frame regions attach through
compatible masked seams to matching ROM perimeter bands or already attached
members of the same intact door assembly. Tangent contacts enforce equal seam
heights; axial continuation requires the authored one-pixel slope. At least
two marked seam samples must agree, and all accepted contacts must agree.
Conflicts invalidate the assembly on that plane. Separate plane records remain;
ambiguous visible-plane bases are not applied. This pass does not modify floor,
wall, mixed-floor, fallback or authored-height arrays.

Frame application uses canonical `tilePixel` (already transformed by H/V),
checks producer hash/flips, and splits instances by the proved region offset.
Marked pixels receive the contextual frame base. False pixels receive **no new
offset**, retaining authored height, including ramps/stairs. Absent mask remains
unknown; explicit zero means no frame. Stale stair-group membership cannot
disable the explicit Door contract. Existing instance offsets are preserved.
Capture instance serialization already retains split instance offsets; no new grammar or
capture version was needed. Height debug, inspector and lighting consume the
same effective instance heights, not a separate debug tint.

The runtime fixture adapter now exports `doorframe_attachments` with layer,
plane, source-template, canonical mask and 64 effective values. It exercises
actual frame application for each attached tile, including offscreen placements,
and checks every pixel against reconstruction and immutable authored arrays.
Validation v2 independently checks the final complete template, mask and
opening preservation and fails on missing marked placements. Region geometry
is intentionally not flattened into the tile-wide ALTPHM3 map.

## Actual research samples

Full fixture and room061_019/current-profile replay agree:

| BG2 position | authored span | frame base before → after | effective frame span |
|---|---|---|---|
| (5,31), (58,31) | 0..7 | unattached →16 | 16..23 |
| (6,31), (57,31) | 0..7 | unattached →8 | 8..15 |
| (7,31), (56,31) | 0..7 | unattached →0 | 0..7 |

At (6,31), mask `0000e0feffffffff`, canonical pixel32 remains7;
pixel33 changes6→14; pixels48..63 remain exactly7..0 twice. Rows34
have mirrored placement transforms. Canonical masks/arrays are not rewritten.
West/east full frame columns (5/58, rows31..34) receive16; six inner
frame-edge cells receive8/0. Source templates are0x2c6a/0x2dea.

## Gates and denominators

Three independently pinned complete ALTPPD1/ALTPAT1 default-state fixtures,
current immutable profile, ROM templates, full doorway graphs, both planes,
ALTPHM3 maps, JSON and SVG validation-v2 artifacts:

| room | support | planes | missing support | conflicts | solved/unknown walls | marked frames solved/required | v2 exit |
|---|---|---|---|---|---|---|---|
|055|439/439|439,0|0|0|1391/1|0/18|1|
|060|711/711|437,274|0|1|399/62|0/20|1|
|061|1556/1570|1570,0|14|0|704/8|16/28|1|

Frame validation issues0; unresolved marked placements remain missing rather
than default solved. Each fixture inventories16384 layer/plane records (8192
BG placements); the three fixtures total49152 layer/plane records. These
denominators include offscreen input, not viewport-only coverage.

Available live corpus:79 indoor packets;30 complete sidecar packets replayed
(room060:19,061:7,041:4),49 unsupported because complete sidecars are absent.
All are explicitly newer-metadata research replays. `corpus.json` records
per-packet frame/support/conflict/wall counts. Hundreds of room IDs, additional
switch/water/door states and complete provenance are not covered by this corpus.
No claim of complete-room or game-wide geometry coverage is made.

## Verification commands

Let `OUT` be the artifact directory above. Commands run from repository root:

```sh
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o "$OUT/solver-after"
python3 -B remaster/tools/ALttP/alttp_run_runtime_height_fixtures.py \
  --solver "$OUT/solver-after" --fixtures build/remaster-geometry/surface-audit-room-local \
  --profile "$OUT/alttp-profile.toml" --output "$OUT" \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc
python3 -B remaster/tools/ALttP/alttp_audit_doorframe_corpus.py \
  --captures "$HOME/Library/Application Support/Snes9x/Remaster" \
  --solver "$OUT/solver-after" --profile "$OUT/alttp-profile.toml" \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --output "$OUT"
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_validate_room_heights.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_compile_room_height_map.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_doorframe_metadata.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_doorframe_runtime_audit.py'
git diff --check
```

Python tests passed23/3/3/1 respectively. Portable `c++ -std=c++17 -O2`
tests passed: doorframe_runtime, doorframe_placement, dungeon_mixed_floor,
dungeon_perimeter_flank, dungeon_terminal_corner,
dungeon_cross_bg_wall_support, dungeon_complete_assembly,
dungeon_placement_reuse, inspector_height. New runtime test covers all four
directions, all H/V combinations, priority-only overwrites, rejected partial
overwrites, rises37/50/83 and translated bases, frame/opening stair preservation,
absent/empty masks, ambiguous ownership, missing structural witnesses.
The existing shared untracked `doorframe_test.cpp` failed line115 when it
relabels a serialized schema29 packet as28; left untouched and reported, not
hidden by changing format tests. Parent should resolve that separate format
regression before shipping. No app was terminated and no git state modified.

## Follow-up: proved corner contacts and residual owner trace

Parent's initial Debug/Release builds passed; subsequent runtime changes here
still require parent builds. Concurrent capture schema is now30. Do not edit
the unrelated compatibility test which relabels current bytes as28. Request a
fresh **current-schema** packet, not a fixed29 packet.

The original remaining12 room061 BG2 placements were traced against final ROM
templates, complete structural outputs and independently pinned ALTPRV1 writer
provenance. Eight south-door cells `(30,56),(33,56),(30,57),(33,57),
(30,58),(31,58),(32,58),(33,58)` have intact south template0x2b88,
origin(30,56). Their contacts are solved corner branches0xc06/0xc46, not
straight perimeter0x2e8/0x2f8. The shared rule now accepts proved corner
branches through the same exhaustive marked-seam constraints. All eight
receive base0; this preserves their authored0..7/8..15/16..23 bands rather
than assigning neighboring bases0/8 indiscriminately. This rule also resolves
all18 marked room055 placements (previously0), demonstrating cross-room use.

Visible ownership is now explicit on each attachment. Structural witnesses
must own the visible map base; visibility propagates only through their same
door assembly's compatible seams. Hidden other-plane bases are retained for
plane auditing but cannot silently veto visible heights. Two genuinely visible
owners disagreeing still reject application. Link's active gameplay plane is
not used to choose rendered geometry (an underpass actor cannot lower a bridge).
Portable and independent Python tests cover both cases and original offset3.

Latest fixture gates, all exit1, unchanged support/wall/conflict counts:

|room|frames solved/required|missing frames|
|---|---|---|
|055|18/18|0|
|060|0/20|20|
|061|24/28|4|

The exact four remaining room061 placements are all BG2:

* `(30,5)` word0x0808 and `(33,5)` word0x4808: **not door dispatch**.
  ALTPRV1 final writer73, phase1 ordinal59, raw4986; ROM atlas identifies
  subtype1 object0x1e at(30,4). `dungeon.c:1758..1767` records an inter-room
  upnorth staircase and draws its4x4 template. Adjacent surviving decoration
  is writer72/subtype0 object0x3a; no adjacent solved classified wall band
  exists. Reusing a Door hash/mask in stair art does not transfer ordinary
  door ownership. Missing capability: canonical staircase **frame-part**
  template/part coordinates and its proved landing/band attachment, distinct
  from opening/treads. The intra-room stair table is not this handler.
* `(3,15)` word0x0809 and `(3,18)` word0x8809: intact west template0x2d5a,
  origin(2,15), ALTPRV1 writer123/door phase3 ordinal6 raw2. Neighbors
  `(3,14)` and `(3,19)` have authored32..39 but surfaceUnknown and no solved
  structural base. Surviving high-wall words across x2..4 are0x0892/0898/08a4
  and0x0893/0899/08a5; writer32 subtype0 handler0x61 and base-layout writer3
  respectively. These are genuine wall-handler art, but final metadata lacks
  activated structural ownership/support constraints at those contacts.
  Canonical32..39 is not an absolute placement base. Missing capability:
  exact surviving high-wall/trim assembly-to-support contract for the full
  high bands. Their immediate trim x5 is authored Floor0..1, deferred as
  nonflat by current classification; neighboring solved floor x6 cannot be
  copied across it. Resolving that supporting assembly would broaden floor/
  high-wall reconstruction, explicitly outside this narrow follow-up.

These gaps exist in both complete default fixture and all seven complete live
room061 states. A fresh viewport alone cannot close them. Fresh complete
current-profile input establishes current state parity; generalized staircase
part and surviving high-band support provenance/semantics remain required.
No arbitrary floor repair, protected metadata change or guessed offset was made.

Re-ran the three complete fixture validators and all79-packet corpus after this
change:30 replayed,49 missing complete packets. Seven room061 research replays
each have24 frame regions/4 unknown/0 frame conflicts; room060 remains unchanged.
All nine narrow portable tests passed again. Validator Python23 and new region
audit1 passed; `git diff --check` passed. Current complete adapter adds
`doorframe_ownership` with final ownership and structural neighbors for tracing.
Artifacts/commands use the same paths above and were regenerated, not just
inferred from the initial report.

## Remaining reusable capability

Ordinary full-door recognition cannot prove partial/split lower-layer door
operations. Room060 exposes that failure class. Need dispatch operation,
remapped type, destination/background, part coordinates and overwrite history
to connect surviving split regions to the proper structural bands. North/south
door templates with no compatible solved wall witness also remain unresolved;
do not add nearest-floor attachment. Need verified high/low band ownership and
seam contracts for those assemblies. Room061 missing support is separate from
the frame-region solve and remains a blocker.

Next: parent build, then fresh settled room061/current-profile current-schema packet
with full dungeon-input/template/profile sidecars and matching gameplay views
of both east/west frames and stair openings. A capture is comparison evidence,
not a replacement for the failing complete-room validator. Do not hand back
the room as approved until all applicable gates exit zero.
