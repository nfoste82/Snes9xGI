# Runtime cleanup validation

The cleanup starts from `973b3a45` and does not change authored profiles, freezes,
source sampling quality, geometry rules, current renderer reference modes, or
performance tolerances. Private evidence is under `build/remaster-cleanup/`.

## Baseline scope

The owner's two known curated-gate failures are:

- `visible_face_work`: frozen complete output remains
  `18198169831120924552`, but invocation allocations are 1,048,544 bytes against
  the unchanged 950,000-byte bound.
- `tile_type_overlay`: the older view-cycle assertion expects HeightOffsets to
  cycle to Composite. The cleanup does not alter the cycle or its assertion.

The opt-in monolithic `legacy_remaster_smoke` fails before its old-format decode:
the manufactured packet's `modernTail` comparison fails at baseline line773
(candidate line774). Both binaries fail the same expression. This is separate
from the two known default full-gate failures; older test-guide line references
describe an earlier version of the same drifting fixture.

Broader test discovery is not equivalent to the curated gate. The untouched
supplemental portable baseline has 53/64 passing; failures include manufactured
old-format packets, older geometry/cache assumptions, and the deliberately
failing `paused_capture_legacy_race`. The legacy offscreen suite additionally
had a stale hardcoded held-lamp X position; its updated test computes the
expectation through the actual lamp contract while retaining its other checks.

Top-level Python tooling baseline: 50 tests pass. Broader ALttP discovery with
the default Python environment runs 368 tests with five failures, five errors,
and nine skips. Errors include missing Pillow and a missing historical profile
backup. Existing authored-diagonal/profile and door-pot expectations also fail.
These are recorded as baseline debt, not silently excluded or repaired by
changing authored data. Full logs retain individual names and traceback details.

One baseline attempt was invalidated by adding a test-reference header during
the run; it is not a stable-source baseline. Its replacement uses an immutable
`git archive 973b3a45` source tree. A missing `build/remaster-tests` parent in the
first attempt caused a layout-evidence setup failure; the unchanged binary passes
after creating that directory. The final baseline/candidate retain that parent.

## Additional checks

- Retired accumulation regression checks both boolean capture bytes with exact
  roundtrip preservation, invalid wire rejection, old profile acceptance, omitted
  obsolete save key, and unchanged scene settings.
- 36 native baseline captures decode and serialize byte-identically using the
  old and cleaned-up implementations.
- Offline declaration ROM tests and the 320-room inventory remain available
  after moving the header out of runtime; both execute successfully.
- Canonical profile validation and parse/save/parse layer counts pass without
  modifying the authored file.
- Release and Debug macOS builds succeed.
- All64 supplemental portable outcomes match exactly (including the same11
  pre-existing failures). All368 ALttP Python outcomes retain the same failure,
  error and skip set. Top-level Python tooling passes53/53, adding three cleanup
  boundary guards to the50-test baseline.
- Quick ASan/UBSan passes16/16; six focused compatibility/offscreen/inspector
  sanitizer tests also pass. The additional triangle Metal predicate regression
  passes under ASan/UBSan.
- All38 remaining standalone portable test translation units compile in syntax
  checking, in addition to the executed curated/supplemental/targeted cases.
- Stable full+Metal+legacy baseline passes121/124; cleanup passes125/128. All121
  baseline passes and all four newly curated checks pass. Both retain exactly
  the same three failures above, with unchanged allocation count/fingerprint.
  All five Metal tests pass on both implementations; neither gate has source
  mutation or incomplete-execution errors.

## Native performance status

The complete18-workload/two-repeat native comparison executed all36processes
and accepted the pinned input/gameplay/source evidence. It **failed two of288
timing checks**: many-lights representatives8/live GPU median97.9908→153.5359ms
and p95134.8131→255.9489ms. All other286checks passed. This is not a performance
pass; the original comparison is retained without rebaselining or changing
tolerances. A bounded matched Release ABBA comparison of the flagged workload
is used to investigate causality, not to overwrite the full-matrix result.

The matched ABBA follow-up passes14/14checks with the same tolerances. Baseline
versus cleanup GPU median is97.5495→94.6390ms; p95153.7642→153.1281ms. Refresh,
field, mesh, setup, source construction, throughput and submission checks also
pass. The flagged slowdown did not reproduce under matched interleaved builds;
this supports run variability rather than an attributable cleanup regression.
The original full matrix nevertheless remains failed, not silently relabeled as
passing. No5ms-budget approval or statistical no-regression guarantee is claimed.

Completed task-owned probe binaries, symbols, isolated benchmark apps, matched
baseline products and matched DerivedData were removed after validation. Static
fixtures, source closure, capture inputs, reports/logs/hashes and the current
default Release/Debug apps remain. Exact cleanup manifests are in the local report.

See [cleanup boundaries](runtime_cleanup.md) and the pinned
[performance stress suite](performance_stress_suite.md). Native stress results,
complete gate summaries, sanitizer results and artifact cleanup are recorded in
the task's final local report once their runs finish. A build or capture-roundtrip
pass alone is not a performance result or complete-room height approval.
