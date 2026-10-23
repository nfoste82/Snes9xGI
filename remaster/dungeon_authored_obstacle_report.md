# Global authored obstacle footprints

## Native editor implementation and parent verification

Metadata mode now records clicked occurrence cells independently of the legacy
identity selection. Cmd-click selects each member of one example, including
repeated identities. Create Obstacle Group validates aligned same-BG cells and
single-group identity ownership; defaults Z0 and editable south-boundary contacts.
The group section provides name editing, member dropdown, clickable 2D footprint
(Z labels, * contacts), signed Z, support checkbox, apply, delete and Show Entire
Group. Show Entire Group highlights member identities throughout the capture.
Profile-wide undo/redo, dirty tracking, save and active runtime profile publication
include groups. Existing pixel clipboard/reset operations leave group definitions
independent. Missing authored local sources are reported in the group section.
Runtime support/conflicts are evaluated on subsequent gameplay frames; the paused
capture is not re-solved from incomplete evidence and the UI states that limitation.

Parent independently compiled strict-warning editor and runtime regressions:
occurrence origins/all raster flips, repeated identities, duplicate group ownership,
layout rejection, schema roundtrip, runtime composition and exact cache tests pass.
Independent Python proof8/compiler10/HTMLsync11 tests pass.
Arm64 Release/Debug app builds and diff check passed after native integration:
`/Users/nic.foster/.local/share/opencode/tool-output/tool_1297cd080001uRgpRN91Senk2m`.
Products `build/release/Snes9x.app` / `build/debug/Snes9x.app`.
Running user app untouched; interactive editor workflow/gameplay confirmation
remains pending. No user obstacle definitions were automatically authored.

Native additions: `remaster/runtime/obstacle_group_editor.h`,
`remaster/tests/obstacle_group_editor_test.cpp`, `macosx/mac-os.mm`.

## Compiler / HTML preservation and reference-corpus follow-up

Schema22 payload integration is complete in the Python compiler and registered
semantic HTML synchronization. `validate_groups` rejects invalid group keys,
pre22 group payloads, member keys/types/ranges, duplicate positions/names,
cross-group identities and missing explicit contacts. Registration refresh and
HTML save perform this validation before replacing any file. Existing groups
participate in revision hashing, so external group edits invalidate stale browser
saves. Semantic saves retain the group payload and schema22 unchanged.

`_render_profile` now isolates actual group/member table blocks rather than
swallowing them into the last asset. Tests cover groups before, between and after
assets. Group text and parsed payload survive roundtrip. The compiler protects
grouped assets' authored layers and leaves absent layers or entirely absent asset
tables absent; no metadata is inferred from group membership. Missing sources
are reported as `obstacle_missing_local_sources`. Generated coverage now counts
actual layer presence instead of assuming every candidate has all layers.
Diagonal semantic activation also avoids reclassifying grouped assets. These
changes do not write to user profiles; only temporary test/candidate outputs were
used. Parent mac-os.mm and obstacle_group_editor.h were not edited.

Portable suites passed: atlas compiler10, HTML semantic atlas11, diagonal semantic
activation2, doorframe metadata3, authored obstacle8 tests. Two existing mocked
compiler fixtures needed actual temporary semantics JSON files because the
existing diagonal activation reads them; fixtures now supply those inputs.
`git diff --check` passed. Parent owns app build/interactive validation.

Reference corpus was available from the prior346-context manifest. Extended
`dungeon_geometry_cache_probe.cpp` records ORIGINAL authored denominators/contact
evidence before its synthetic animation and compares those complete inventories
against a forced full solve. The existing animation/cache versus forced solve
comparison additionally covers bothBG maps/evidence/diagnostics and exact frame
packet bytes. No group variants were auto-authored for animated identities.

Both temporary example profiles were run across **346/346 contexts in319 rooms**,
zero missing-input unsupported or refused contexts, immutable profile hashes:

* Column:200 placed authored members,168 solved,32 unresolved;0 unmatched
  footprints;50 contact claims/50 witnessed;4 conflicting assemblies. Every
  original and synthetic-animation cache/full comparison matched exactly.
* Statue:480 placed members,360 solved,120 unresolved;0 unmatched footprints;
  160 contact claims/122 witnessed;1 conflicting assembly. Every cache/full
  comparison matched exactly. Contact/assembly counts are not member counts.

