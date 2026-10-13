# Outgoing room geometry lifetime — reviewable but incomplete

## Release integration / warning cleanup

Arm64 Release integration build succeeded. The four retained-word index
expressions whose `cell` is size_t now keep `at` as size_t, with decoded `word`
declared separately as int, removing the reported64-to32 narrowing without
changing placement selection. Optimized narrow departure regression and
`git diff --check` pass after cleanup. Final Release rebuild after this warning
cleanup also succeeded; updated product is `build/release/Snes9x.app`.

## Cause and repair

EndFrame previously retained previous maps only when the observed room ID changed.
At the EARLIEST module07/submodule02 departure the room ID can still name the
source while collision/construction state and actor plane have begun changing.
Inputs are then unpublishable, so current `reviewMap` was absent and the renderer
fell into baked default geometry. Outgoing placement bases and canonical local
overrides dropped before the snapshot intended to retain them was even taken.

New render-only outgoing lifetime is independent of `transitionSupport`, which
is the grounded doorway/entry anchor and cannot identify every visible wall.
At first handler02 departure, snapshot BOTH already-published BG maps plus all
their solved arrays/diagnostics/door regions/fallbacks/local previews and complete
last-settled final words. ROM/profile/session identity pins the snapshot. While
room ID is still source, render that immutable solve without testing it against
partially overwritten destination WRAM. Exact captured word(tile/palette),
hash/flips still qualify the displayed PPU producer. During mixed scroll the
existing actual origin/room ownership mapping selects frozen source or independently
solved destination. No actor-plane/destination solve may rebase source surfaces.

Retirement checks all viewport corners against the attested destination origin;
the source is safe to unload after the entire rectangular scroll viewport belongs
to destination. A tombstone prevents resurrection while handler02 lingers. Older
inputs without attested origins expire at settle. Maximum240-frame transition
bound, ROM/session/profile changes, null/outdoor/non-dungeon state, unsupported
handler and unrelated room jumps expire snapshots. Reset/defrost continuity hook
also clears them; profile/scene/compiled-map publications invalidate retained
geometry. Unknown/changed producers retain explicit baked fallback, never borrow
source or destination floors. No new height datum is solved in this slice.

## Verification

Production EndFrame regression starts settled, injects an EXISTING solved map
fixture (BG2 base40/BG1 base24 with canonical0..7 local override), then exercises
earliest departure unchanged room ID, actor plane change and collision overwrite,
all construction phases1..7, mixed scroll, completed destination solve, full
offscreen source/unload and no revival, settle, reset/defrost, profile replacement,
ROM change and unrelated room jump. Hash mismatch and same-hash DIFFERENT placed
tile word both reject base AND effective local geometry. Both BGs retain exact
effective sources through departure/mixed visibility. The test deliberately
distinguishes retained-result lifecycle proof from an independently authored
floor-anchor fixture. No room-specific runtime offset rule was added.

Optimized and ASan/UBSan departure test passes. Existing transition cache,
entry lifecycle, four-direction scroll ownership, complete geometry/cache and
offscreen source-continuity tests pass; diff-check passes. Transition cache test
now models re-entry into dungeon after an invalidating module14 frame before
asserting previous-map retention; stale unsupported-module snapshots no longer
count as successful continuity.

Evidence corpus:61 captured own dungeon sidecars,59 with complete templates/
maps/profile, ZERO submodule02 frames. Latest room050015 is settled, not a
transition trace. Thus there is no new captured gameplay proof of departure
timing/teleports, and fresh mixed-scroll capture is explicitly required. Faithful
handler/order production regression is the current evidence, not a substituted
attractive screenshot. See `alttp_transition_evidence_inventory.py` output in
preapproved temp `departure-evidence-inventory.json`.

Mandatory complete-room015 validation v2 was run with own pinned profile,
matching ALTPPD1/ALTPAT1 and compiled map:376/376 supports,4/4 fallbacks,
missing placements0/component conflicts0/map missing0/map mismatches0,
walls439 solved/485 unknown. Exit1: the existing incomplete room is NOT approved.
No solver/model change or room-height success claim; outgoing preservation cannot
repair preexisting unknown walls. Artifacts `departure015-gate/` under the
preapproved temp directory contain full inputs/planes/state/graph/profile/runtime/
map/JSON/SVG. Release integration passed as recorded above. No changes to profile,
frontend, deterministic Direct, signed source continuity or hidden clustering.

## Files / next action

`remaster.h`: immutable outgoing latch, validity/invalidation/unload, earliest
source render-map selection; `frame.h`: retained final-word provenance for base,
fallback, mixed floor and local preview consumers. New
`dungeon_departure_geometry_test.cpp` and this report; narrow existing transition
regression update and evidence inventory tool.

After integration, capture settled source, earliest departure and mixed scroll
with both complete input sidecars; compare tile effective geometry across the
sequence. Remaining limitations: current live animation hashes that differ from
the frozen producer are conservatively rejected, not reclassified into guessed
equivalence; independent actor support plane transitions and offscreen emitter
lifetime remain distinct scopes. General outgoing animation-state transport
needs provenance-qualified geometry-equivalent identities if live captures show
animation-only rejection; no unsupported identity revival is introduced here.
