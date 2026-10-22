# Room051 own039: authored stair datum and side-contact pillars

## Result (partial repair; wall coverage NOT complete)

Own `frame_room051_039.s9xrmf` confirms configured fallback50, not a measured
50-unit rise. Intact BG1 template0x10c8 at(30,39) has uniform, agreeing outer
rails23→0 (rise24), explicit stair ends and two independently reproduced zero
landing columns at both ends. BG2 retains the first stair row but clears the
other three rows. Existing BG1 rail logic measures24 correctly, but its signed
relation export required the ENTIRE connected wall graph to be conflict-free.
An unrelated southern BG1 wall cycle invalidated419nodes, suppressed the export,
and caused BG2 to fall back to50.

`frame.h` preserves the independently verified rail/landing relation separately
from graph-wide path exports. Existing consistent authored wall paths retain
precedence; partial/missing/nonuniform stair arrays still fail. BG2 now consumes
one signed24 relation, configured fallbacks1→0, lower floor0/upper floor24.
No configured-rise/profile-array changes or room/hash height override.

The two Floor_pillar assemblies at(25,34)/(37,34), eight members each, ended
against rail trim rather than independently occupied floor. Their explicitly
declared feet have an entire authored zero-height side edge touching ordinary
zero-local floor. Terminal obstacle contact now accepts that exact transformed
edge when no direct occupied foot/outward support exists. It still requires
one qualified plane/component/base, does not borrow another obstacle's offset,
does not accept arbitrary proximity/nonzero edges, and preserves existing
direct contacts. Independent Python obstacle audit reconstructs the same rule.
Both pillars255→24; all16members and464actual production pixel usages checked.

Production input check matches92,826captured identity/flip/camera samples.
11,953upper-floor pixel usages verify actual offset24 without placement-height
replacement. Of8,192complete placements, BG1 bases unchanged;1,892BG2 bases
change (upper datum50→24 plus16previously missing pillar members).

## Remaining requested walls: reproduced, diagnosed, NOT repaired

BG1 lower-area wall graph remains0/288solved,419bad graph nodes. First conflicts
at plane1,(13,53..57)/(50,53..57): authored east/west edge section
`a19ae88181451d61` spans0..5, while adjacent face bands and the perpendicular
corner seams require8-unit continuation. At(13,53), the support contact asks
base19 (24−5), whereas the contiguous inner wall bands ask16. The corner/end
arrays `ccf87d6be13c02a4` and flipped straight end participate in the same
3-unit discrepancy. The complete pinned arrays are not rewritten or rescaled.
Fixing24 export isolates trustworthy stair evidence; it does NOT validate this
wall graph. Required capability: explicit per-face/per-contact structural
region constraints for clipped/capped authored relief, or reviewed correction
of inconsistent canonical arrays. A global +24/+50 or guessed flattening would
hide contradictory geometry rather than solve it.

West/east BG2 wall-decoration strips at x16..19 and44..47,y14..31 have64
ambiguous authored Wall_Pillar members. The 4x2 group matches V-symmetrically
but its mirrored variants specify different outward contact direction. Existing
deduplication correctly retains that semantic distinction; dropping it without
proving equal support would silently choose an owner. Another54BG2 wall
placements remain unresolved, including48upper decoration members at the
repeated strip ends. Required capability: contact-aware equivalence of complete
symmetric footprints and ROM-backed wall-column/support-region ownership.
No blanket wall/group override was applied.

## Validation and corpus

Mandatory complete-room v2 artifacts in `build/room051039-{before,after}/`:
lossless matching ALTPPD1/ALTPAT1, pinned captured profile/state, full manifest
and hashes in `gate.json`. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`;
profile serialized FNV1a64 `3b98a54cc03c23ea`, SHA256
`370b3d0d58edf65b61a506b5560d49b2642b9dd24e2928e78e82618a9339abb3`.

Gate remains exit1: reachable denominator[0,360],360/360valid,0missing;
fallback4/4,0conflicts; BG2walls564solved/54unknown,4structural-conflict nodes;
authored obstacles80/144solved,0missing/64ambiguous-overlap,28/28contacts
witnessed; doorframes8/12; compiled-map4missing/54mismatches. The own039
before gate uses the updated independent side-contact audit: it already proves
the16pillar bases at50, exposing12missing visible-map mismatches. Those disappear
in the after gate. These historical before counts differ from own035 because
the actual pinned Link seed/plane is different. No stair playback invented.
Both-BG/both-plane floor inventory retains full local/effective bases, flips,
fallback directions and missing denominators, not hash-level height presence.

Corpus346entrance/default contexts/319room IDs, both BGs/planes with unchanged
profile: complete cache/full-map/diagnostic/packet equivalence passes. Relative
to previous floor-continuity baseline, solved authored obstacle members1672→1696:
room04b12→20 and room05164→80; no solved-member losses. Structural-conflict
sum17,983, unknown-wall sum262,701 and authored-floor missing69,940 unchanged.
This remains synthetic animation cache stress, not all-state gameplay approval.
Artifact `build/room051039-corpus-final.json`.

Tests: north/south rise24/32 intact stairs with an unrelated conflicting wall
cycle; cross-BG transport; partial/missing/nonuniform rejection. Obstacle tests
cover zero-side contacts/four flips and reject nonzero edges. Optimized and
ASan/UBSan tests pass; full portable suite25/25 (includes concurrent existing
tests), independent obstacle Python13/13 and validator Python26/26 pass.
Debug/Release arm64 builds and `git diff --check` pass. Final gate manifest:
`build/room051039-tests-final/20261022T200234Z-jehcog3y/results.json`.

Reproduce production:
```sh
c++ -std=c++17 -O2 remaster/dungeon_room051_height_probe.cpp -o build/room051039-height-probe
build/room051039-height-probe "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_039.s9xrmf" build/room051039-after/production
```

Probe also saves original-art PPM for inspection; no user capture/profile was
overwritten. Fresh interactive visual comparison not performed. Relaunch updated
Debug/Release app for live check. Full requested wall repair remains open.