Artifacts `$ROOT/authored-obstacle-review/reference346.json` and
`reference346-statue.json` include every context, support missing counts,
unknown-wall/conflict counts, timings, snapshot paths and profile pins.
Scope is reference default/entrance graphics contexts, NOT all game states or
Snes9x visual approval. One of320 room IDs is absent in the available corpus;
special/dynamic states remain unenumerated. These conservative unknowns/conflicts
were preserved rather than treating cache equivalence as geometry approval.

Commands (repeat with example050 and reference346-statue.json for statue):

```sh
c++ -std=c++17 -O2 remaster/tests/dungeon_geometry_cache_probe.cpp -o "$ROOT/authored-reference-probe"
python3 -B remaster/tools/ALttP/alttp_audit_authored_reference_corpus.py \
  --manifest "$ROOT/room061-057-review/corpus-after.json" \
  --profile "$ROOT/authored-obstacle-review/example061/temporary-example.toml" \
  --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc \
  --probe "$ROOT/authored-reference-probe" \
  --output "$ROOT/authored-obstacle-review/reference346.json"
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_alttp_atlas_profile.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_alttp_tile_semantic_atlas.py
```

Whole-room gates still have the unrelated failures documented below; no runtime
height policy changed during this compiler/probe follow-up. Smallest next action:
parent build/UI verification, then user-authored reviewed groups and fresh capture.

## Independent-proof follow-up (supersedes the historical blanket-gate limitation below)

Implemented `remaster/tools/ALttP/alttp_authored_obstacles.py` and integrated its independent
proof into validation-v2. New group profiles are **not blanket refused**. The
Python proof reconstructs exact full footprints, transformed expected artwork,
safe same-group semantic symmetry, overlapping claims, required contacts and
member offsets. It independently reproduces the ordinary directed Link-body
movement predicate/components from completed collision arrays on active planes;
it never uses C++ assembly IDs/contact/component claims as evidence. Floor bases
remain inputs from the existing independently gated room solve (this feature
does not replace the floor solver). Missing local arrays retain the full placed
identity denominator and report `missing_local_metadata`; they do not fabricate
profiles or claim support. Explicit contact keys and at least one contact are
mandatory in Python and native schema22 validation.

Native runtime fixture JSON now includes independent BG1 plane floor bases and
the actual frame-applied64pixel effective arrays for each authored member,
including offscreen members. Python compares these against reconstructed base+Z
and unchanged local arrays. Actor maps keep supporting floor bases, not memberZ;
compiled visible maps use the structural member offset, not max-merged support.
New-group classifications are excluded from ordinary fallback/wall templates.

Temporary examples were derived from ONE footprint per research profile, without
modifying user profiles or captured inputs. Deliberately signed member offsets
exercise mechanics; these are research authoring, **not user-approved geometry**:

* own061057: BG2 column example(12,28),2x4, topZ+1/nextZ-1/restZ0; only bottom
  row contacts. Six independent copies: **48/48** placed members solved,
  zero missing/conflicting members, zero Python/native discrepancies. Supporting
  plane0 denominator1570, plane1 absent. Actual frame-composed arrays pass.
* own050: BG1 statue example(44,11),2x3 with same signed row policy. Two copies:
  **12/12** members solved, zero missing/conflicting members or discrepancies.
  Supporting plane1 denominator3564, plane0 absent. Actual frame arrays pass.

New-group corpus research replays (separate profiles per capture, not current
user metadata substituted): each example audited all **152** local packets;
**43** complete matching captures replayed (rooms050/060/061/062), **102** lack
complete sidecars, **7** are stale packet/map/camera refusals.

* Column example:672 required members,240 solved,432 missing local metadata,
  zero conflicting members, zero Python/native composition mismatches. Older
  own-profile snapshots lack member sources; these remain visible blockers.
* Statue example:24 required/24 solved, zero missing/conflicting/mismatches.
* These packet counts are sampled states, not hundreds-of-rooms coverage.

Mandatory whole-room validators ran with complete graphs, snapshots, pinned
temporary profiles, fresh runtime JSON and compiled signed maps:

* New-group own050: group proof passes; room gate exits1 for unchanged489 unknown
  walls. Reachable support376/376, zero support missing. No blanket group block.
* New-group own061057: independent group proof passes; room gate exits2 for the
  preexisting missing independent diagonal preview proof at BG2(2,38). Not waived.

Proof artifacts under `$ROOT/authored-obstacle-review/example050/`, `example061/`,
`group-corpus/`, `statue-group-corpus/`: temporary TOML, runtime JSON/map,
independent-proof JSON, manifests/summary and room validator logs/results.

