# Humanoid NPC geometry pass

The user approved the Link proposal on 2026-10-19. Its exact asset changes and
OBJ classifications were promoted to `Profiles/ALttP/alttp-profile.toml` using a
three-way field merge. Existing detailed geometry was preserved. No git push is
involved. The optional `--promote-approved-link` flag performs that merge and
rejects conflicts with newer authored fields before writing.

The other NPCs remain a separate **review candidate**, following the same
proposal/review workflow:

```sh
python3 -B tools/ALttP/alttp_humanoid_geometry.py
python3 -B -m unittest discover -s tools/ALttP -p 'test_*geometry.py'
```

Load `build/remaster-geometry/humanoid-geometry/humanoid-profile.toml` in the app.
It contains the approved Link geometry **and** the NPC proposals. The generator
does not change the main profile unless explicitly promoting approved Link.

## Scope and preservation

The explicit family inventory includes 70 named visual families, 155 contexts
and 1,361 unique identities: people, guards, thieves, robed humanoids, fairies,
Armos, Stalfos/Gibdo, Wizzrobes, Freezors, Pengators, Eyegores, Kiki, Ganon and
Blind. Nonhumanoid creatures, blobs, hands, quadrupeds and storytelling object
forms are excluded. Seated humanoids and fairy torsos are included even though
their poses are not standing bipedal locomotion.

The pass changes 1,214 identities: 515 added height maps, 528 added normal maps,
686 replaced whole-tile +Y placeholders, and seven flip-specific geometry sets.
Every existing height, non-placeholder normal, other asset field, geometry
variant and named context is preserved. Approved Link is unchanged. This also
means existing flat heights and non-+Y uniform normals remain authoritative.

Geometry uses explicit head/crown/face, torso, sleeve/hand, robe/skirt, separate
legs/boot tops, armor planes, and shallow prop/wing surfaces. Material hue can
segment skin from clothing; brightness never becomes relief. Outlines inherit
nearby physical surfaces. This is **not** a 3D reconstruction or a learned
anatomy model, nor is Link's exact face geometry copied to unrelated characters.
The recipes remain approximate, especially for side-facing silhouettes, snouts,
horns, pointed hats, attached tools and elaborate dresses.

## Review artifacts and evidence gaps

`review-*.png` columns are artwork, normals, height (0–40) and neutral-albedo
directional lighting. `review-index.json` gives page/row, actor, context and
facing. `report.json` contains per-family counts, changed identities, preserved
exclusions, missing maps, deferred poses and reuse disagreements.

- 26 graphics-inherited contexts are skipped, not silently accepted as verified
  character artwork. This leaves e.g. Bug-Catching Kid, Camper, the human Flute
  Boy, Average Middle-Aged Man and Uncle without new geometry in this pass.
- Eight identities are blocked by background/Blue Bari sharing or authored
  variants. Global hash geometry cannot safely distinguish those appearances.
- Fourteen later Blind boss frames are deferred: detached parts should not use
  upright-body geometry. Overlapping identities may still inherit first-frame
  proposals; those phase placements require review.
- Dark-world Flute Boy/tree transformation is deliberately not modeled as a
  human. Its current context is also inherited/unverified.
- 7,131 pixel/flip samples on 236 identities have proposals differing by more
  than two height units or 30 degrees. Mean normals and median heights reconcile
  those **approximately**, not as proven conflict-free geometry.
- Existing generated-ramp runtime alignment applies only to its exact signature;
  arbitrary custom heights need independent placement support. New custom maps
  are assembled-pose estimates, not validated live actor placement.
- The candidate passes the actual C++ profile parser; offline preview lighting
  is not in-game GI, animation continuity or assembly validation.

Best reference-image targets: **Ganon**, the **Great/Waterfall/Cursed Fairies**,
and the **dark-world Flute Boy/tree form**. Their nonstandard anatomy needs more
than Link's reference. Blind's detached phases primarily need phase/ownership
evidence, not just a character portrait. Graphics-inherited exclusions need
correct graphics-context reconstruction, not reference images.
