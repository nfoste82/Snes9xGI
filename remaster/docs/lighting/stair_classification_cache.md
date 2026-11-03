# Classification retention across stair plane changes

The classification-only cache in `remaster/runtime/remaster.h` keeps up to three
exact artwork states independently of the solved-artwork cache. A plane/support
change still invalidates solved maps and runs the existing solves. It no longer
forces recurring animation phases to repeat full room classification when all
classification inputs exactly match a retained entry.

Entries retain both complete placement arrays, maps/attributes, referenced VRAM
evidence, graphics bases, templates and room/profile/ROM/session/research context.
They do not retain height-map solutions or entry datums. Profile publication,
including unchanged resume, and continuity/identity boundaries revoke entries.
Research producer-driven classification remains ineligible without exact ordered
provenance. The three entries require approximately5.92MiB on arm64; eviction
occurs before allocation. Unchanged frames do not consult this cache.

Controlled own-input room02f alternating-artwork/plane replay, two cold plus18
warm frames: classification calls36→0, solves54→54, median EndFrame98.05→28.61ms.
All20 complete placement/map/serialized-packet inventories match separate baseline
and forced-uncached processes. Six additional own-input captures/24 plane-changing
frames also match forced-uncached output. These are controlled replay results,
not a measured live-gameplay speedup or a complete-room geometry approval.

The remaining solve/assembly cost is above the frame budget. Initial artwork
states still classify, and animation sets exceeding capacity can evict entries.
The preexisting visible-face allocation-budget failure from inline terrain data
is unchanged and is not masked by this repair.

Reproduction, pins, test coverage and limits:
`build/dungeon-classification-artwork/REPORT.md` (local evidence).
Regression: `remaster/tests/dungeon_classification_artwork_test.cpp`.
Fresh live stair capture is required to establish actual pacing improvement.
