# Concurrent profile saves

Save Profile compares canonical saved records from the loaded baseline, current
session and current disk profile. Tiles merge as complete metadata records;
materials, asset groups, obstacle groups and area lights merge by identity.
Lighting/game settings merge per field; the ordered rules are one record.
Schema version uses the maximum emitted version. Different ROM identities and
invalid combined references reject the save without replacing the file.

Independent changes merge automatically. Different simultaneous edits to the
same record open one dialog with “Overwrite with changes from this session”,
“Keep changes currently on disk”, and Cancel. The policy applies to all
conflicts; unrelated disk changes survive either choice.

Keep Disk saves nonconflicting changes, retains conflicting session records in
memory, and advances the disk baseline. Explicit pending conflict keys ensure
the next Save prompts again. Matching session/disk records resolve naturally.
Additions/deletions use the same rules. Reload/discard clears pending conflicts.
The editor remains dirty; close/load Save confirmation cannot discard them.

Preview uses the merged session, including retained unsaved conflicts. Scene
controls refresh after merging. Undo history clears after an external rebase:
old whole-profile snapshots could resurrect stale disk records. Ordinary saves
retain history. Invalid disk files are never replaced by merging.

The disk is reread after dialog/validation before atomic writing. If changed,
Save stops with edits intact and asks for retry. Atomic rename provides no
cross-process lock; a writer racing the final check/rename remains possible.

Verification: portable profile_merge_test passes independent/conflicting edits,
Keep/Keep/Overwrite retries, deletions/additions, settings/area lights, materials,
groups/rules, invalid references and different-game rejection. No-op merge of
the complete active profile preserves every serialized field byte-for-byte,
including signed/variant geometry and obstacle ordering. Debug/Release builds
pass. Fresh interactive dialog/reload/close-flow checks were not performed.
