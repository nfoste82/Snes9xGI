# Directional doorway support

Room051 / exact071 and sampled corpus: **reviewable but incomplete**. The new
metadata/runtime capability is implemented; room-wide approval remains blocked
by missing authored directional contacts and other existing geometry gaps.

## Contract and reusable repair

`RemasterTileRole::Doorway` is appended at ordinal10; actual `Door` stays8 and
`Particle` stays9. `RemasterSupportDirection` is `uint8_t`-backed:
Unknown0, North1, South2, East3, West4. Asset and frame metadata contain
`bool doorwaySupport=false` and `RemasterSupportDirection
doorwaySupportDirection=Unknown`. Profile v34 writes `doorway_support` and,
only when true, `doorway_support_direction="north|south|east|west"`.
True requires Doorway and a direction; false plus a direction is rejected.
Writer validation rejects invalid combinations instead of silently dropping them.

Profiles before34 migrate old Door into Doorway. Capture v39 adds support and
direction bytes after the doorway-mask payload (two bytes for every metadata
record); captures before39 migrate ordinal8 into Doorway after decoding the
historical role rules. New Door leaves have neither doorway masks nor support;
no closed/locked leaf inference was introduced. Shared canonical metadata was
migrated: 28 canonical assets, eight semantic-sidecar entries. Sidecar metadata
version2 distinguishes new actual Door from old doorway-role spelling. All
support flags remain false: no directions were guessed from artwork/visibility.
Canonical parsed comparison to `build/doorway-parent/profile-before.toml`
is exactly schema/role/default-support changes, preserving every authored array,
mask, normal, alternate geometry layer and unrelated field.

The failing class was unmarked wall-seam / implicit inward-landing support:
hidden exterior floors can meet both doorway ends, so implicit or visible-end
selection cannot supply an admissible datum. Runtime now seeds only the direct
adjacent support in the marked direction, H/V transformed at placement time,
with reproduced occupied-body support and solved plane base plus flat authored
floor edge. No gap search or opposite-side vote is used. Adjacent members of
the same intact, unambiguous ROM assembly transport that datum, not seam-derived
height; signed authored frame/opening relief survives. Different marked bases
invalidate the assembly on that plane. Both gameplay planes remain independent;
conflicting visible bases are reported and application withholds their offset.
Alternate local-source selectors remain unsupported for this proof rather than
being certified from an inactive default. Actual Door leaves cannot activate
doorway recognition, diagnostic wall ownership or border fallback.

Profile cache keys, placement equality/signatures and animation equivalence
include support metadata. Metadata-layer rotate/flip transforms direction;
individual mask/height edits do not change the independent metadata direction.
Python profile/semantic generation preserves marked support and distinguishes
new Door leaves; independent doorway audit follows explicit support contacts.

## Automated evidence and denominators

Artifacts: `build/doorway-backend/` contains targeted binaries/logs,
`own071.s9xrmf`, inventory/JSON, `live-runtime.json`, complete-room validation
JSON/SVG, and `corpus.csv`. Generated artifacts are review evidence, not source.

