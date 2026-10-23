# Room061/048 — reviewable but incomplete (independent v2 proof implemented)

Validator-only/tooling follow-up; no C++ runtime, renderer, authored array,
profile or schema edits in this follow-up. Parent owns builds/independenttests.

Exact048 v2 now **exit1**, not exit2: JSON/SVG enumerate98independently proved
diagonal placements,42partial owned-tangent sources; supports1568/1570,
walls956solved/2unknown,0conflictingcomponents,compiled22missing/40mismatches.
Still NOT room approval. Two missing reachable local sources and existing compiled
representation discrepancies remain genuine blockers, never waived by new proof.

## Independent evidence

New validator arguments `--provenance-snapshot` and `--atlas` require verified
USA `--rom`. ALTPRV1 is read against its complete reference ALTPPD1; all8192
FINALwords must equal exactlive048 maps. Atlas ROM SHA must match. Reconstruct
diagonal dispatch footprints with `alttp_diagonal_sets.inspect`: object phase,
ordinal,handler9..32,source words,destination,bands,finalwriter/priority-only
overwrite attestation. Equal-word/unobserved/ambiguous owner is NOT accepted.

Reference VRAM differs in24animated placed identities, but completewords match.
Writer evidence attests words, not animation art: hashes/semantics/local arrays
are decoded ONLY from exactlive048 snapshot/OWNprofile. No graphics substitution.
Per-source evidence records original writer,handler,templateoffset,semanticband,
companion,top and64canonical samples; proof-input SHA256 retained in v2JSON.

Grounded semantic extent seed is independent of cardinal-section proof. Exact048
cardinal0x2c8 authored band does NOT satisfy that alternative convention; previous
Python early return incorrectly suppressed the independently admissible semantic
foot/top solve too. Grounded zero/+Z foot,corroborating groundedcompanion,explicit
nonmatching boundaries,all surviving owned/transformed members prove the seed.

Partialattachment audits WHOLEsurviving column rather than copying one runtime
pixel/witness. EVERYband must have exacttransformed finalword/hash and identical
writer/phase/handler/band/template ownership to the companion. All64canonical
samples agree with independently solvedclipped ramp; transformedsame-band tangent
displacement cancels displayed8,-8 along x+y. Entirecolumn phase must agree with
actualtop atmaximum+7; wrong/unknownterminator,mixedH/V,changedidentity,foreign
writer,missingownedseam and contradictorycompanion phases reject. Ambiguous
proposals permanently reject rather than choosefirst. No runtimepreview consumed
to makeproof; only compared after proofproduction. Withoutprovenance partials
remainunproved and gateexit2 remainsappropriate.

All346defaultcontexts/319roomIDs,bothBGs independently audited with OWN048profile:
onecontext has42partialproofs (061),others0. Dynamicnondefaultstates/missing320th
room/equalwrites remain unsupported, not countedapproved. This adds evidence,
not a game-wide heightapproval. Artifact `partial-proof-corpus.json`.

## Pot local sources — current user authoring already supplied

Read-only currentprofile SHA256
`9e5dd3b1fbea2076cd52ec13fc512b7f9bbc43bf36937ea8c51bbe708bb3ec9e`,stable duringaudit:

|C2 OBJchild|canonicalhash suffix|currentheight|currentnormal|
|---|---|---|---|
|46|21c1767459218910|64samples,0..5|absent|
|47|a9b04900f78069e6|64samples,0..5|absent|
|56|ac36befaa085a824|64samples,0..4|absent|
|57|a98b371291ed848a|64samples,0..4|192samples|

Allfour were absent from capture019's OWNprofile; do not substitute currentprofile
into historicalgate/replay. User authored concurrentchanges are preserved. Current
metadata presence is not independentproof ofshape correctness; latestprofile
reload/currentcapture can establish thatlocal undefinedness is actually closed.

ExactcapturedVRAMcanonicaltile IDs verified ataddresses35008/35040/35520/35552.
Compared every64palette-indexpixel against both ROMSinglePottemplates e82/e92,
allfourparts/allHVtransforms: **zeroexactrelations**. BGcharacterbase16384,
OBJactualnamebase32768. Thus nohashalias/decodedflip relation permitsBG-toOBJ
relieftransfer. ProducerEC/C2 connects physicalobject lifecycle,not identical
intrinsicartgeometry. Noindependent3Dauthoringreference supplied beyond current
userarrays; noarrays added/copied. Preferredscope:fourcanonicalOBJ8x8childassets,
viewed/authored as ONE16x16C2assembly with intrinsicrelief only; preserve separate
BGgroundpot/pedestal/replacementfloor and runtimeownedbase/lift. OtherCvariants,
giganticthrownobjects/breakgarnish/animationidentities require their ownmetadata
inventory,notgenericpotcopy. Current57normals do not justify cloning toothers.

## Commands / tests / artifacts

```sh
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_partial_diagonal_proof.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_diagonal_preview.py'
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_alttp_validate_room_heights.py'
git diff --check
```
PASS3newtests (allfourflips/wholeownedseam/foreignwriter/ambiguous/wordreplacement/
unknownboundary/missingseam/wrongphase/twocompanioncontradiction),4existingpreview,
23validator. Additional `test_diagonal_sets.py`:9pass/1FAIL authored-profile
expectation `test_low_authored_bands_and_full_wall_direction`; it comparescurrent
userarrays against oldtransfer expectations (31versus24 firstsample). No runtime
or array repair made to satisfy that unrelated dirty-profile fixture.

Exact gatecommand persisted here (reexecute, EXPECTexit1):
```sh
ROOT=/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode
python3 -B -c 'import json,subprocess; from pathlib import Path; p=Path("/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/action-height-review/live048/independent-gate.json"); raise SystemExit(subprocess.run(json.loads(p.read_text())["command"]).returncode)'
python3 -B remaster/tools/ALttP/alttp_audit_partial_diagonal_proofs.py --corpus "$ROOT/diagonal-sets-priority" --profile "$HOME/Library/Application Support/Snes9x/Remaster/frame_room061_048.s9xrmf.profile.toml" --rom build/remaster-geometry/room-preview-engine-v21/zelda3.sfc --atlas build/remaster-geometry/alttp-atlas.sqlite --output "$ROOT/action-height-review/partial-proof-corpus.json"
python3 -B remaster/tools/ALttP/alttp_pot_local_source_audit.py --capture "$HOME/Library/Application Support/Snes9x/Remaster/frame_room062_019.s9xrmf" --profile remaster/profiles/ALttP/alttp-profile.toml --output "$ROOT/action-height-review/pot-local-source-audit.json"
```
Gateadds `--provenance-snapshot "$ROOT/diagonal-sets-priority/room-061-entrance-4.raw"`
and`--atlas build/remaster-geometry/alttp-atlas.sqlite` to prior exact048command.
Output `live048/validation-v2.json`,SVG,independent-gate.json; sameOWNprofileFNV
90f968a81fe8e52f/SHA769f20ff...,ROM66871d66...,state/plane/camera unchanged.

Smallestnextaction:parenttests; fixremainingtwoeffectivegroundlocals and compiled
representationmissing/mismatches; compare currentauthoredpotprofile with actual
pickup/carry/throw gameplay. No freshGUIcomparison or build claim here.
