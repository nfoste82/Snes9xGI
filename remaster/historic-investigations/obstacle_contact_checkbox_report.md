# Support Contact checkbox: unchecked member would not enable

## Scope / verdict

`East_facing_curtains` authoring: **reviewable but incomplete** (interactive
validation pending). This is an editor click-policy repair, not room-height
generation, support solving, or room approval. No canonical profile/contact flags
were changed, and no unrelated wall repair is included.
Read-only profile SHA-256 at final inspection:
`34a52b76858f64aed90ce8454cbe02b8716f6c1ac665236218e912856738c01d`.

## Actual root cause

The read-only canonical group has eight BG members (4×2), Z=0 throughout,
and only (3,1) marked contact at investigation time. Members 3=(3,0) and
7=(3,1) share `966069b2157ea21f`; the bottom member has V flip, the top does
not. Scene occurrence matching uses coordinates, source and transformed flips;
the saved footprint/dropdown uses stable canonical member indexes. These map
independently under every whole-object H/V mirror. Shared artwork is not the
failure, nor is a top-member contact disallowed by profile validation.

The checkbox has `allowsMixedState=YES`. Native AppKit reproduces its cycle:
**Off (0) → Mixed (-1) → On (1) → Off**. The old action enabled contacts only
when the saved selection was mixed OR the post-click sender state was On.
Clicking a noncontact therefore requested *false*, successfully serialized a
no-op, and refreshed back to unchecked without entering its error alert branch.
This affected every all-off member selection, not only East curtains.

## Repair and safety

- Portable click intent derives solely from saved selected flags: all-off or
  mixed → enable all; all-on → disable all. AppKit's intermediate state is only
  presentation, never mutation intent.
- Contact mutation remains a candidate-copy transaction. Last-contact removal
  has an actionable message: enable another member first. Bounds/empty/stale
  selection and missing/ambiguous group errors are explicit.
- Rejections refresh the checkbox first, then retain the reason in status and
  tooltip and show an alert. Disabled no-group/ambiguous controls have visible
  status and explanatory tooltips; an invalid selection cannot appear as an old
  checked box. Invalid member indexes are cleared before preview indexing.
- Explicit footprint/member selection wins over old scene clicks; its named
  group is re-found after profile reorder rather than editing the inherited
  vector slot. A missing/changed group rejects. No hash fan-out, nearest member,
  guessed support end, or automatic contact reset.
- Existing one-snapshot Undo, metadata sync/live publication and named-group
  Save Profile merge remain the commit path. Serialization failure is visible
  for both the old and candidate profile. No intermediate partial edit commits.

## Verification

Artifacts: `build/obstacle-contact-checkbox/`.

- Portable editor regression passes optimized and ASan/UBSan. New read-only
  canonical-East fixture checks exact member/flip identities, every mirror,
  top-only enable, both right contacts, mixed intent, last-contact atomicity,
  stale/empty indices, competing shared identity groups, and unchanged input.
  Existing tests cover repeated occurrences, independent shared groups, source
  isolation, dirty assets, serialized Undo/redo, roundtrip and disjoint merge.
- Real optional Room051 `_003` capture plus current profile pass: 24 sprite
  occurrences, 1,518 visible pixels, exact selections; no profile writes.
- Portable profile merge regression passes.
- Native `macosx/remaster-obstacle-contact-checkbox-test.mm` passes, demonstrating
  actual NSButton Off→Mixed behavior and production-helper edit/rejection policy.
- Native source layout/routing/message checks pass **20/20**.
- Debug and Release arm64 application builds succeeded (see `debug-build.log`
  and `release-build.log`).
- `git diff --check` passes.

No floor solver, runtime support direction/plane behavior or authored geometry
was modified, so no complete-room validator/approval is claimed. Existing
horizontal support-end handling remains unchanged. The smallest next check is
launching the rebuilt app, selecting East member (3,0) in the footprint/dropdown,
clicking Support Contact once, and confirming both right members are contacts;
then Undo/redo and explicit Save/reload only if the user wants that change.

## Independent final verification

Parent native AppKit regression and inspector layout20/20 pass. Release native
build succeeds; product `build/release/Snes9x.app`, log
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/curtain-contact-release.log`.
Two live-worktree full gates correctly reject concurrent input changes despite
passing individual executables. An isolated final source/profile snapshot then
passes the full portable+Metal gate **34/34** without mutation; results:
`build/obstacle-contact-checkbox-parent-verification/20261023T204310Z-gbsqqkk1/results.json`.
This snapshot pass is not misrepresented as an immutable live-worktree pass.
Interactive user authoring remains unverified; no contact flags were written on
the user's behalf.
