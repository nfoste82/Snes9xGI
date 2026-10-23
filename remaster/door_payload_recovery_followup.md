# Three Door-art identities: expanded recovery search

2026-10-14. Verdict: **blocked by missing evidence** for recovering the user's
Door roles, ramps and painted normals. No profile, runtime or generator changed.
This is not a room/state height approval. Existing worktree changes were retained.

## Correction to the limited occlusion report

`profile_occlusion_recovery_report.md` documents an exact three-line occlusion
copy, not recovery of the user's Door authoring. Explicit arrays do not establish
human authorship. In particular `alttp_atlas_profile.py:116-118` produces exactly
height=64 zeros, normals=64 copies of (128,128,255), occlusion=64 zeros and
emission=256 zeros. All three old target records match this neutral payload.
Consequently even the recovered zero occlusion arrays must not be presented as
proof of painted metadata. The user's restoration request remains unfulfilled.

## Search scope and evidence artifacts

Read-only search of the workspace (including ignored build outputs), Snes9x
Application Support, and the approved temporary artifact directory:

- 1,023 filesystem TOML candidates: 23 workspace, 934 temporary, 66 Application
  Support. This includes both disk backups and 20 build review/candidate profiles.
- 26 semantic/profile/annotation/metadata-named JSON candidates, plus corresponding
  historical Git blobs. The canonical `remaster/tools/ALttP/alttp-tile-semantics-v1.json`
  has no target entries or entries for their exact-source conversion aliases.
- All-ref/reflog object traversal across 2,761 commits, including the old root
  profile paths, deleted backups and height_generation branch; 37 distinct
  TOML/semantic-JSON blobs inspected. 47 unreachable blobs checked too.
- 1,669 `.s9xrmf` binaries decoded using the current native deserializer, zero
  failures. 1,187 target metadata records: c925=419, 9c047=414, de193=354.
  All eight facing/flip geometry slots checked, not just base layers.
- Read-only exact-source alias lookup in `alttp-atlas.sqlite`: c925↔bb95ef35793b1673,
  9c047↔f732ec3e0913b3ff, de193↔8e4036793a6b95de. Alias profiles also contain
  only neutral arrays, never Door roles/masks or painted normals. No artwork
  similarity transfer was used.
- Spotlight filename discovery returned no additional profile/capture roots.
  Native preferences identify this workspace/current profile. Time Machine
  reported no machine backup directory; local snapshots listed only OS-update
  snapshots. This does not exclude disconnected/external backups.

Full exact arrays, file paths, profile SHA256s and filesystem mtimes are in:
`/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/`:

- `door-payload-audit.json` — every distinct target/alias asset dictionary and
  all found source memberships; 1,056 TOML inspections including Git blobs.
- `door-payload-captures.jsonl` — native-decoded target payloads with presence
  flags, roles, masks and all geometry variant layers.
- `door-payload-summary.txt` — capture groups, native timelines, Git timeline,
  semantic alias lookup (generated before adding alias dictionaries to audit).
- `door-payload-audit.py`, `door-payload-capture-probe.cpp`,
  `door-payload-summary.py` — investigation tools, not profile repair tools.

Repeated generated/replay copies are not independent evidence of authorship.
JSON search was scoped to metadata-like filenames, not every diagnostic JSON.
No running-process memory, browser unsent form storage, disconnected backup,
other user's files or deleted filesystem blocks were inspected.

## All target variants

Every found variant has **no tile_role, no doorframe_mask, no authored semantic
fields**, all-zero emission and flat +Z normals (128,128,255) at all 64 pixels.
No nonempty facing/flip geometry payload was found in captures or profile target
records. `hasNormals=true` in a capture means an array exists, not that it was
painted by the user.

For **each** target c9254894bb09de73, 9c0473118412613f, de1930ab64c9b0de:

1. Height all zero, zero occlusion, no height_sampling key (older build output).
2. Same arrays, `height_sampling="nearest"` (old backup, older captures/Git).
3. Same height/normals/emission, occlusion absent (backup2, later captures/Git).

**c925 only**, two additional variants:

4. Height `7-x` on every row, flat +Z normals, occlusion absent: exact temporary
   pre-restoration backup from the previous session.
5. Same `7-x` ramp, zero occlusion present: current disk and native capture081.

Thus profile dictionaries have 5/3/3 variants respectively; capture payloads
have 3/2/2 variants (sampling omission is not a distinct effective capture layer).
The c925 ramp is preserved evidence, but its author/event is not established by
this search and it does not recover the missing normals or Door role.

## Timeline (filesystem dates are not action-log proof)

- Native `frame_OLD_3` (Sep 25 10:02 -06): all three flat/neutral, zero occlusion.
- First Git appearance found: b14c6818 (Oct 3 13:07 -06), root profile; all three
  flat/neutral, zero occlusion. Later path reorganization and ce1ffe5d,
  1369803f, 7f98606a, ac8ba407, b56e5b65 retain that state.
- Native capture061_057 (Oct 10 22:28 -06): still flat/neutral, zero occlusion.
- Earlier binary evidence than the previous sidecar-only boundary:
  **capture061_064, Oct 11 10:43:24 -06**, already lacks all three occlusion
  arrays. Capture061_066 at 10:45:21 confirms the same via its profile sidecar.
- c6667eac (Oct 13 17:19 -06) records occlusion removal; af2ab83d retains it.
  Neither establishes which operation caused the earlier loss.
- capture060_079 (Oct 14 13:49 -06): c925/9c047 flat, +Z, unknown role, no mask
  or occlusion; de193 not in viewport metadata.
- **capture060_080 (15:05 -06)**: all three flat/+Z, unknown role, no masks or
  occlusion. No `.profile.toml` sibling; binary was inspected directly.
- **capture060_081 (15:50 -06)**: all three zero occlusion; c925 has the `7-x`
  ramp, other two remain zero; all still unknown role/no masks/flat +Z.
  No `.profile.toml` sibling. None recovers painted normals.

## Persistence/generator findings, not historical attribution

The neutral generator is a proven source of this *kind* of placeholder. Current
generation inserts absent layers; it preserves existing normal arrays, protects
Door geometry and rejects output equal to its source. Floor/semantic overrides
can change contextual proposals, but no found target variant demonstrates a
Door/ramp/painted-normal input being replaced by this generator. No patch is
justified as the three-hash overwrite repair without that input/event evidence.

Native editing and undo snapshots are memory-resident; this source has no
separate durable autosave journal of pending authoring. Dungeon evidence pins
the active profile to `.profile.toml` when such evidence is available
(`remaster.h:556-567`), not a hidden independent authoring database. Captures
without those sidecars still carry viewport metadata and were decoded above.
The existing semantic mask-omission fix is not evidence of lost roles/normals.
Current profile merge changes were already in the worktree and are not claimed
as a fix produced by this investigation. The running release app's memory was
not inspected or reloaded; current source is not proof of its exact behavior.

## Smallest next action

If any still-open inspector/undo state shows the painted Door data, preserve
that session **before reload, quit, undo traversal or Save Profile**. Export a
separate capture with all three identities visible and/or a non-destructive
profile copy. Otherwise locate a saved TOML or capture made after the painting
and before replacement, or an external/editor version-history backup from that
interval. A fresh capture of the already-neutral state cannot recover lost
values. A screenshot may establish intended geometry but is not byte-exact
normal/role recovery. No normals, roles or additional ramps were fabricated.

Native decoder build and all 1,669 decodes passed; `git diff --check` passed.
No room geometry changed, so no room validator/build/gameplay approval claimed.
