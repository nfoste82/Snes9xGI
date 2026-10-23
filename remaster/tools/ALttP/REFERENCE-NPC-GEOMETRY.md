# Reference-guided NPC revision

## Frame17 mirrored centerline v6 (2026-10-20)

Load `build/remaster-geometry/reference-npc-detail-v6/reference-npc-profile.toml`.
The final Ganon draw-table assembly has a different role layout from frames9–15:
slots5/6 head,7/8 shoulders,9–12 arms,13/14 torso,15/16 boots. The generic late
fallback had rounded the mirrored head halves as independent shoulder pads and
the torso halves as independent arms. Corrected role metadata makes head and
torso share the existing assembled centers instead of pointing inward at x7/8.

Only16 generated Ganon normal maps differ from v5. All existing heights, other
profile fields and the user-authored normals remain unchanged. v5, edited v4
and main are retained. `v6/v5-generated-baseline.toml` records the exact unedited
v5 generator output before the correction; three-way normal merge uses edited
v5 as its source. Do not regenerate that baseline with newer logic.

Generated-profile regression across both frame17 facings, head rows[-8,-2] and
torso rows[4,12], x7→8 (32 pairs): maximum normal angle113.68° →8.57°;
mean absolute lateral component0.563 →0.036. Targeted counts are retained in
`frame17-centerline-audit.json`. Upward head surfaces can remain +Z-dominant;
this check requires near-zero X at the center, not indiscriminate +Y everywhere.
Other part edges, small anatomy and later-phase detail still need review.

## World-up calibration and frame16 provenance v5 (2026-10-20)

Load `build/remaster-geometry/reference-npc-detail-v5/reference-npc-profile.toml`.
This revision uses the user's edited v4 as the actual merge source. All five
normal maps changed by the user (shoulders, skull/necklace and a head tile) are
preserved exactly, along with every existing height, sampling option and other
profile field. Only 52 generated Ganon normal maps change. Main and v4 are not
written. The exact unedited v4 regeneration was saved before modifying the
generator as `reference-npc-detail-v5/v4-generated-baseline.toml`; do not replace
that baseline with a new generator's output.

The previous front-oriented model lacked truly world-up caps. Ganon now has a
continuous anatomical top region approaching +Z on the head, snout, shoulder
armor, arms/fists and feet, blending into front-facing surfaces without resetting
at tile edges. User-authored shoulder maps remain authoritative, not overwritten
with approximations. This calibration currently targets Ganon, not a blanket
rotation of all characters or every normal vector.

Frame16's banding was also a classification failure: hash-first late-frame slot
fallback labeled some newly drawn head tiles as arms and a torso quadrant as
head. The ROM-backed Ganon_Draw DMA head block and frame15's specific OAM
assembly now override that fallback. Side-by-side frame16 head boundary audit:
15 horizontal neighbor pairs at x7/15, maximum123.48° →17.66°, p95119.96° →11.31°.
These are targeted head seams, not evidence that all late-phase arm/cape geometry
or every anatomical detail is correct. Skull/bead detail and hash reuse still
need visual review. All five user-edited normal maps were checked for exact
preservation after merging; not just the subset conflicting with new proposals.

Reproduce using `--output build/remaster-geometry/reference-npc-detail-v5`,
`--edited-profile build/remaster-geometry/reference-npc-detail-v4/reference-npc-profile.toml`
and `--generated-baseline build/remaster-geometry/reference-npc-detail-v5/v4-generated-baseline.toml`.

## Ganon jewelry/armor detail v4 (2026-10-20)

Load `build/remaster-geometry/reference-npc-detail-v4/reference-npc-profile.toml`.
This preserves v3 and the main profile. Only Ganon normal maps differ from v3;
existing heights and every other actor are unchanged.

User-supplied front/three-quarter references clarify the skull pendant, beaded
necklace, red armor insets and gold borders. Front-phase detail now has one
assembled pendant spanning mirrored torso halves: rounded cranium, recessed eye
sockets, cheek/jaw returns; necklace beads, rolled pauldron rims and separate
chest plate orientations. Color pairs segment material regions, not individual
painted highlights. Dark outlines inherit nearby material instead of acquiring
their own random bumps. Broad v3 surfaces remain underneath; no whole-row normal
replacement or restarted arm domes are reintroduced.

These landmarks are applied only to front assemblies (steps0–7), not stamped on
back/turn phases. Some reused hashes can still transfer front detail to other
placements, so phase continuity requires review. Detail is normal-only, without
matching new height or occlusion relief. At this resolution the skull jaw/beads
are approximate; no complete reconstruction of the reference model is claimed.

The earlier smooth-belly angular metrics are retained in the report but now
include intentional pendant/armor edges. Their increased counts are not evidence
that detail is correct, nor should their old thresholds force flattening the
pendant. Regression tests instead pin skull symmetry, inward sockets, continuity
of the underlying front model and absence of a gold-plate row switch.

## Horizontal-band repair v3 (2026-10-20)

Latest candidate:
`build/remaster-geometry/reference-npc-detail-v3/reference-npc-profile.toml`.
Generated from the unchanged main profile; v2 remains available for comparison.

The remaining broad blue bands were in the normal data, not the now-removed
browser outlines. Ganon's front belly/head/arm used a far-crown normal rolloff
despite the artwork starting partway down those surfaces. The repair uses a
front-exposed rounded model without that crest. Overlapping arm slots now share
one limb extent rather than restarting their curvature at each slot. Snout
transition blends into the face instead of switching at a horizontal threshold.
Cloth folds, other NPCs and existing authored heights/normals are preserved.