- Supported ROM SHA256
  `66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
- Current canonical profile SHA256
  `ed1fe7d1b75f089be910d70203d59d2b30ff025f361288881d5973e14ed151c5`.
- Exact071 own profile retains original SHA256
  `9ff34398d990e899eec578c02443013f4b0762bb7a1be35c02906de5a1a48134`;
  original serialized pin `c4651736b3e5651a` is checked through a versioned
  legacy-grammar hash transformation, separately from migrated serialization.
  Original companion files were not modified.
- Production EndFrame replay:147451 matching BG pixel identities/transforms;
  BG1 staircase16 members base0, overwritten BG2 rejected, named authored stair
  arrays unchanged; BG1 cap result288 solved walls/0 unknown/0 conflicts preserved.
- Complete-room v2 command was run with the original complete ALTPPD1 snapshot,
  matching ALTPAT1, pinned graph/state, own profile, compiled ALTPHM3 map and
  newly compiled runtime solution. Inputs are under
  `build/remaster-geometry/stair-attachment-review/`; outputs under
  `build/doorway-backend/validation-v2.{json,svg}`. **Exit1**, not approved:
  886/886 reachable supports, denominators `[886,0]`, zero unknown walls;
  four existing runtime structural conflicts;32/32 stair plane-members pass;
  0/12 required marked doorway placements solved,12 explicitly missing,
  zero accepted directional door claims/issues. Existing BG2 map gaps persist.
  Fewer door-seam issues are not repaired heights: unmarked claims were removed.
  Separate current-profile migration research gate is under
  `build/doorway-backend/current-profile/` and also exits1; it uses the new
  canonical profile pin, not a claim of original-capture visual agreement.
- Available own-input corpus:100 captures /200 BG solves in rooms002,011,012,
  022,041,051,061. Doorway cells2260; recognized assembly cells1444;
  ambiguous origins0; authored directional markers0; directional plane
  attachments0; marked frame unknowns1620; directional conflicts0.
  These are repeated sampled capture states, not all320 rooms/all states.
  No complete-room coverage claim follows from this inventory.

Targeted C++ tests pass: new `doorway_metadata_test`, extended
`dungeon_door_foot_test` (all axes/flips, signed bases -17/+13, both-end floors,
recognized member propagation, unrelated/ambiguous member rejection,
contradictory marked supports), `dungeon_semantic_cache_test`, `doorframe_test`
(v38 legacy Door read and v39 distinction), `doorframe_runtime_test`,
`doorframe_placement_test`, `dungeon_frame_recognition_test`,
`dungeon_door_band_test`, `dungeon_door_jamb_height_test`,
`door_opening_height_test`, `dungeon_owned_wall_projection_test` and exact071
stair/cap replay. Python targeted tests pass: doorway metadata3, doorframe
metadata3, independent frame audit1, atlas-profile10, semantic-atlas13,
complete-room-validator26. `git diff --check` passes.

## Remaining work / handoff

No fresh interactive Snes9x capture or visual approval was performed. Parent
owns aggregate runner registration/tests and arm64 macOS Debug build.
The smallest next action is evidence-backed canonical directional support
authoring for reusable doorway parts, then rerun corpus and complete-room v2.
Missing final-writer/overwritten ownership and placement-local alternate-source
compilation remain general capabilities to extend; no whitelist or room repair
was substituted. Runtime doorway heights remain explicitly unsupported for
unmarked legacy art despite restored metadata classification.

## Parent integration verification

The Metadata inspector exposes the Doorway support checkbox and required cardinal
floorward direction. Metadata copy/reset/Undo and capture-to-editor paths retain
both fields; Doorframe editing now targets Doorway, not Door. See
`doorway_authoring.md` for usage and manual acceptance checks.

Parent independently compared the parsed pre-migration canonical profile against
the migrated profile: exactly28roles migrate, schema31becomes34, and all other
parsed values are unchanged apart from false support defaults. Source-layout
tests pass25/25; Python doorway/doorframe/validator tests pass32/32. ASan/UBSan
metadata and directional support regressions pass. Independent exact071 replay
matches147451pixel usages and preserves16stairs and288solved BG1 walls.
arm64 Release and Debug builds pass (`build/doorway-parent/*-build.log`).
Parent reran complete-room v2 with the new runtime solution:
`build/doorway-parent/validation-v2.json`, exit1. No whole-room or live visual
approval is claimed. Fixture-free metadata and directional-contact regressions
are registered in the curated full gate; aggregate result is recorded separately.

The first aggregate passed40/41 and exposed an obsolete frame-recognition fixture
that still expected implicit wall-seam bands16/8/0 and solid-plane attachment.
That fixture now requires explicit transformed contacts, checks unmarked rejection,
preserves authored relief at base0 and rejects opposite-side/unsupported-plane
support. Its288recognition cases pass at `-O0 -UNDEBUG` and under ASan/UBSan.
The exact071 production replay also passes ASan/UBSan. Parent atlas/semantic/audit
Python tests pass24/24. Parent independently confirmed the100capture/200BG corpus
inventory:2260doorway cells,1444recognized frame cells,0authored support markers
and0directional attachments; no authoring or complete-room repair is implied.
Legacy migration was independently checked against the saved parsed canonical
profile; current profile SHA256 remains
`ed1fe7d1b75f089be910d70203d59d2b30ff025f361288881d5973e14ed151c5`.
The final UI sidecar fallback respects metadata_version2 so actual new Door leaves
are not accidentally remigrated. Both final arm64 app configurations were rebuilt
after that UI correction. No fresh interactive authoring acceptance is claimed.
Parent also reran the separate current-profile research v2 with the migrated
canonical profile and new matching runtime solution; it remains exit1
(`build/doorway-parent/current-profile-validation-v2.json`).

Extra all-ALTTP Python discovery is **not passing**:252tests ran with4Pillow
(`PIL`) import errors,4skips and1diagonal-array expectation failure. The diagonal
failure was reproduced independently using the saved pre-doorway profile, so
authored arrays were not changed to satisfy it. Log:
`build/doorway-parent/python-all.log`. These limits are separate from the focused
56passing metadata/atlas/validator tests and the curated CPU/Metal gate.

Final stable curated full+Metal gate passes **41/41**:
`build/doorway-parent/final-tests/20261024T042513Z-t9chmy43/results.json`.
Parent ASan/UBSan semantic-cache regression and final `git diff --check` pass.
Release/Debug builds are successful. Complete-room gates still exit1; support
authoring, overwritten ownership and fresh interactive review remain outstanding.
