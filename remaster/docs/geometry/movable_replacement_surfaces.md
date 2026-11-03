# Opt-in movable background replacement surfaces (partial implementation)

Profile schema40 adds `movable = true` to an obstacle group. Absent/false retains
fixed-group behavior and does not migrate existing groups. The native Obstacle
Grouping checkbox edits the saved group through the existing serialized-profile
Undo/publication/merge path. No artwork, height, normal or mask array is authored
by this feature. Canonical profiles are not changed.

## Implemented initial family

Verified USA ROM SinglePot declarations, state1111, sixteen bounded replacement
slots, encoded destination, exact unique authored four-member group, complete
final light/dark template words, matching collision index, and a bounded terminated
ROM secret list are required. ROM `$81:DB69` contains bank81 addresses (not offsets
relative to the pointer table). WRAM `$0500` is state, `$0520` object offset, `$0540`
encoded destination, `$0560/0580/05a0/05c0` UL/LL/UR/LR saved words. The actual
replacement attribute is the LR word's low10-bit LUT value from `$FE00`, copied to
all four cells. Encoded bit13 selects BG1/map4000/plane1 versus BG2/map2000/plane0;
it is not an arbitrary independently selectable collision plane.

Replacement art is decoded in its actual location/graphics/flips by the ordinary
classifier. The initial supported underlay requires explicit authored Floor role,
valid height layer with all64 authored zeros, no active alternative geometry or
context-dependent semantics, and LUT00. Missing normals remain missing. Private
structural solving uses those placements and attributes; the public map retains
current obstacle identity/flips and visible relief/normal consumers. Real RAM,
gameplay collision and stock color rendering are untouched.

Switch88, hole>=80 secrets, moved blocks, segmented/repeated pot families, nonzero
or alternate/context geometry and missing semantics/evidence reject. These are
**unsupported**, not ordinary floor. Opted-in unproven groups lose fixed-obstacle
support claims instead of falling back to neighboring floor guesses.

## Evidence and compatibility

ALTPDR1 is a bounded little-endian owned snapshot (1276+3×secret-count bytes,
max256 secrets). DRP1 embeds it in new frame packets; old packets remain readable
and replacement-unknown. Additive `.replacement-input.bin` and recording DRE1
retain the same values. A sidecar cannot promote a packet lacking embedded proof;
`S9xRemasterReplacementSidecarMatches` rejects orphan/mismatched snapshots.
Completed-frame and first-animation-frame evidence copy values, never borrow RAM.
Replay presents saved geometry without borrowing live slots or inferring old proof.

## Remaining acceptance gaps

This is not completion of the full movable-surface request. A pickup currently
invalidates and performs complete classification/solves (ordinary synthetic event:
two classifications/two solves). Stable-underlay structural cache reuse, including
door-recognition avoidance and fresh visual-ownership rebinding, is **not wired**.
Existing capacity-three stair classification-only caching remains unchanged.
Replacement-bearing inputs conservatively compare all VRAM and disable solved
artwork-equivalence reuse; this is correct but not a performance optimization.

No fresh live GUI capture or complete-room v2 approval is claimed. Historical
room02f025/026 have no replacement evidence and cannot become direct slot truth.
The deterministic reference room02f run is labeled separately. Full nonzero
underlay/context selection, switch/hole/blocks and independent source-to-packet
state/profile pinning need additional implementation and tests before broader use.
