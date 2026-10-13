# Room 0x050 offscreen source audit — blocked by missing evidence

This is source-recovery diagnosis, not room-height approval or a restored-light
claim. No floor solver, placement base, frontend or hidden clustering was changed.

## Pinned reproduction

Own-input captures under `~/Library/Application Support/Snes9x/Remaster/`:

| Capture | Serialized profile FNV1a64 | Matching BG pixels | Emitting placements | Unknown bases |
|---|---|---:|---:|---:|
| `frame_room050.s9xrmf` | `5e1caa974dab0652` | 89,217 | 6 | 2 |
| `frame_room050_001.s9xrmf` | `4dc7ca99efe236ee` | 91,226 | 8 | 4 |

Both pin USA ROM SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`,
room 80, module7/submodule0/construction0, collision mode0, Link plane1,
graphics bases16384, rise50 and complete own maps/attributes/VRAM/templates.
State identity uses each capture's own complete input and serialized profile,
not a room-only key. Original capture scroll is [256,2585]. No entrance-based
height anchor is introduced.

The production EndFrame replay proves matching visible identities/flips/tilePixel
against completed maps on main/sub/scene buffers. The four flame cells
(49,7),(50,7),(49,8),(50,8) have visible base0, support [255,0], no structural
base. Their 96 emitting pixels are in viewport and correctly remain exclusively
on the visible path. Both replays publish zero spheres and zero hidden pixels.

Offscreen wall cells (59,16),(59,17), additionally (60,16),(60,17) in001,
have base255, support [255,255], structural [255,255], no diagnostic floor
owner on either plane. In001 these have depth1 and 82 emitting pixels; they
fail before transport because there is no existing solved placement base.
In the original profile the two unknown emitting cells have depth0, so they
also do not qualify for directional offscreen sphere promotion.

This falsifies the proposed adapter-only plane1 selection repair. `offsets` is
the visible artwork base; `planeOffsets` can be support *under* wall artwork.
Selecting Link's plane, copying a neighboring base or converting255 to0 would
manufacture geometry. Visibility can still render authored local emission when
placement is unresolved; that does not prove its physical offscreen datum.

## Implemented narrow repair

`offscreen_emitters.h` now excludes zero/black emission arrays before source
placement/unknown counting. These arrays are common non-source metadata; the
old diagnostic counted4318/4320 placements and383/385 unknowns instead of6/8
actual emitters and2/4 unknowns. This restores source denominators only, not
lights or effective heights. Room050 before/after complete map inventories and
production replay packets are byte-identical.

`offscreen_emitter_bases_test.cpp` checks plane1-only solved bases, signed
structural bases independent of actor plane, unknown artwork with support under
it, overlapping planes, no zero default, H/V flips, changed producer/room and
independent BG1 ownership. It also guards zero/black source denominators.

## Corpus and verification

`offscreen_emitter_capture_probe.cpp` extends the existing production own-input
replay without changing it. `tools/ALttP/alttp_audit_emitter_sources.py` runs it
over available own-profile sidecars and retains explicit rejections.

55 sidecar states in rooms041/050/060/061/062:46 audited,7 rejected for
packet/map/identity mismatch,2 unsupported due to absent complete templates.
Audited corpus:894 emitting placements,42 unknown visible bases; zero unknown
visible bases with a solved support-only or structural-only array to recover.
These are capture-state occurrences, not unique assets or all320 game rooms.
Room055 has no complete own-input sidecar in this corpus and is not checked.

Artifacts: preapproved temporary directory `emitter-source-corpus/`, including
`emitter-source-audit.json`, own-profile replay packets and complete inventories.
Optimized new/existing offscreen tests, ASan/UBSan new tests, arm64 macOS Debug
build and `git diff --check` pass. No new interactive gameplay comparison.
No height changes were made, so no room-wide height-validation gate is claimed.

## Smallest next action

Trace the unknown emitting wall placements through reusable ROM assembly/final
writer semantics and compatible signed structural contacts. Establish fixture
ownership and solve its base without borrowing actor support. That is actual
height-solving work and must include failure-class regressions, complete-room
validation v2 and a corpus audit before source recovery can consume the result.
Full-room offscreen occluders remain a separate transport limitation.

## Structural follow-up: exact evidence blocker

Sustained follow-up identified the final writer class, not merely nearby art.
ROM-matched reference `dungeon.c` subtype-0 handlers55/56 write north wall
torches with ROW-major4x2 templatesa14/a24;7f/80 write side wall torches with
COLUMN-major2x4 templatesa34/a44. The room050 atlas records handler80 at
(59,15), ordinal24, source692345. Its eight final BG2 words exactly matcha44
with uniform palette change and priority-only writes; a34+H is an equivalent
art claim, and V symmetry produces additional claims. They establish the
same footprint but do not establish a signed Z datum. No room/hash whitelist
or actor-plane selection was introduced.

The owning side fixture has four unique identities: a819ba7614ee77d0,
fe630bf28976e706, cc05dbdf62879545, a5f3fec8d608599a. Only the two usages
of fe630bf28976e706 have local heights (canonical16,15,13,12,11,10,9,8).
The other SIX placed members have no local height array. All EIGHT perimeter
contacts with solved wall placements meet those missing members; thus none
can reproduce a signed exact seam. Adjacent solved bases are12/36/44/52;
those are local-band offsets, NOT interchangeable fixture-floor anchors.
Copying52 or a nearby floor is consequently unjustified. Original and current
profile research inventories both retain6/8 missing locals. No canonical
surface annotation for these identities exists in the tile semantics sidecar.

Latest room050002/003 packets have no complete sidecars. Inspecting their actual
visible sources nevertheless confirms the runtime gap:fe630 emits28 pixels at
effective15..16/base0; its animated opposite half713a/e3ce emits60 pixels at
effective0/base0. This is unchanged authored fallback, not a newly established
floor datum. These packets cannot prove complete-room state equality with001.

Two capture-pinned complete ALTPPD1 wrappers preserve original VRAM/maps after
checking all8192 WRAM/PPU words; ALTPAT1 preserves both full collision arrays.
ROM templates are checked against the verified ROM. The observed room-local
Link seed [402,133] on plane1 supplies traversal scope only, never a height
anchor. No stairs are present, and none were borrowed. Complete static graph
scope is[0,376] cells; runtime broader required support remains[0,3564]. Both
v2 validators exit1:376/376 valid supports,4/4 fallbacks,419 solved walls,
489 unknown walls,0 missing placements,0 conflicting components,0 compiled-map
missing/mismatches. These are failures, not room approval; fresh movement and
camera/state comparisons remain absent.

Follow-up corpus inventories every177 available room packet:53 have complete
own inputs/templates/profile,124 explicitly lack evidence (including050002/003).
Across rooms041/050/060/061/062 there are4 exact wall-torch footprint occurrences,
32 members,12 missing local members and2 emitting members missing local geometry.
These are the two north and two side occurrences in the older050 states. The
atlas separately inventories117 ROM object records in this handler class:
44 handler55,32 handler56,22 handler7f,19 handler80. Atlas presence is not
complete final-state coverage. The earlier source-parity audit remains46
accepted/7 mismatched/2 missing templates; ownership inventory includes the7
mismatched packets ONLY as complete-input candidates, not gameplay parity proof.

New tools: `alttp_wall_torch_evidence.py` inventories canonical core identities,
complete transformed templates, equivalent claims, absent local arrays and
capture-matched exact perimeter votes; `alttp_emitter_packet_probe.cpp` compares
actual visible effective heights; `alttp_emitter_live_gate.py` runs pinned
whole-room v2 with explicit observed-seed scope. Five Python regressions cover
the fixed C++ hash vector, both writer layouts, transforms, priority/palette
changes, partial overwrite rejection, room boundaries and symmetric claims.
All pass; new/existing portable emitter checks and diff-check also pass.

Artifacts under the preapproved temporary directory:
`emitter050-torch-evidence.json`, `emitter050-torch-current-profile.json`,
`emitter-wall-torch-corpus.json`, `emitter050-structural-gate/` and
`emitter050-original-gate/` (snapshot, planes, profile, manifest, graph, runtime,
compiled map, validator JSON/SVG and summary). No structural runtime repair is
published: the irreducible input needed is reusable authored fixture local
geometry/contact regions (or equivalent ROM-derived signed fixture-to-backing
wall datum). The ROM handler proves XY ownership but does not contain Z geometry.
Author/verify those canonical contacts, then implement terminal wall-assembly
attachment, preserving transforms and rejecting partial/ambiguous claims, and
rerun this gate/corpus. Guessing a datum would hide the defect rather than fix it.
