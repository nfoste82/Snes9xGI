# Rooms002 own004 / 012 own031 — partial repair, not room approval

Both user captures were replayed with their own immutable profile (serialized
FNV96fbd008f2ae3a2f), complete ALTPDI1/ALTPDT1 and terrain evidence. Production
EndFrame matched78575/64709 BG pixel observations respectively. Both are settled
module7/submodule0: room002's earlier dialog lifecycle repair is not this issue.
No profile, authored height/normal, ROM or original capture was modified.

## Repair

Room012's north-door jamb `f9a35cc3c64ee363` has authored0..7 relief, but collision
00 on the structural plane. Ordinary wall seams correctly avoid treating such
art as an underpass ramp; they also discarded this independently bounded jamb.
The complete north-door outer4x3 ROM footprint0x1470 now permits terminal
attachment of its outer middle members, requiring wall roles, existing relief,
both independently solved solid end bands, full eight-sample affine seams and
agreement between admissible planes. No graph edge/support datum is added.

Both placements(30,5)/(33,5) now base32/effective32..39 rather than0..7 in the
actual EndFrame replay. BG2 solved/unknown walls906/54 ->908/52, conflicts2
unchanged, required supports[782,0],missing[0,0] unchanged. Intrinsic arrays
remain untouched. No room/hash exceptions. Other door orientations/templates
are outside this narrowly evidenced rule.

`dungeon_door_jamb_height_test.cpp` checks all4flips, signed bases-256/0/32/300,
opening exclusion, authored-source preservation, partial writes, absent or
contradictory ends, nonblocking end, nonaffine source, incompatible role and
multiple differing structural owners. Checks remain active under NDEBUG.

## Unresolved reported surfaces

Room012: `a2bb56b8e036dcdb` and `6259c84953e22501` have wall semantics but NO
authored local height in the pinned profile. `dcb66380243b0151` has an all-zero
array with no authored normal. `45dae355f7847291` has clipped0..2 relief whose
edge samples do not independently match the neighboring backing ramp. All
remain unknown, not solved via arbitrary hash offsets or generated placeholder
arrays. Side-wall parts reproduce the full3x4 ROM decoration0x89a/0xa54
(counterparts0x8b2/0xa6c), but a footprint alone cannot infer missing intrinsic
shape or its region/backing composition. Required next capability: authored or
ROM-part-supported local region geometry plus explicit backing/relief contacts,
preserving existing authored arrays. North cap members need the corresponding
decorated-wall assembly/flat-face ownership rather than gradient-only seams.

Room002: replay exactly reproduces the captured bases; BG1 walls566solved,
96unknown; BG2 walls750solved,0unknown,0structural conflicts. Visible high bands
already receive bases36/44/52 (effective36..43/44..51/52..59), adjacent ceiling
art is local60. Missing BG1 class is high outer-corner strips0xb66..0xbc6 along
the center passage, with no accepted support datum. This inventory does NOT
establish that these are the user's entire 'above a certain height' symptom or
that the high-shell datum is correct. Need independent cross-BG ownership and
height/contact evidence; a blanket ceiling/height increase was not applied.

## Mandatory complete-room v2 gates

Lossless complete ALTPPD1 wrappers, matching ALTPAT1 planes, pinned capture state
and profile, observed-seed static cardinal graphs, compiledALTPHM3, verified ROM:

*002: support730/730,planes[0,730],fallback8/8,missing0,component conflicts0;
 compiled-map missing8/mismatches8; directional contacts11296,missing3640,
 conflicts0; exit1.
*012 before/after: support118/118,planes[118,0],fallback22/22,missing0,
 component conflict1; map missing2, mismatches8 ->10; exit1 both.
 The extra two map mismatches expose the validator's independently reconstructed
 support expectation for the newly attached nonblocking jamb; terminal-art versus
 gameplay-support representation still needs explicit independent validation.
 The gate was NOT relaxed or represented as passed.

These graph denominators are smaller than runtime support scope, not full dynamic
state/entry enumeration. Whole-room two-BG placement inventories have8192rows.
Neither room is approved; no fresh GUI/lighting comparison performed.

Paired corpus audit:346default/entrance contexts,319/320room IDs, selected capture
profile used as research metadata (not matching historical gameplay approval).
BG2 solved83879 ->83881,unknown262755 ->262753,structural conflicts17975
unchanged;1changed context,0unsupported contexts. Dynamic switch/chest/water/door
variants are not enumerated. New Debug and NDEBUG jamb regressions pass including
actual frame application; existing tall-wall-mount, owned-wall projection,
cross-BG support, geometry and wall-cap tests pass. Release macOS build succeeded
at `build/release/Snes9x.app`; git diff --check passes. No interactive approval.

Artifacts under `$TMPDIR/opencode`: `walls002-before*`, `walls012-before*`,
`walls012-after*`, `walls002-gate/`, `walls012-gate/`, `walls012-after-gate/`,
`walls002012-corpus.json`, `walls002012-release-build.log`. Diagnostic placement
export adds actual `has_height` and explicit-floor status so absent sources are
not confused with an all-zero array. Existing unrelated dirty work preserved.
