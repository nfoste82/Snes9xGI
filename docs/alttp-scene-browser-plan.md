# ALTTP scene capture browser

Status: design with a linked-packet and geometry-review prototype,
2026-09-26. The current HTML room atlas is an interim visual review tool.
Room `0x061` has a first linked inspection library; rooms `0x055` and `0x061`
also have local proposed-height inspection fixtures. See
[the prototype and accuracy boundary](alttp-linked-inspection-captures.md)
and [the geometry workflow](alttp-elevation-reconstruction.md#reproduce-and-inspect-one-proposed-room).
A native browser and comprehensive library have not been built.

## What constitutes a scene

The ROM has 320 dungeon room IDs, but an ID is not a complete rendered scene.
The local atlas finds placed room objects in 264 IDs; 56 have none. Every ID in
`0x128..0x13f` has zero placed objects and zero placed sprites, explaining the
plain default-layout previews. This is evidence of empty placed-object lists,
not proof that scripts never use those IDs or that more dungeon room IDs exist.
A room can still have multiple useful visual
states: entrance graphics and palette, opened doors and chests, water level,
switches, animated tiles, encounter state, Link position, and camera quadrant.
The reference-engine preview currently chooses one entrance/context and one
default state. Its static room image cannot establish coverage of those states.

The overworld has a different topology. The atlas expands 160 base Map32
quadrants to Map8 positions and records 112 area heads. A quadrant is a map
chunk, not necessarily one entered scene: the light/dark world, progress stage,
graphics theme, palette, animated VRAM uploads, overlays, and persistent map
changes select the visible result. Special indices 128–159 can be entered maps
or overlay sources; they cannot be assigned one universal palette/theme. A
visual grid must show world position and context, not reuse dungeon room IDs.

The stable scene key should therefore be `(ROM SHA-256, domain, map/room ID,
entry or graphics context, state variant, camera viewport)`. Store exact source
registers/addresses and the way the state was reached. A `room_id` or tile hash
alone is insufficient. A full 512×512 dungeon room needs multiple 256×224
Snes9x viewports; one `.s9xrmf` is a viewport capture, not a whole room map.

## Why the current PNGs cannot become lighting captures

The HTML PNGs are rendered by the reference Zelda3 engine. They contain final
RGB pixels, but no Snes9x main/subscreen owner buffers, winning tile-instance
links, decoded 8×8 content IDs, VRAM addresses, or profile rule matches.
`RemasterFrame` schema 19 needs those fields to display height, normals,
occlusion, emission, and lighting correctly. Filling a `.s9xrmf` with PNG colors
and empty metadata would appear to open in Snes9x, but its diagnostic views
would be meaningless. The linked prototype reconstructs BG metadata from the
reference PPU's winning pixel decisions, not from PNGs, and explicitly marks
sprites/HUD unsupported. The eventual exact producer must capture actual
Snes9x rasterized frames using the same core path as View > Capture Remaster
Frame.

`S9xReadRemasterFrame()` and `S9xWriteRemasterFrame()` already strictly handle
the portable capture format. The macOS `openRemasterFrame:` path can replay a
capture without running a ROM, synchronize matching profile metadata, and use
the existing pixel inspector and GI debug views. The native browser can use
this path; it does not need a second renderer or packet format.

## Producer and catalog

1. Build a versioned scene manifest from the ROM atlas. Include every dungeon
   room ID and overworld quadrant, and list explicit scene variants separately.
   Each row needs domain, stable key, world-grid location or dungeon ID,
   entrance/theme/palette source, state settings, viewport scroll/Link location,
   flags, capture path, and status (`planned`, `captured`, `validated`, or
   `unsupported`) with an explanation. Never count a default preview as a
   validated capture.
2. Add a deterministic headless Snes9x runner using the user-supplied ROM and
   selected profile. Reach each candidate through the game's real room/area
   load path, then let VRAM, CGRAM, OAM, PPU registers, and animation settle.
   Mutating only the room number in WRAM is inadequate: it can leave the prior
   room's graphics and object state. Record the actual room/area, theme,
   palette, progress stage, BG scroll, and relevant state bits at capture time.
3. Request a remaster capture for each viewport and write it under the stable
   scene key. Keep the ROM, game assets, and generated captures out of Git.
   Capture ordinary game lighting, plus a clearly labeled *inspection-lit*
   variant for a lights-out room when the original PPU output hides geometry.
   An inspection-lit capture must still be rendered through Snes9x so all
   ownership and profile metadata remain coherent.
4. Decode every generated packet immediately and reject incomplete ones:
   wrong ROM hash, wrong room/area or scroll, invalid dimensions, no visible
   BG owner coverage in a populated scene, missing instance links, or missing
   expected profile metadata. Compare representative original-color frames
   with live gameplay and the current reference-engine previews. Track
   unrenderable states explicitly; do not silently substitute plain layouts.
5. Add overworld contexts after the dungeon runner passes a small fixture set.
   Cover both worlds, known progress stages, ordinary area heads, and special
   entered/overlay contexts. Display a world-position grid in the browser;
   animations and changed maps are separate manifest variants. The existing
   160-quadrant base atlas is the starting coverage set, not a completed scene
   count.

## Native browser in Snes9x

Add a modeless **Remaster Scene Browser** window in the macOS frontend. It
opens a local manifest directory, groups dungeon and overworld scenes, and
shows thumbnails, flags, capture coverage, and reviewer notes. Filters include
upper-floor clues, layer mode, water, lights out, inferred graphics context,
unsupported states, and review status. Dungeon browsing is by room ID and
viewport; overworld browsing is spatial. Selecting a captured entry calls the
existing `openRemasterFrame:` replay path. Previous/Next move through the
filtered set without dismissing the window. The active capture remains in the
normal Snes9x view, so Original, Height, Normals, Occlusion, Emission, and GI
controls and the pixel inspector keep working. Entries without a capture are
visibly unavailable and explain why. The browser must never show a PNG as if
it were a fully linked `.s9xrmf`.

The browser should read thumbnails and manifest records lazily; loading all
capture packets and the full profile at startup would repeat the former large
profile performance problem. Review notes should be exportable and keyed to
scene variants, rather than only to room IDs. Changing the profile should
refresh the active replay packet using the existing metadata-sync path.

## First acceptance slice

- Generate and validate genuine Snes9x packets for the captured upper-floor
  fixtures `0x60` and `0x61`, one dark room such as `0x00b` in original and
  inspection-lit form, and one ordinary overworld quadrant in two contexts.
- Open all of them through the native browser and confirm that the same
  selected pixel reports the same tile identity, layer, and metadata as a
  manually captured gameplay frame. Height and normal views must render from
  linked packet data, not from a PNG approximation.
- Only then scale out to the dungeon and overworld inventories, recording
  missing contexts and validation failures in the manifest.

The current gallery remains useful for identifying candidate fixtures and
reviewing room art. Its lights-out images now clear the reference engine's
color-math flag solely for preview, while keeping the original ROM flag in the
catalog. That convenience preview is not a substitute for either native
capture variant above.
