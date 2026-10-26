# Shared obstacle artwork and occurrence-based editor focus

## Scope / verdict

Room051 curtain authoring: **reviewable but incomplete**. This implements the
authoring/runtime capability, not a newly approved room-height map. Canonical
`East_facing_curtains` and all authored profile data are untouched. No West group
is created automatically. The read-only regression builds a temporary West
variant with precisely `221db4615adb55fd -> f91826cad69954c9` and
`ad3b150df3ea5b3b -> a2af9baf8d303df7`, retaining the other six occurrences.
All four mirrors yield two independent eight-member claims; absent support stays
missing, never defaulted to a floor height.

## Behavior and safety

- Existing member clicks focus a group only when its complete visible footprint
  contains every selected occurrence. Selecting a different object, even with
  shared artwork, returns to creation. Plain click replaces the selection;
  Cmd-click toggles individual cells. No hash fan-out or inferred completion.
- A mixed old/new selection is a new proposed footprint, not edits to the old
  group. Review it before Create. New groups retain existing Z=0/south-boundary
  defaults; they do not copy East's authored contact arrangement automatically.
- Competing complete groups and stale cells disable creation/editing. Shared
  inventory identities alone do not pick the first/last group. Explicit saved
  footprint controls keep their focused group until a scene click.
- Partial/occluded motifs cannot auto-focus by occurrence. Runtime still rejects
  incomplete, overlapping and ambiguous footprints. A complete shorter subset
  also vetoes a larger claim: no longest-match or profile-order precedence.
- Native parser/serializer and Python validation permit shared hashes. Python's
  identity index now returns sets of group indexes, not an arbitrary owner.
  Other consumers use membership only. Name-keyed merge preserves independent
  group edits and shared members. No grammar/schema bump is required.
- Gameplay-plane selection, floor-solving and authored local height composition
  are unchanged. BG and OBJ remain isolated.

## Cost

BG matching builds a one-to-many identity-to-cell index, selects each group's
rarest observed member as anchor, and checks one full footprint per compatible
origin/mirror. For N room cells, G groups, M members/group and A occurrences of
the anchor, work is O(N log N + sum(G: M + 4*A*M)); A <=4096, often zero/one.
There is no Cartesian product of shared-tile occurrences. This is room-geometry
construction/cache work, not a per-pixel lighting branch.

OBJ matching uses the analogous rarest-member index (rather than trying every
observed tile against every member). Overlap uses exact 8x8 spatial occupancy,
not candidate-pair/member-pair products; signed/offscreen children are retained.
Each candidate visits 64 pixels/member and their actual spatial occupants.
Same-group semantic-symmetry deduplication retains the existing scan of prior
claims; that residual candidate-count cost is polynomial, not combinations of
member occurrences. No millisecond benchmark claim is made.

## Verification / limits

Portable editor, BG runtime, OBJ runtime, clipped-producer, oriented-obstacle,
animated-footprint and merge checks pass. Python authored-obstacle (16), atlas
profile (10), semantic-atlas (13), activation (2), and Cocoa layout (19) checks
pass. Tests cover shared motif disambiguation, all mirrors, complete subsets,
duplicate competing claims, source isolation, stale selection, roundtrip and
disjoint named-group merges. The independent room validator suite (26) and
ASan/UBSan editor + OBJ runtime checks also pass. Room051 `_003` read-only editor capture still yields
24 sprite occurrences / 1,518 pixels / six initial contacts.

The older optional `obstacle_contact_completion_test` assumes every sprite group
is a 16-member incomplete mantle; current canonical data does not satisfy its
`group.members.size()==16 && occurrences.size()==24` assertion. No authored
profile was altered to bypass it. The movement-capture test additionally needs
its external case05/RGBA arguments; it is not a fixture-free command.

arm64 macOS Debug build succeeds. A first full curated run detected source
changes during execution and aborted after 13 passes; the final immutable-input
rerun passes **33/33** (222.66s). Artifacts are in
`build/obstacle-shared-tests/final-gate/20261023T183942Z-87enuc24/`.

Mandatory room-wide validator was attempted with complete Room060 default-state
door graph, ALTPPD1, ALTPAT1, pinned state and freshly compiled runtime solution
using the untouched canonical profile. It exits **1**, not room approval: authored
obstacles have 20 required/0 solved/20 missing/0 conflicting cells, with four
missing contact witnesses, and no independent/runtime ownership mismatches.
The broader gate also reports 0/711 valid walkable placements, one conflicting
component, 28 constraint issues and 536 unknown / 480 solved wall cells. Its
24 overlapping-plane cells remain independently represented. These unresolved
fixture/profile inputs were not repaired or hidden by this authoring change.
The initial non-runtime invocation fails with the tool's existing missing-Link
state path; supplying the complete runtime output produces the actual report.
Artifacts: `build/obstacle-shared-tests/room060-runtime.json`,
`room060-validation-v2.json`, `room060-validation-v2.svg`. No compiled placement
map was generated. No Room051 whole-room or corpus-wide height success is
claimed. Fresh interactive click/Create/Undo/save/reload and matching gameplay
captures are still required before approving a newly authored curtain group.

## Independent final verification

Parent-session full portable plus production Metal gate passes **34/34** against
the final unchanged sources. Results:
`build/obstacle-shared-parent-verification/20261023T184727Z-9uk86a8a/results.json`.
Native inspector layout checks pass19/19. The arm64 Release build succeeds;
product `build/release/Snes9x.app`, log
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/curtain-groups-release.log`.
The canonical profile is byte-identical to the initial investigation baseline;
no West group or contact arrangement was written on the user's behalf.
Whitespace checks pass. Interactive authoring and effective room-height approval
remain outside these automated checks.