Commands:

```sh
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_authored_obstacles.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p test_alttp_validate_room_heights.py
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_runtime_room_heights.cpp -o "$ROOT/authored-obstacle-solver"
```

**8/8 new Python failure-class tests**, **24/24 existing validator tests** pass.
Tests cover all mirrors, repeated identity/different signedZ, negative bases,
two independent copies, bothBGs/planes, component/base/plane conflicts, explicit
contacts versus noncontact heads, missing metadata, overwrites, safe symmetry,
differentZ symmetry rejection, overlap, actual body narrow-strip rejection,
tampered runtime pixel arrays/inventory, and proved-group validator success with
unrelated wall-gate failure preserved. Portable native group regression remains
passing. No parent `mac-os.mm` or `obstacle_group_editor.h` edits.

Smallest remaining actions: parent build/UI authoring; user-reviewed groupZ and
fresh matching gameplay validation. Whole-room unrelated wall/diagonal evidence
gaps still prevent publication, but independent group validation is implemented.

Verdict: **reviewable but incomplete**. Shared native runtime implementation is
functional; no user profile/group metadata was authored. No room is approved.
Frontend integration/build belong to the parent. No application lifecycle or git
staging/commit/exclusion operations were performed.

## Shared format and API

Profile schema **22**, frame schema **30 unchanged**. Existing profiles serialize
identically when they have no obstacle groups. Use:

```toml
[[obstacle_groups]]
name = "example"
[[obstacle_groups.members]]
tile_hash = "v1:4bpp:0123456789abcdef"
x = 0
y = 0
z = -1
hflip = false
vflip = false
support_contact = true
```

`RemasterProfile::obstacleGroups` is `vector<RemasterObstacleGroup>`; group fields
are `name`, `members`. `RemasterObstacleMember` fields are `tileId`, signed integer
`x/y/z`, `hflip/vflip`, `supportContact`. All member keys are required in TOML.
Names follow existing ASCII profile names and must be unique. X/Y are -63..63,
bounding span at most 64 tiles; Z is -4095..4095 tile units. Maximum 4096 groups
and 4096 members per group. Duplicate positions are rejected. One identity can
belong to only one obstacle group, but repeat at different positions/Z within
that group. Existing material asset-group membership is independent.

`S9xRemasterValidateObstacleGroups(groups, diagnostics)` validates in-memory
authoring before publication. Parse and validated serialize use the same check.
Missing height metadata is NOT a profile-load error; runtime refuses matching
that footprint and retains explicit unresolved members. Nothing auto-authors
height/normal/role arrays. Dungeon matching currently consumes 4bpp BG placements;
2/8bpp identities may persist but are not supported dungeon placements.

## Runtime contract

Relative coordinates mirror about the selected example origin, with expected
artwork flips XOR the global footprint H/V transform. All member identities,
flips and authored local sources must match. Palette/priority are not identity
or height evidence. Copies remain independently owned. Overlapping candidates
reject every involved footprint, not just shared cells. Exact same-group H
symmetry collapses only when all occupied slots have identical Z/contact
semantics; differing Z or transformed contact direction remains ambiguous.

Every declared contact must have occupied movement support at its own cell or
one immediately outside its canonical south underside. Global V reflection
changes that edge to north; member artwork V flip does not. Sparse/arbitrary
width footprints are supported: no width2-foot assumption. A Z0 head is not a
contact unless explicitly marked. All contacts require one identical movement
component, gameplay plane and signed solved floor base. Separate planes remain
independent, and conflicting planes cannot be resolved by selecting the higher
base. These consumers introduce no floor/structural graph nodes or constraints.

Every rendered member offset is `supporting_floor_base + z*8`, added once to the
unaltered authored canonical pixel array. Actor/support plane offsets remain
their original solved bases, including under nonblocking artwork. Stale asset
roles/groups, fallback ramps, grounded clamps and placement previews cannot
replace this geometry. Both BG application paths and main/sub/independent scene
paths are tested. Offsets outside signed representable range or exactly legacy
unknown sentinel 255 reject, never clamp. ROM statue/column/pot/stair handlers
remain unchanged for profiles without groups.

Placement fields, entry evidence key, dirty profile geometry key, retained
geometry comparison, full inventory and eight-state exact cache carry all new
ownership/Z/contact dependencies. Changed identity belonging to a group cannot
use artwork-only geometry equivalence. Schema30 serialization is untouched.

