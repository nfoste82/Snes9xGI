# Obstacle Grouping layer / exact occurrence transfer

## Scope

Pixel Inspector UI authoring only. No profile asset bytes, local geometry, room
solver rules, runtime support inference, or room-height approvals were changed by
this UI task. The pre-existing shared-worktree changes remain intact.

## Contract

- `RemasterEditorObstacleGrouping` is appended after Doorframe; old numeric layer
  values remain unchanged. Doorframe stays at its fixed index and is hidden for
  non-door tiles instead of inserting/removing an index before grouping.
- The layer owns the obstacle controls, previously under Metadata. Metadata
  retains ordinary tile semantics. Grouping displays a disabled **Select Tile
  Locations** indicator and hides context controls. The prior BG context mode is
  restored on exit without invoking its destructive selection-change action.
- Scene clicks are tracked independently of the identity selection list in all
  scene layers. Repeated hashes and individually flipped cells survive transfer,
  even if ordinary identity toggling removed that hash from the visible type list.
  Grouping Cmd-clicks toggle exact cells; highlight instance IDs are recovered
  from only those observed cells, including split scanline records.
- No identity lookup, adjacency expansion or footprint completion runs during
  layer transfer. Type-only selections retain their identities but no invented
  scene locations. The status explains the need for explicit occurrence clicks.
  **Show Group Types** also leaves occurrence selection empty, rather than
  highlighting every matching object. Existing completion proposals are separate
  actions requiring confirmation.
- Resume, new frame and ROM loads clear occurrence evidence. NPC transitions
  discard scene highlight IDs. Grouping cannot paint, reset or paste tile layers.
- BG contextual locations still require valid `contextLocations`; OBJ grouping
  does not make invalid sprite context bindings possible.

## Validation

- Debug and Release arm64 app builds passed with code signing disabled. Logs:
  `build/obstacle-grouping-debug.log`, `build/obstacle-grouping-release.log`.
- ASan + UBSan occurrence editor test passed with the real
  `frame_room051_003.s9xrmf`: 1,518 visible OBJ pixels, 16 unique tile types,
  24 independent selected positions, 24 saved members, six default contacts.
  The capture and authored profile are read-only inputs.
- Synthetic BG and OBJ adjacent-object tests passed: picking three repeated
  identities selects only the three clicked cells, never the adjacent object's
  same-identity cells. Toggle, individual flips, missing/stale evidence and empty
  identity-only transfer are covered.
- 15 native source-routing/layout checks passed. Layer flip/shift regression
  passed. `git diff --check` passed.
- Full ASan + UBSan gate passed **20/20**, including all 12 quick cases and
  both small-stack builds, in 128.94 seconds. Artifact:
  `build/obstacle-grouping-validation/20261021T213748Z-kg6hd08c/results.json`.

No interactive AppKit click/Undo/Save session was performed. Source checks and
portable tests are not a substitute for final visual UI acceptance.
