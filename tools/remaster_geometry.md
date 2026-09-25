# Capture-driven character geometry

`remaster_geometry.cpp` generates **suggestions** for per-pixel character height
and normal layers from captured, decoded SNES tiles. It uses the project's own
frame reader, profile parser, and profile serializer, so tile hashes and the
output format stay compatible with the emulator.

Build from the repository root:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -pedantic tools/remaster_geometry.cpp \
  -o /tmp/remaster-geometry
```

For Link, capture a frame with a well-authored pose and captures of other poses,
then run:

```sh
/tmp/remaster-geometry \
  --profile alttp-profile.toml \
  --capture "/path/to/authored-pose.s9xrmf" \
  --capture "/path/to/other-poses.s9xrmf" \
  --group link_sprite --palette 7 --replace-flat \
  --output /tmp/alttp-link-candidate.toml \
  --preview /tmp/alttp-link-preview.ppm
```

The tool takes any number of `--capture` arguments. Capture while the relevant
animation plays. The current capture feature records 120 rendered frames, but
its representative image includes only the first frame; its decoded asset list
includes the variants seen during the full interval. Capture each direction,
movement speed, and attack separately when needed. The optional `--palette`
includes newly seen object tiles of that palette in the named profile group
*only when a suggestion is generated*. Palette numbers are capture-specific;
check the printed tile IDs and preview before using a profile made this way.

The first preview column is tile artwork colored from the matching palette's
captured pixels where available, the second is height, and the third is encoded
XYZ normals. Colors that were not captured use arbitrary fallback colors. Rows
follow the printed tile ID order. An image viewer that does not support PPM can convert it with
`magick /tmp/alttp-link-preview.ppm /tmp/alttp-link-preview.png`.

Existing detailed layers are kept. `--replace-flat` replaces uniform normals
on opaque pixels, while preserving existing heights and other metadata. Strong
tile matches transfer surface directions from the authored pose, mirroring X
when the artwork matches mirrored. Narrow fragments with existing height use a
camera-facing blade normal. Weak matches remain untouched. New tiles that
match a reference receive both height and normals. No height is guessed for a
weak match or an unseen tile. The output is a separate, parser-validated profile;
load it in Snes9x or copy it over the original after reviewing the result.

This is a capture-driven aid, not an extractor for the entire game's maps or
sprite library. A static sprite sheet cannot be reliably mapped to the runtime
hashes without reconstructing the game's palette and tile decoding. Surface
direction also cannot be read uniquely from 2D pixel art: the transferred
normals are a starting point for in-game lighting review and manual correction.