Evidence: `RemasterDungeonHeightMap::authoredObstacleEvidence` records assembly,
group, support plane/component/base, member/contact/witnessed-contact counts and
conflict. Cell/solved/unmatched counts distinguish runtime heights from metadata.
Runtime JSON records these and all placed BG/plane group/Z/contact fields.

## Parent frontend integration

Add `\n[[obstacle_groups]]` to `RemasterNonSceneProfileOffset` in mac-os.mm, and
include groups in undo/copy/reset/save/profile sync. Current asset-only undo
snapshots cannot preserve group edits. The web semantic atlas is generated in
`remaster/tools/ALttP/alttp_tile_semantic_atlas.py`; it currently authors intrinsic roles,
not this profile-backed example footprint. No frontend files were touched.

## Verification

Portable command (outputs under preapproved `$ROOT` temp directory):

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Werror -pedantic \
  remaster/tests/dungeon_authored_obstacle_test.cpp -o "$ROOT/authored-obstacle-test"
"$ROOT/authored-obstacle-test"
```

PASS: schema22 strict roundtrip/required keys/duplicate ownership; sparse repeated
identities with different signed Z; all four mirrors; two independent50/0 bases;
negative evidenced floor; explicit contacts; missing local metadata; partial
overwrite; overlapping ownership; safe H symmetry/differing-Z rejection;
unsupported and two-plane conflicting contact rejection; authored pixel
preservation; both BG/main/sub/scene runtime composition; group-only cache edits;
eight exact states and cached/full complete map/evidence equivalence.
ASan/UBSan passed. `git diff --check` passed.

PASS existing obstacle_assembly, symmetric_column, obstacle_invariance,
obstacle_support, obstacle_support_component, geometry_cache, transition_cache,
pot_placement, scroll_ownership, authored_stair, stair_detection, semantic_cache,
placement_reuse, shared_face, unobserved_shared_structure, tile_semantics and
inspector_height tests. Small-stack512/544KiB passed with own061057 profile.
Python validator suite: **24/24** passed, including conservative new-group gate.

Unaddressed existing broad verification failures observed: remaster_test.cpp
line739 legacy previous-version deserialize; doorframe_test.cpp line115 packet
deserialize; root alttp-profile.toml small-stack load fails. The generated
alttp-profile-obstacle-heights.toml fails duplicate existing asset-group membership.
No packet/profile validity rule was weakened to hide these failures. A historical
`dungeon_stair_art_cache_test.cpp` command refers to a nonexistent file.
Parent must perform the arm64 Debug app build; no interactive check performed.

## Corpus and mandatory complete-room gates

Artifacts:
`$ROOT/authored-obstacle-review/`, where ROOT is
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode`.

Own-input replay audit: **152** local gameplay packets; **43** replayed, **102**
unsupported missing sidecars, **7** refused stale packet/maps/camera. Replayed
rooms are050/060/061/062. All source packet/profile/map/template hashes remained
immutable. Own061057 matches112531 pixels; own050 matches89217. These profiles
contain **zero authored groups**, so this is preservation evidence, not gameplay
validation of newly authored groups. Synthetic regression is the only declared
new-group corpus; no game-wide new-group coverage claim.

Fixture gate with immutable own061057 profile, ROM SHA256
66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb,
configured rise50, full ALTPPD1/ALTPAT1 and compiled ALTPHM3:

* 055: validator exit1; supports439/439; missing0; conflicts0; walls1602 solved,
  12 unknown; plane denominator[439,0].
* 060: exit1; supports711/711; missing0; conflict records1; walls1017 solved,
  9 unknown; planes[437,274].
* 061: exit2, independently refused diagonal preview at BG2(2,38), rather than
  pretending missing provenance is satisfied.
* Own050 pinned complete-room gate: exit1; supports376/376, conflicts0,
  walls419 solved/489 unknown. Own061057 gate also exits2 at missing independent
  diagonal preview proof. Both were rerun using fresh runtime JSON from unchanged
  full captured inputs/profile; preserved existing compiled map supplied.

New groups are explicitly blocked from room approval by the Python validator:
independent complete-footprint transform/overlap and movement-component contact
proof is still missing. This is not a debug-only implementation: effective native
runtime member heights work, but independent room publication evidence does not.
Next actions: parent editor/build integration, user-selected example authoring,
independent Python group/contact proof and matching fresh gameplay captures.
