# Room050 own007 — reviewable but incomplete

## Pins and implemented runtime repair

Capture: `~/Library/Application Support/Snes9x/Remaster/frame_room050_007.s9xrmf`
with its immutable complete inputs/templates/profile. ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`, profile serialized
FNV1a64 `8ce20b3a13ee3191`; room80, module7/submodule0/construction0,
collision0, active plane1, Link world[371,2691], scroll[256,2583], both graphics
bases16384, rise50. Production replay matched89,434 BG pixels across buffers.

This is actual placement/runtime source repair, not a count-only change. There
is no numerical four-tile ceiling: unobserved-plane structural nodes required
`wallTemplateOffset`, so fixed three-face/four-band recognizers silently excluded
taller authored stacks and surviving overlays. `frame.h` now admits authored
classified solid walls to that plane's structural graph. Movement/support scope
is unchanged; nonblocking underpasses cannot supply structural datums. Full signed
seams, graph anchors, contradictions and representation checks still apply.

`remaster.h` recognizes complete transformed north/side wall-torch handler
footprints, requiring authored Wall seed and local metadata on all8 members.
Uniform palette shift/priority changes preserve ownership; partial overwrites
and incompatible roles reject. Wall-torch members are terminal, not graph
bridges. `dungeon_wall_mounts.h` requires both immediate tangent wall ends to
independently solve the SAME displayed cross section on one compatible structural
plane. Every member consumes that backing cross section, transformed back into
canonical tilePixel indexing. This implements the user's wall-mounted-wall
contract without rewriting intrinsic arrays, flattening the backing ramp, using
actor-plane selection, inventing0 bases, or nearest-wall filling.

The full four-word surviving outer east door band can inherit ordinary wall
classification when authored as a complete0..7 wall ramp and bounded by two
authored wall ends. Mixed/opening Door regions retain existing dispatch/mask
semantics. This narrowly repairs the observed failure class; arbitrary door
parts/orientations remain outside this added rule.

`offscreen_emitters.h` consumes the same existing placement-local source for
offscreen sphere height as visible/HUD source geometry, instead of using only
the intrinsic asset. No emission RGB/alpha/depth/direction/combination data changes.
Production replay verifies all83 captured emission metadata records exactly and
retains serialized profile bytes. The user-authored source profile was not edited.

## Effective result and complete-room gate

8/8 emitting placements now have effective bases (before0/8). Both complete
torch assemblies,16 cells, receive backing geometry. North bands retain authored
32..39 and24..31 plus base12; side bands receive canonical flipped backing
24..31 and32..39 plus base12, rather than an unrelated authored Y ramp. The
backing wall's solved base is inherited with its local cross section, not copied
from actor support. Structural ownership is north plane0 / side plane1.

The actual007 viewport has all210 emitting pixels onscreen, so it correctly
publishes0 spheres (visible path only). Controlled pure adapter camera changes,
same complete inputs and no historical observation: scrollY128 produces2
offscreen spheres,256/384 produce4, unknown bases0. These are source-recovery
proof, NOT fresh gameplay/camera screenshots or a GI occlusion comparison.

Mandatory validation v2 ran on lossless ALTPPD1 wrappers with both completed
ALTPAT1 planes, exact own profile, pinned local seed[371,131], matching ROM
templates and compiled ALTPHM3. Observed-seed graph denominator[0,376], supports
376/376, fallbacks4/4, missing placements0, component conflicts0, map missing0,
map mismatches0. Broader runtime support remains[0,3564], missing[0,0]. Walls:
419/495 solved/unknown before ->439/485 after. Directional seam conflicts0;
missing directional contacts3866 ->3812 (3258 missing bases,554 unresolved
ownership after), wall-plane denominator2648 across both BGs/planes. Both gates
exit1: room still incomplete and NOT approved for capture/production replacement.

## Generalization audits / regressions

Available own-sidecar source-parity corpus56 states in rooms041/050/060/061/062:
47 audited,7 rejected map/packet identity mismatch,2 unsupported missing templates.
902 emitting occurrences retained; unknown emitting bases50 ->42. Complete-room
unknown walls3412 ->3394; structural conflicts616 unchanged; missing support4065
unchanged across those accepted historical own-profile states. Only the three
050 states changed: older profiles gain4 door/wall placements but retain their
2/4 unresolved emitters because intrinsic wall-mount inputs remain incomplete;
007 gains all8 source bases. These totals are state occurrences, not unique
assets or complete320-room coverage.

Reference fixtures055/060/061 were also solved before/after with immutable007
profile (research metadata on reference inputs, NOT gameplay profile substitution):
055 gate support439/439, conflicts0, walls1620/12 ->1658/28 (solved/unknown).
060 gate support711/711, component conflicts1 unchanged, walls1017/9 ->1021/9;
runtime structural conflicts3 unchanged.061 runtime supports1570/1570, conflicts0,
walls1010/2 ->1014/2; validator exits2 BOTH before/after for preexisting independent
diagonal-proof rejection at BG2(2,38).055/060 gates exit1. The added unknown wall
denominator in055 comes from activating full authored mount footprints, not
filling unsupported backing contacts. No reference fixture is approved.

Portable C++ tests passed: tall_wall_mount (16-band stacks/all flips/underpass
rejection/backing ramp canonical transforms/incompatible owners/partial writes),
mount_classification (both handler layouts/flips/priority/palette/overwrites and
full door-band vs missing-end rejection), unobserved_wall, shared_face,
cross_bg_wall_support, geometry, geometry_cache, existing offscreen emitters,
emitter bases. ASan/UBSan tall mount test passes. Validator25 tests and
wall-torch-evidence5 tests pass; diff-check passes. No app build in this slice:
user explicitly reserved final integration build. No frontend/direct/clustering
changes were made.

## Artifacts / next action

Preapproved temp directory: `emitter007-gate-before/`, `emitter007-gate-after/`
(complete wrapper/planes/profile/manifests/graph/runtime/compiled map/v2 JSON+SVG),
`emitter007-corpus-before/`, `emitter007-corpus-after/`, `emitter007-corpus-delta.json`,
`emitter007-fixtures-before/`, `emitter007-fixtures/`, production before/after
packets and semantic inventories. Exact denominators remain in each
`*runtime.json` (`required_planes`, `missing_planes`, `solved_wall_cells`,
`unknown_wall_cells`, `structural_conflicts`) and `*validation-v2.json`
(`directional_wall_continuity`). The one-off summary printer was retired;
these outputs remain review artifacts.

Remaining shared capabilities: solve485 unknown007 walls via proved structural
ownership/contacts, complete arbitrary door assembly regions/orientations, fix
independent reference diagonal validation, and full-room offscreen occluder
transport. Finish integration build then obtain fresh gameplay camera/state
comparisons; none of the automated source checks alone establish scene appearance.

Final integration: arm64 Release `xcodebuild` succeeded, including runtime and
Metal frontend integration; `git diff --check` passed. Updated product is
`build/release/Snes9x.app`. Fresh live room050 scrolling/height appearance remains
to be checked after quitting and reopening the rebuilt app.
