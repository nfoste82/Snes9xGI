# Reference local wall candidate (opt-in; incomplete)

## New authorized diagonal samples (2026-10-02)

The latest source is **not** the entire backup: backup has 147 assets, current
has 17,354, with 17,207 current-only assets. `alttp_reference_source_merge.py`
keeps current non-asset settings (backup differs in schema and asset groups),
all current asset fields except the five explicitly authorized fields: normals
of `503ab068dfa941d3`, `31aba23efb31666b`, `a4aa6ebd24fefeb7`, height of
`a4aa6ebd24fefeb7`, and full-floor zero height of `1a032ad7756c49db` (absent in
backup). It does not restore any of the other backup normals. Exact TOML literals
are retained for transferred normals; every merged-source normal is preserved in
the candidate. Root and user backup remain untouched.

```
python3 -B remaster/tools/ALttP/alttp_reference_source_merge.py --current remaster/profiles/ALttP/alttp-profile.toml \
  --backup remaster/profiles/ALttP/alttp-profile_backup.toml \
  --output build/remaster-geometry/alttp-profile-reference-diagonal-merged-source.toml \
  --report build/remaster-geometry/reference-diagonal-source-merge.json
python3 -B remaster/tools/ALttP/alttp_atlas_profile.py --atlas build/remaster-geometry/alttp-atlas.sqlite \
  --source build/remaster-geometry/alttp-profile-reference-diagonal-merged-source.toml \
  --output build/remaster-geometry/alttp-profile-reference-diagonal-v2.toml \
  --reference-templates remaster/tools/ALttP/alttp-reference-wall-templates-v1.json \
  --report build/remaster-geometry/reference-diagonal-generation-v2.json
```

Authored a4 cross-section over x+y=0..14 is
`5,5,5,5,5,5,5,5,5,4,3,2,1,1,0`. The new **review proposal** requires constant
diagonal bands, minimum zero and three consecutive unit descents before extending
that measured unit descent to a full-face `14-x-y` local ramp. All four facings
use H/V transforms. This is extrapolation, not proof of an assembly's total rise.
503a exact-up normals give cap region x+y<=1; the proposed ramp clips its high end
at 13. Intermediate rounded normals are not inverted into slopes. a4 and its
exact-source alias have explicit ownership and use the authored array verbatim,
not the full-face generator. 1a and exact-source alias 03d2 are explicit floors,
removed from wall membership, with zero local heights; their lighting normals
remain untouched. Configured floor rise is now preserved instead of forced to 50.

**Latest saved evidence:** backup SHA-256
`074486c8ad3cd1e1f5618f5469493950335a4a7821c00e6e067166aeaedbad40` now has
15 exact `[128,128,255]` floor normals and 49 `[220,215,128]` face normals on
user-identified mixed-floor 31ab. Canonical floor mask is
`0xf8f0e0c080000000` (row-major bit y*8+x), exactly x+y>=10:

```text
WWWWWWWW
WWWWWWWW
WWWWWWWW
WWWWWWWF
WWWWWWFF
WWWWWFFF
WWWWFFFF
WWWFFFFF
```

The bottom template requires this complete diagonal half-plane and proposes
`max(0,10-x-y)` from the measured a4 unit slope. All 15 floor pixels are zero;
wall samples span 1..10. This replaces the old bottom height only in the candidate,
not in the merged source/root/backup. Both exact-source usages (31ab and 8ea9)
have explicit ownership, excluding the generic full-face template. Calibration
rejects increasing/non-unit descent, nonzero datum, absent floor/face, and incomplete
floor bands. It records mask, rows, normal digest, boundary and full local samples
in `reference-diagonal-generation-v2.json`. Normals are region evidence only for
this user-identified part, not general floor labels or derivative slopes.

The supplied schema-19 gameplay packet has the five hashes visibly laid out in
the diagonal strip, but their local heights are all zero and its offsets use the
old 50-unit mask. It is useful artwork/identity evidence, not proof of this new
candidate's geometry or the complete pinned state. Complete fixtures confirm
same-writer upper/middle/bottom/cap contacts; adjacency alone is not ownership of
support or a signed seam. The existing runtime's exact seam checks conservatively
leave unsupported capped joins unresolved. No runtime change was made: accepting
rounded/clipped corner connections without assembly provenance would weaken the
gate rather than prove them.

Final v2 artifacts: `build/remaster-geometry/reference-diagonal-v2-validation/`.
055/060/061 supports 439/439,711/711,1570/1570; planes [439,0],[437,274],[1570,0];
missing support zero; conflicts 1/3/1; solved/unknown walls 685/533,225/264,484/266.
All room-wide gates exit 1. Room061 improves solved walls from 448 to 484 but
explicit classification also grows the wall denominator; unknown walls are 266.
Immediate fallback borders pass 29/29,118/118,130/130 with transformed local ramps
and directions in the complete placement lists. Room060 retains 24 overlaps and
independent plane denominators. No compiled placement map was produced (the exact
runtime solution is the input); compiled-map checking is therefore inapplicable.
Room061's conflict is an isolated flat component with 84 negative-base nodes,
zero equality mismatches, zero overflow; no clamping/normalization was introduced
to hide it. Missing wall classification/support and assembly seams still fail.
18 reference/preservation tests, six compiler tests, eight semantic-atlas tests,
eight validator tests and `git diff --check` pass. Runtime is unchanged, so no new
arm64 app build was required or performed. No approval and no fresh gameplay
comparison. Candidate SHA-256 is
`cacff60d9b938ba7d69f41d7ae7b36dc79f823d0411cd5f5bdad94221365a205`;
all 17,354 normal arrays and exact field literals match the freshly merged source.
The new floor clipping does not change complete-room solved/unknown counts: local
shape evidence alone does not establish missing placement/assembly contacts.
The sections below describe the
previous reference candidate, not the new diagonal-v2 result.

