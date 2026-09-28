# ALTTP linked inspection captures

Status: corrected first dungeon room prototype and geometry proposal path,
2026-09-26. Room `0x061` has local
inspection captures for its full 512×512 layout. They can be opened with
**View > Open Remaster Frame...** in Snes9x and inspected with the existing
Height, Normals, Occlusion, Emission, and Composite views. This path does not
need a save state or manual playthrough.

## Current local result

The ignored directory `build/remaster-geometry/inspection-room-061-full/`
contains `manifest.json`, 24 `.s9xrmf` packets, and one PNG preview per
packet. Six overlapping 256×224 viewports cover the whole room: x=0/256 and
y=0/144/288. Four game times, 0/8/16/24 frames after room initialization,
were sampled for every viewport. All 24 previews are distinct because of
moving sprites and small color changes, but the linked BG tiles did not change
at these four sampled times.

The upper-left packet
`room-061-x000-y000-f000.s9xrmf` contains 57,048 linked BG pixels out of
57,344 screen pixels, 81 distinct visible BG tile identities, and 1,920 BG
tile instances. All 81 identities have profile height, normals, occlusion,
and emission arrays. The room-specific upper-floor offset is applied to 267
tile instances in this viewport. The profile's existing semantic material
rules match 401 instances; 1,519 have no material rule. Their per-pixel
metadata is still present and visible in the diagnostic views. Unlinked pixels
are chiefly reference-engine sprites and backdrop. The file was serialized
and strictly decoded again before publication.

The first preview had forced room `0x061` through entrance 0, which belongs
to another room and loads graphics theme 3. The ROM's own entrance 4 loads
room `0x061` with graphics theme 4. Regenerating through entrance 4 fixed most
missing/wrong railing, wall, and trim tiles. A remaining horizontal strip was
caused by jumping the camera before the reference engine had streamed that
offscreen tilemap quadrant into VRAM. The renderer now copies the room's
expanded BG1/BG2 maps into the PPU tilemaps before drawing an inspection view.
The upper-floor mask was also narrowed at the inner wall and stair opening.
Against the supplied Snes9x gameplay capture, all 54,951 pixels with BG tile
links in both packets have matching tile identities, effective heights, height
offsets, and BG layers. Before these corrections, 6,365 matching-tile pixels
had an erroneous extra 48 height units. Sprites and the reference renderer
HUD remain different.

## How it works

`tools/build_alttp_room_renderer.py` builds a local Zelda3 reference renderer
from the user's verified ROM. The adapter
`tools/alttp_room_preview_adapter.c` loads a requested dungeon room through
the reference game engine's entrance path and publishes its complete expanded
BG1/BG2 tilemaps to VRAM for direct camera positioning. A small hook in the ignored copy of
the reference PPU records the winning BG priority/color word for each pixel,
the per-line BG registers, and the decoded VRAM/CGRAM. The raw intermediary
has magic `ALTPPD1\0` and is kept only until conversion.

`tools/alttp_linked_room_capture.cpp` reconstructs the BG1/BG2 8×8 source
pixel, tile flips, tilemap cell, palette, and canonical v1 content hash. It
verifies the decoded palette index against the PPU's winning color word before
linking a pixel. Then it attaches the chosen profile's visible asset metadata,
material matches, and the existing room-specific upper-floor height offsets,
and writes schema-19 `.s9xrmf`. `tools/alttp_make_room_inspection.py` runs
the selected viewports/times, writes a manifest, and creates lightweight PNG
previews. The PNGs are navigational thumbnails; the `.s9xrmf` packets contain
the geometry fields.

The newer renderer also traces each room-object and door dispatch. It writes
an `ALTPRV1` sidecar that maps both 64×64 BG tilemaps to their last room-loader
writer, checks them against the final linked tilemaps, and marks later
untraced changes separately. The inspection producer consumes and removes
that temporary sidecar when generating a geometry report.

Example with the local, ignored ROM-derived build:

```sh
c++ -std=c++17 -O2 tools/alttp_linked_room_capture.cpp \
  -o build/remaster-geometry/alttp-linked-room-capture-v2
python3 tools/alttp_make_room_inspection.py \
  --renderer build/remaster-geometry/room-preview-engine-v15/alttp-room-preview \
  --converter build/remaster-geometry/alttp-linked-room-capture-v2 \
  --profile build/remaster-geometry/alttp-profile-candidate.toml \
  --geometry-atlas build/remaster-geometry/alttp-atlas.sqlite \
  --room 0x61 --all-viewports \
  --output build/remaster-geometry/inspection-room-061-full
```

The producer chooses a ROM entrance for the same room when one exists, and
records the actual entrance, source room, graphics theme, and whether its
context was inferred. The renderer can be rebuilt from a local `snesrev/zelda3` checkout and the
verified USA ROM using `tools/build_alttp_room_renderer.py`. `--asset-cache`
can reuse a previous extraction made from the exact same ROM, avoiding a
second Python asset extraction. The ROM, engine copy, packet library, and
previews remain ignored local artifacts.

With `--geometry-atlas`, the inspection producer writes an experimental
`geometry.json`, `geometry.png`, `height-proposals.png`, and
`height-proposals.bin` beside the capture. Geometry inspection requires a
renderer that supplies the checked `ALTPRV1` provenance export. The report
keeps flat art from different structural objects separate and lists wall and
door attachment suggestions based on authored pixel heights at tile seams.
It also writes `tile-semantics-template.json`: edit canonical face masks and
orientations there, then pass the file using `--geometry-semantics`. For an
unchanged room state, each variant gets a `-proposed.s9xrmf` capture with the
suggested BG2 placement offsets added to the tile's authored per-pixel
heights. Open it beside the ordinary capture to compare their Height previews.
If the BG tilemaps change after the first variant, the manifest marks
`geometry_state_changed` and the producer does not reuse the old proposal.
The proposed capture is a review artifact; it does not update the profile.
An opt-in live review build can now read the same compact map beside the
candidate profile for gameplay comparison. See
[the live review steps and report fields](alttp-elevation-reconstruction.md#check-the-proposal-during-live-play).

## Accuracy boundary and next work

This is a **linked inspection capture from the reference Zelda3 PPU**, not a
recorded Snes9x emulation frame. Its original colors and BG priority decision
come from that renderer. Visible BG1/BG2 pixels have exact decoded tile links
that use the same v1 hash algorithm as Snes9x. Sprites, HUD, and backdrop
retain color but do not carry tile/profile links; sprites may repeat when the
room is viewed from different scroll positions. The preview also inherits the
reference engine's inferred graphics context and occasional stray HUD-like
art at the top of room images. No game-state switch/chest variants are
generated yet. The ordinary capture's floor offset is the existing room mask.
The separate proposed capture uses the offline solver and is not yet validated
against a matching gameplay frame for every placement.

Next prove an animated BG fixture produces distinct sampled frames, then add
an overworld loader for both worlds and context variants. A native Snes9x
scene browser can use these packets through the existing replay path; it
should display this capture provenance and coverage limits. A later Snes9x
runner can replace reference-rendered packets where exact PPU ownership and
sprite links are needed.