Generated-profile audit of the reported upper-belly rows (176 vertical neighbor
pairs across steps0–7 and both facings): maximum angular change **19.32° →
3.20°**, pairs exceeding15° **80 → 0**. This is a targeted band regression,
not a claim every part boundary is smooth. The older broad belly audit includes
arm/body overlap edges and its maximum increases to97.72°; report retains both
metrics rather than using the better targeted result to hide remaining edges.

Regenerate with `--output build/remaster-geometry/reference-npc-detail-v3
--compare-profile build/remaster-geometry/reference-npc-detail-v2/reference-npc-profile.toml`.

## Detail revision v2 (2026-10-20)

The user identified flat capes, Ganon's horizontal normal stripes, a spherical
guard visor and unsupported straight Armos armor structures. Revised candidate:

```sh
python3 -B remaster/tools/ALttP/alttp_reference_npc_geometry.py \
  --output build/remaster-geometry/reference-npc-detail-v2 \
  --compare-profile build/remaster-geometry/reference-npc-geometry/reference-npc-profile.toml
```

Load `build/remaster-geometry/reference-npc-detail-v2/reference-npc-profile.toml`.
The previous review candidate and main profile remain untouched.

- Ganon's crown normal previously jumped at `v=-.70`; a continuous rolloff now
  replaces that branch. A second stripe source was replacing painted gold belly
  details with a constant normal. Those details now follow the abdomen.
- Cloth normals derive from gradients of a continuous draped-depth function:
  shoulder-anchored folds spread/deepen toward the hem and undulate vertically.
  No phase resets at tile boundaries. Existing height layers still remain intact;
  these are normal-detail folds, **not matching new height/occlusion relief**.
- Ganon's cloth region is assembled by broad cape-bearing OAM parts, including
  their outlines. Front shoulder armor is no longer misclassified from red hue.
- Agahnim's robe and hanging sleeves get separate draped folds; the stole follows
  a gentler version of the underlying drape rather than cutting a flat stripe.
- Guards have explicit inner lip, side returns, lower rim and independent skin
  inside their helmet opening. Back-facing helmets have no invented opening.
- Armos's arbitrary rectangular breastplate patch is removed; visor uses an
  elliptical region and cuirass is rounded. Straight shield/blade faces remain.

Pinned front-belly vertical-neighbor audit (2,016 pairs): p95 angular change
falls from **35.32° to 4.72°**; >30° pairs fall from **176 to 40**. Remaining
outliers include real/approximate overlapping part boundaries; maximum is still
91.11°, so this is **not a zero-seam result**. Full counts/scope are in the v2
report. Reuse-disagreement tiles increase from 51 to 63 as pose-varying cloth
detail exposes limitations of hash-keyed averaging. Static cloth folds do not
constitute validated cloth animation. Both facts need review, not concealment.

---

The user rejected the broad humanoid pass as lower quality than Link and supplied
individual reference images for Armos Knights, Blue Guard (also red/green), Ganon
and Agahnim. This revision starts from the main profile with approved Link, **not
the rejected humanoid candidate**. Main profile is not modified.

```sh
python3 -B remaster/tools/ALttP/alttp_reference_npc_geometry.py
python3 -B -m unittest discover -s remaster/tools/ALttP -p 'test_reference_npc_geometry.py'
```

Load `build/remaster-geometry/reference-npc-geometry/reference-npc-profile.toml`.
Preview columns: artwork, semantic part masks, normals, height (0–32), left-lit
neutral albedo, right-lit neutral albedo. Page/row/context is in
`review-index.json`; mask colors are in `surface-colors.json`.

## Changed approach

- Inspect assembled art and isolated OAM parts before assigning geometry.
- Armos: separate helmet dome, horns, visor, cuirass, boots, sword and shield.
  Broad shield face stays planar with perimeter bevels; emblems aren't bumps.
  Jump-expanded quadrants map back to stable reference coordinates.
- Sword guards: explicit helmet slot, visor/face opening, separate breastplate,
  arm/hand/boot, blade and shield contracts for front/back/side assemblies.
- Ganon: head/snout/tusk masks, shoulder caps, arm/hand, abdomen and boots.
  Early-frame hash provenance survives later OAM slot reshuffling.
- Agahnim: hood, face, sleeves, hands, central stole and shallow robe folds.
  Avoid the earlier full-cylinder resets that produced vertical striping.
- Dominant semantic surface wins when a reused pixel has different part labels;
  do not average a shield plane into a body cylinder. Remaining disagreements
  are explicitly reported, not claimed solved.

The references guide these manually specified models; they are not fitted 3D
meshes. Painted highlights, palette brightness and cloth embroidery are not
depth. All existing heights (including ramps), non-placeholder normals, variants
and named contexts are preserved. Only missing heights and missing/confirmed
uniform +Y/+X placeholder normals are eligible.

## Scope and remaining limitations

359 identities change: 273 placeholder normal fills replaced, 86 new normal maps
and nine missing heights filled. Link and unrelated actors are unchanged, except
that nine other guard families intentionally share some of these same canonical
helmet/body/weapon identities; the report lists them.

Inherited/unverified graphics contexts are skipped. Some shared identities still
receive geometry from verified contexts. 51 identities have placement estimates
differing beyond the reporting thresholds. Ganon's later-phase anatomy, small
face/hand details, Agahnim side-facing folds and occluded artwork remain
approximate. Preserved height ramps are not new sculpted relief. The reference
images have not eliminated all semantic-mask inaccuracies. Review opposing
neutral-light images and animations before approval; parser success and layer
presence are not visual quality or runtime placement/GI validation.
