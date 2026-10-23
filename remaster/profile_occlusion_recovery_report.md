# Door-art occlusion persistence investigation and limited recovery

Verdict: three confirmed absent occlusion layers recovered; historical mutation
cause unresolved. This is persistence recovery, not room-height approval.
No authored heights, placement geometry, runtime solver or macOS lifecycle code
changed by this operation. No captured evidence was modified.

## Exact recovery

Only `occlusion` was added to these existing assets in
`remaster/profiles/ALttP/alttp-profile.toml`:

- `v1:4bpp:c9254894bb09de73`
- `v1:4bpp:9c0473118412613f`
- `v1:4bpp:de1930ab64c9b0de`

Each recovered line is byte-identical to the 64-zero array in the freshest
original saved capture containing that authoring:
`frame_room061_057.s9xrmf.profile.toml` (local mtime October 10, 22:28:20).
Capture066 (October 11, 10:45:21) and every subsequent saved capture profile
already omit those arrays. Filesystem mtimes are not proof of save-event order.

An entire byte-exact pre-restoration profile is preserved locally at
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/door-occlusion-pre-restoration-exact.toml`.
It contains 2,251,405 bytes; SHA256:
`925b3676928cf79b8121b7a9f23f3106639da53a75553921c4570665a9801788`.
Recovered profile SHA256:
`a062ea41c6ff654c1f47b0130b548be8287e96809958728f57bf1e847a65bc83`.
Removing exactly the three inserted lines reproduces the entire backup bytes.
The current c925 height (7-x on every row), every other field, ordering and
whitespace are preserved. No Door role, doorframe mask or geometry variant was
found for these three in the investigated saved authoring; none was invented.

The two additional masks absent in the same immediate capture transition were
NOT restored: `482a1674f4994ba7` (six covered columns) and `c72c0d2c90678917`
(five/six covered columns). The broader 39 older-backup/current differences and
134 historical capture-sequence disappearance candidates were NOT restored;
they may include intentional reauthoring or profile selection changes.

## Provenance and our cardinal edits

All 66 original saved capture profiles have matching serialized-profile FNV
pins in their dungeon sidecars. Current serializer roundtrips all 66 with no
detected height/normal/nondefault-occlusion/mask/facing/flip layer loss.
The capture079 viewport independently has absent occlusion for c925 and 9c047;
de193 is not present in that viewport. Current running in-memory state was not
inspected; current disk is not proof of the active profile.

Backup2, capture071 and cardinal-review before.toml have identical bytes/SHA256
`a78079295b18b0c5116aa7834403f1cacba81a8809d4310685e5e5c90b022aaf`.
The three target records are unchanged between before.toml and capture079 and
are not cardinal reauthoring targets. Regenerating the cardinal-only proposal
reproduced its recorded SHA256 and 263 changed assets with no removed keys.
Capture079 additionally changes four heights versus that proposal, on other
identities; none is a recovery target. The disappearance therefore predates and
is not caused by the recorded cardinal reauthoring transformation.

## Historical git evidence, not attribution

Reviewed full profile diffs and structured records for commits ac8ba407,
b56e5b65 and c6667eac, plus target states in 7f98606a and af2ab83d.

- All five occlusion fields are present through b56e5b65 (October 10, 23:02).
- The complete asset dictionaries at b56e5b65 equal capture057's dictionaries.
- b56e5b65 removes seven *other* coverage fields, not these five.
- c6667eac (October 13, 17:19) records exactly these five existing asset
  occlusion removals, with no other field changes on those five; its full
  profile diff also adds 660 assets. No existing asset is removed.
- af2ab83d retains the absent fields.
- The b56e5b65-to-c6667eac portable parser/serializer and semantic-atlas code
  diff does not introduce zero/mixed-coverage compaction. The storage predicate
  is identical: hasOcclusion and any sample less than 255.

The later commit records the changed state but cannot explain the mutation:
capture066 already lacks the five arrays two days earlier. No OS overwrite,
revert, user mistake, or specific application operation is established here.
The macOS save/edit lifecycle investigation is separate.

## Separate confirmed semantic-atlas bug fixed

A Door semantic annotation without doorframe_mask previously stripped an
existing independent mask. The atlas now preserves each member's own mask on
Door edits that omit the field. Explicit replacements (including an all-zero
mask) still replace it; leaving Door removes it. This is a different payload
from occlusion and is not claimed as the three-hash root cause.

The regression exercises actual registered-profile save/reload: distinct group
member masks, explicit empty, unknown, omitted mask, explicit zero/nonzero
replacement, non-Door removal and return to Door without resurrection. Height,
sampling, zero/mixed occlusion, normals, emission and flip geometry are checked
unchanged throughout.

## Verification

- `profile_occlusion_persistence_test.cpp`: portable profile write/read,
  repeated save after unrelated and same-asset height edits, serializer parse
  roundtrip, frame capture roundtrip, explicit zero/mixed coverage and empty
  Door mask preservation: PASS.
- `python3 -m unittest discover -s remaster/tools/ALttP -p test_alttp_tile_semantic_atlas.py`:
  12 tests PASS.
- Recovered complete profile strict parse/serialize/reparse: 1776 assets,
  70 nondefault coverage records, zero detected layer losses: PASS.
- Exact three-line-only recovery and source-array comparison: PASS.
- `git diff --check`: PASS.

No height assignment was changed, so no room-height repair/validator success is
claimed. No runtime code changed here; no macOS build or fresh gameplay capture
was performed. Reload/active-state confirmation belongs to the main lifecycle
investigation. Do not overwrite newer authoring or mass-restore the additional
coverage candidates without reviewing intent and evidence.

## Native lifecycle review

Main agent inspected profile loading, saving, undo/redo, layer painting/fill,
reset, paste and animation-copy paths in `macosx/mac-os.mm`. Native save checks
the disk snapshot before replacing the profile; loading/auto-loading clears undo
history. Coverage-changing actions exist (reset, occlusion paste/fill, whole-tile
paste/animation copy and undo), but no captured action history identifies one as
the historical trigger. None is attributed to the user here. No native source
patch or application rebuild was warranted by the established evidence.
Main agent reran the persistence regression and 12 semantic-atlas tests: passed.
Recovered disk layers require reloading the profile in an already running app.
