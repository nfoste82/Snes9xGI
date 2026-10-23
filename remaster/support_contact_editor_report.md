# Bulk Support Contact editor repair

The standard fixture-free `obstacle_group_editor_test.cpp` also exercises the
contact-completion proposal across H/V transforms, including selected omitted
repeats, preservation of unselected contacts/Z, and nonmutation before commit.
This keeps the failure class in the regular sanitizer gate; the separate
capture-dependent test reproduces the exact six-cell Room051 selection.

## Scope

Authoring-only contact flags; no geometry solving, support inference, profile
migration, or complete-room approval. Existing worktree changes were preserved.
The user profile was read only, never saved or rewritten. Its SHA-256 before and
after validation is
`5ef31411c5473070ff158af2abdb9239f4af116c816d34714e5a0f0837227c41`.

## Behavior

- Support Contact has its own immediate action, with one transactional Undo
  snapshot and the existing dirty/preview/save-merge path.
- Scene cells resolve to authored member indices by position, identity, source,
  and individual flips under translated or whole-object mirrored matches.
  Sparse captured evidence is allowed only when the best mapping is unique.
  Repeated artwork is never edited by global hash membership.
- Cmd-click footprint buttons toggles authored members. The dropdown explicitly
  selects one member. Scene clicks switch back to occurrence selection.
- Mixed contacts display mixed; their first click enables every selected member.
  Contact-only edits preserve all Z values, unselected members and asset layers.
- Ambiguous/stale/nonmatching/cross-group scene selections explicitly fail as a
  whole. Cross-group bulk editing is not implemented; no subset is edited.
- Removing every contact is still invalid: an alert explains the failure and
  refresh restores the checkbox from unchanged data.
- Apply Member / Group Name changes only the focused member's Z and group name;
  it no longer consumes or collapses the bulk contact checkbox.

## Validation (2026-10-21)

All commands ran from the repository root; artifacts are ignored under `build/`.

- `build/obstacle-group-editor-contact-test`: PASS. Portable synthetic cases
  cover BG/OBJ, all mirrors, individual H/V flips, signed/distinct Z, sparse
  footprints, mixed contact updates, unselected preservation, last-contact and
  invalid-index atomic rejection, repeated-art ambiguity, source rejection,
  serialized Undo/redo snapshots, unrelated dirty assets, roundtrip and merge.
  Optional real Room051 capture plus current profile also passed read-only
  occurrence resolution, including mirrored copies of that captured evidence.
  Log: `build/support-contact-portable.log`.
- `python3 macosx/remaster-inspector-layout-test.py`: 9/9 PASS, including native
  action routing, mixed-first-click, explicit dropdown/Cmd selection, focused-Z
  scope and existing Undo/dirty/live commit routing.
  Log: `build/support-contact-native-routing.log`.
- `build/support-contact-profile-merge-test`: PASS.
  Log: `build/support-contact-profile-merge.log`.
- Full arm64 Debug and Release `xcodebuild` with signing disabled: BUILD
  SUCCEEDED. Logs: `build/support-contact-debug.log` and
  `build/support-contact-release.log`. Known deprecated API/deployment warnings
  remain; no build errors.
- `git diff --check`: PASS.

The capture command was:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic \
  remaster/tests/obstacle_group_editor_test.cpp \
  -o build/obstacle-group-editor-contact-test
build/obstacle-group-editor-contact-test \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_003.s9xrmf" \
  remaster/profiles/ALttP/alttp-profile.toml
```

## Remaining evidence limits

No interactive AppKit click/save/Undo smoke test was performed, and no user
profile was saved to test UI persistence. Undo registration is checked at source
level and its serializer/restore/merge data contract is tested portably. Missing
or ambiguous captured motifs require explicit footprint selection. No runtime
floor correctness or complete-room height coverage claim is made.

## Follow-up: missing bottom-row occurrences

The unchanged current profile has a 16-member `ThroneRoomDoorBlocker` group.
Its saved bottom row includes x=0,1,2,4 but not x=3,5. A real six-cell bottom-row
selection in both Room051 captures `003` and `010` reproduced exactly:
`Selected occurrences do not match one authored obstacle footprint.` The first
repair resolved only existing authored members and therefore stopped at this
old incomplete footprint; this was not a tile-hash selection failure.

The contact action now prepares a contact-preserving completion proposal if
ordinary occurrence resolution fails. Only a uniquely completed captured sprite
motif whose members resolve every selected occurrence qualifies. Existing
contacts remain unchanged and added occurrences start noncontact, before the
selected contact intent is applied transactionally. The checkbox shows the
provisional mixed state and labels `add missing repeats…`. Confirmation explicitly
offers **Add Missing Occurrences and Set Contacts**; cancellation and validation
failure leave the session unchanged. A single existing profile commit provides
one Undo record for both additions and contact flags; Save Profile is separate.
The separate legacy Complete Footprint action retains its explicitly advertised
south-boundary reset, but is not invoked by this contact action.

Additional files: `remaster/tests/obstacle_contact_completion_test.cpp`, plus updates to
the shared helper, native action/refresh, native routing tests and authoring docs.

Follow-up checks, all passing:

- New read-only completion test against the current profile and both real
  `frame_room051_003.s9xrmf` / `frame_room051_010.s9xrmf` captures: exact original
  error reproduced, then 16→24 proposal, six selected bottom cells, all four
  whole-object mirrors, individual flips, custom unselected contacts and signed
  Z preservation, new unselected members noncontact, mixed→all-on→all-off,
  original interior contact retained, last-contact failure, foreign source/hash
  rejection, duplicate-motif ambiguity, and serialized roundtrip.
  Log: `build/support-contact-completion.log`.
- Existing editor/capture/profile tests rerun: PASS
  (`build/support-contact-portable.log`).
- Native routing/layout checks: 10/10 PASS, including confirmation before the
  single commit and cancellation before mutation.
  Log: `build/support-contact-completion-native.log`.
- Full Debug and Release: BUILD SUCCEEDED.
  Logs: `build/support-contact-completion-debug.log` and
  `build/support-contact-completion-release.log`.
- `git diff --check`: PASS; user profile SHA-256 still exactly the value above.

This fixes authoring/persistence workflow only. In particular, the user's
existing interior contact at (3,2) is intentionally preserved; any runtime support
conflict it causes is not repaired or silently removed. No GUI interaction, real
profile save, floor inference or room-height approval was performed.
