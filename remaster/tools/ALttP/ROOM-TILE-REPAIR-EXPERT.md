# Room agents: discovery versus repair

Project definitions live in `.opencode/agents/`. The auditor uses
`litellm/gpt-6.1-terra#medium`, the repair expert uses
`litellm/gpt-6.1-terra#high`, and their shared coding specialist uses
`litellm/gpt-6.1-sol#medium`.

| Agent | Input | Responsibility | Output |
|---|---|---|---|
| `room-height-auditor` | One or more rooms, optional states/captures | Run deterministic tools to discover issues, manage evidence, improve detection gaps | Issue groups, exact evidence, coverage and repair-ready handoff; no production repairs |
| `room-tile-repair-expert` | Specific reported tile issues, audit findings or reproduction | Diagnose and, when requested, fix shared causes that do not require human authoring | Regression-backed repairs, before/after evidence, remaining blockers and human-authoring requests |
| `room-code-engineer` | Scoped coding request, role/authorization, pinned evidence and acceptance criteria | Implement tool/test changes or authorized engine repairs; no further delegation | Patch, baseline-fail/after-pass regressions, completed gates and explicit gaps |

The top-level orchestrator assigns audit/repair scope and schedules independent
re-audits. The auditor and repair expert run tools and interpret evidence, but
delegate **all code changes** to the engineer. They may write reports and run
artifacts; executable scripts are code even inside build/. Shell writes must not
bypass this division. The auditor may commission tooling-only work, never
production repairs. The repair expert must convey the user's repair authorization.
The engineer preserves authored profiles and cannot delegate further.

Coding handoffs include issue IDs, requesting role and authorization, immutable
input/source pins, reproduction commands, expected/actual samples, allowed files,
negative tests, acceptance gates and known concurrent work. Each parent runs at
most one coding child at a time and verifies its output. The orchestrator must
serialize coding children with overlapping files; parents do not concurrently
edit child-owned files. If delegation is unavailable, return the handoff to the
orchestrator rather than switching models or implementing directly. Existing
running sessions may retain their old instructions; start fresh role sessions
for this workflow.

**Current harness limitation:** nested calls have failed with `Subagent depth
limit reached (1)`. Permissions alone do not override that execution limit.
Until nested execution is confirmed available, auditor/repair expert return
coding handoffs to the top-level orchestrator. It launches the same engineer as
a direct child, then routes the result back for role review and independent
verification. Do not repeatedly attempt blocked nested calls or code directly
as a workaround. The model/role split remains unchanged.

`room-tile-repair-expert` replaces `room-height-expert`; the old agent ID is no
longer a separate definition. The auditor's name remains height-focused; the
repair expert's remit deliberately covers **room-tile issues beyond heights**.
Neither agent substitutes prolonged manual room inspection for automated checks.

See the [Room Height Auditor runbook](ROOM-HEIGHT-AUDITOR.md) for discovery tools
and evidence protocols, or [Geometry and Placement](../../docs/geometry/00-index.md)
for runtime feature guides and coverage standards.

## Routing examples

* **Discover:** “Use room-height-auditor to inspect rooms 0x001 and 0x061.”
* **Diagnose:** “Use room-tile-repair-expert to explain these BG1/BG2 seam
  failures; do not change runtime code yet.”
* **Repair:** “Use room-tile-repair-expert to fix the scene-admission issue in
  this report, preserving authored arrays and checking the affected corpus.”
* **Human authoring:** “This asset has no authored normal map and no proven
  derivation.” Report the exact asset/context and required content, rather than
  inventing a normal map or silently painting a placeholder.

An inspection request does not authorize runtime changes. A repair request
authorizes work on its specified issues, not every unrelated issue encountered.
When intent is ambiguous, diagnose and establish the repair scope first.

## Repair scope and human-authoring boundary

Repairable classes include metadata parsing/activation/selection, stale source
transport, support/constraint solving, provenance and assembly recognition,
transformed masks, normals selection, BG/plane composition, physical scene
admission, occlusion, capture/replay parity and inspector/debug projection.
Trace the entire path from intended existing metadata through the final consumer;
do not assume a debug symptom means the asset is missing data.

Preserve human-authored heights, normals, masks and artwork. Deterministic
schema migrations, source activation and mechanically proven transformations
may be repaired within scope without inventing content. If the fix requires
new artistic geometry, ambiguous semantic intent or changing a user's chosen
shape, separate that work and ask for human authoring/approval. Report existing
technical fixes that can proceed independently. Do not convert absent content
into zero arrays, flatten relief, or add room/hash/coordinate patches.

## Evidence-first repair protocol

1. Read `AIDocs/00-index.md`, relevant runtime/workflow notes, and the auditor
   runbook `ROOM-HEIGHT-AUDITOR.md` for available diagnostics. For heights, follow
   `AIDocs/alttp/07-generalized-height-coverage.md` and its complete-room standard.
   Load non-height metadata/rendering documentation only as needed.
2. Pin ROM, room/state, profile bytes, runtime revision and capture inputs.
   Record dirty work and preserve unrelated edits. Treat an auditor report as a
   starting point, not unquestionable ground truth or authorization.
3. Reproduce the specific defect using scripts and actual selected sources.
   Distinguish an incorrect runtime result from a stale input, incomplete
   detector, intentional transparency or misleading debug visualization.
4. Identify the general failing stage and affected variants. Add a failing
   automated regression before repair, including transforms, overlaps,
   partial overwrites and ambiguous negative cases where applicable.
5. Repair shared runtime/metadata handling at that stage within requested scope.
   Preserve intrinsic authored content. If an audit tool is wrong, repair its
   contract independently; never weaken an independent check to conceal a
   runtime defect. New tools should reduce future manual analysis.
6. Rerun the affected detectors and available room/state corpus. A targeted
   regression is required, but one room's pass is not game-wide coverage.
   Report before/after counts and every remaining unknown or conflict.
7. For height changes, run complete-room validation v2 with matching ALTPPD1,
   ALTPAT1 planes, state, profile, runtime solution and map when available.
   Require floor bases, effective sources, ramps/directions and denominators;
   a viewport or hash-level height key is not approval.
8. Run relevant non-height source/composition tests, curated full remaster gate,
   `git diff --check`, and required macOS build for runtime changes. Use Metal
   tests when the changed GPU path needs them. Compare matching gameplay captures
   where needed; clearly identify any fresh post-repair capture still required.

## Handoff and report contract

The auditor hands off: issue IDs/classes, scope and pins, reproduction commands,
detector reports, exact BG/plane/buffer coordinates, actual versus expected
samples, assumptions/confidence, and unsupported evidence. It must include all
failure groups, not only newly discovered ones.

The repair expert returns: symptom → root cause → shared fix → regression →
available corpus and before/after results → actual runtime versus debug outcome
→ residual blockers/human-authoring needs. Distinguish “targeted issue fixed”
from “room approved.” A targeted repair may be complete while unrelated room
defects remain; never call the room successful until all applicable gates pass.

Repair verification runs the tools directly after the engineer returns, rather
than recursively launching the auditor. A caller subsequently requests a fresh
independent auditor run. Only auditor/repair-expert → code-engineer delegation
is allowed; the engineer is a leaf. This preserves discovery/repair separation
without creating delegation loops.