Generate from the **current edited** profile, never an old generated artifact:

```sh
python3 -B remaster/tools/ALttP/alttp_atlas_profile.py \
  --atlas build/remaster-geometry/alttp-atlas.sqlite \
  --source remaster/profiles/ALttP/alttp-profile.toml \
  --output build/remaster-geometry/alttp-profile-reference-walls.toml \
  --reference-templates remaster/tools/ALttP/alttp-reference-wall-templates-v1.json \
  --report build/remaster-geometry/reference-wall-generation.json
python3 -B remaster/tools/ALttP/alttp_reference_candidate_check.py \
  --source remaster/profiles/ALttP/alttp-profile.toml \
  --candidate build/remaster-geometry/alttp-profile-reference-walls.toml
python3 -B remaster/tools/ALttP/alttp_reference_wall_audit.py \
  --atlas build/remaster-geometry/alttp-atlas.sqlite \
  --fixtures build/remaster-geometry/surface-audit-room-local \
  --output build/remaster-geometry/reference-wall-evidence.json
```

Do not use `--update-source` for this review. Root and backup restoration belong
to the owner. Existing `normal_xyz` is never replaced, even when it matches a
legacy generator signature. The candidate check compares both parsed bytes and
the exact original TOML field text. Restoring a backup requires an explicit
normal-only merge reviewed against later user edits, then candidate regeneration.
Never replace the entire profile with the backup.

## Algorithm and limits

ROM-pinned, user-selected east/south/inside/outside reference arrays supply
local geometry verbatim: no range normalization, min/max corner synthesis,
derivative normals, nearest-floor selection, image similarity, or room rectangles.
Confirmed semantic roles/facings/topology select templates; exact-source aliases
come from the existing catalog. East/west use only H reflection; north/south use
only V reflection. Corners reflect both faces together. Existing lighting normals
always win. Only absent normals receive reference samples with reflected X/Y
components (`clamp(256-component)`, the existing encoded-normal convention).
Multiple distinct results are ambiguous; no compatible reference is missing.
Reports record reference/config/source/candidate digests and per-hash outcomes.
These are **local metadata classifications**, not solved placement coverage.

The user authorizes the current middle diagonal samples for confirmed full-wall
facings SE, SW (H), NE (V), and NW (H+V). Legacy signature equality does not
invalidate this explicit authorization. Bottom and upper are unresolved **hard
exclusions**, even if their annotations match a full diagonal facing.
An explicit reviewed
`height` array plus `floor_mask` can represent a clipped mixed-floor tile; marked
floor pixels must be zero. A matching legacy ramp or neutral zero array is not
proof of that region geometry. Unresolved entries do not override existing height
arrays; those old arrays remain unapproved input, not accepted templates.

The provided schema-19 room061 gameplay packet shows the diagonal strip's repeated
upper/middle/lower artwork but has zero local heights for all three hashes. Current
root middle `3bc2b767d36c52c3` and bottom `31aba23efb31666b` duplicate the prior
rounded whole-tile southeast ramp; upper `503ab068dfa941d3` is neutral all-zero.
Complete fixtures expose cardinal neighbors, flips and matching last writers.
The audit inventories them without claiming that same-writer adjacency proves
support, a floor mask, or signed wall seams. A reviewed assembly cross-section and
floor boundary remain necessary for bottom/upper. No runtime diagonal algorithm
was changed. Accepting middle changes classifications, but the regenerated candidate
is byte-identical to the earlier candidate (SHA-256
`cc1c21015015d556394e5aab9e8f6340ce187f1a2a9765f9b488ee3b2f721c7e`),
so the preceding complete-room gates still describe exactly these inputs.

`alttp_reference_wall_audit.py --preview PATH.png` writes a dependency-free
canonical artwork/height PNG and labeled SVG companion. Columns are upper,
middle, bottom; rows are diagnostic decoded artwork and current height samples.
Every pixel in these three artworks is opaque: there is no alpha silhouette to
identify floor. Bottom shares indices 9/10/11 with wall artwork, so a palette-index
rule cannot establish floor. Please supply eight rows of eight `F`/`W` characters
for bottom `31aba23efb31666b`, in the unflipped canonical orientation (x/y 0..7).
Also specify whether its wall pixels continue the middle profile at the same
local base, an 8-unit shifted base, or a different approved profile. For upper,
confirm its cap/face regions and height relative to middle. These are the minimum
missing authoring decisions; normals will remain unchanged.

## Required complete-room gate

Compile the existing fixture adapter, then run
`remaster/tools/ALttP/alttp_run_runtime_height_fixtures.py` using this candidate. It invokes
validation v2 over complete ALTPPD1/ALTPAT1, pinned state and doorway graph, and
writes runtime solutions, complete effective-placement JSON and diagnostic SVGs.
Current candidate results (055/060/061): supports 439/439, 711/711, 1570/1570;
planes [439,0], [437,274], [1570,0]; missing supports zero; conflicts 1/3/1;
solved walls 685/225/448, unknown walls 533/264/244. All three gates exit 1.
Separate plane arrays are retained, including room060 bridge/underpass overlap;
this run is not fresh gameplay proof of overlap behavior.

Smallest next action: owner reviews the 24 backup normal differences (including
east/south/outside references), restores only approved original normal fields,
and provides reviewed diagonal heights/floor mask and assembly contacts. Regenerate,
rerun complete-room gates, then compare fresh matching gameplay across cameras and
planes. This candidate is not suitable for promotion as approved room geometry.
