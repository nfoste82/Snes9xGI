# Read-only outdoor evidence adapter

`alttp_outdoor_audit.py` reads live ALTPOW1 evidence, **not** the similarly named
historical JSON geometry-candidate format. It does not use dungeon solvers, infer
art, assign heights, write profiles or approve an area. Local ROM-derived outputs
must stay in ignored build directories.

```sh
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_outdoor_evidence.cpp -o build/outdoor-evidence
c++ -std=c++17 -O2 remaster/tools/ALttP/alttp_capture_debug_sources.cpp -o build/outdoor-trace
python3 remaster/tools/ALttP/alttp_outdoor_audit.py \
  --capture CAPTURE.s9xrmf --profile FROZEN_PROFILE.toml \
  --bridge build/outdoor-evidence --trace build/outdoor-trace \
  --output build/outdoor-audit.json --samples build/outdoor-samples.jsonl
python3 -m unittest discover -s remaster/tools/ALttP -p test_alttp_outdoor_audit.py
```

Both exact-named sidecars must accompany the packet. The profile must match the
embedded bytes. `--samples` contains the existing production CPU resolver/field
trace plus independent raster comparisons; all main/sub/scene and per-channel
donors are retained. This is **captured-packet CPU re-evaluation**, not fresh
EndFrame, emulator or GPU replay. Canonical zero arrays remain present data.

Exit 2 and an invalid/unsupported-input JSON mean input verification failed.
Exit 0 means the diagnostic ran, **not** that geometry passed. Always read
`raster_mismatches`, `counts`, `denominators`, `capabilities` and `wall_contract`.
Outputs have sorted keys and no timestamps. Caller controls provenance filenames.

## Supported facts and strict limits

* Strict magic, contiguous bounds, unique sections and whole/per-section FNV;
  all known WRAM field offsets/widths/values independently verified; fixed Map16
  and ROM-definition schema; embedded/current profile equality; packet ROM and
  dimensions. FNV is integrity, not cryptographic association. The existing format
  lacks a packet SHA/frame identifier binding: atomic identity stays unsupported.
* Redundant PPU manifest/register-mirror checks, per-line mode1 8x8 BG addressing,
  SC0..SC3 wrapping, member words, canonical H/V, current VRAM decoded indices,
   canonical hash, opacity and selected/observed main/sub/scene comparisons.
   **These are component comparisons, not captured full-word parity.** Map bit13
   is decoded as visibility priority. The bridge exports serialized `ppuPriority`,
   but current ordinary BG `S9xRemasterSetDraw` calls omit the priority argument
   and serialize its default zero, not bit13. All such priority comparisons are
   explicitly unsupported, even when the raw values happen to agree. `mismatch`
   is null when checked=0; raw-field differences are not confirmed priority errors.
   A semantically attested map-bit producer is supported by the comparison helper,
   not assumed for this bridge or from packet schema. Per-buffer linked denominators
   overlap; unlinked/opaque-unobserved samples are not priority comparisons.
   Packet `vram_address` is exported but not compared: it is a graphics-cache
   address, not a map-entry address, and graphics-address parity is not attested.
  Mosaic size1 is exact; larger mosaic requires uncaptured start phase and is
  explicitly unsupported. Tile-size16 and non-mode1 are unsupported, not guessed.
* Depth0/unlinked does not distinguish disabled, clipped, transparent or unobserved
  raster intervals. Expected-transparent and expected-opaque-unobserved counts
  stay separate. End-frame TM/TS are reported but not retroactively applied as
  raster history. There is no assertion that an opaque unobserved tile is broken.
* Resident 4096-cell Map16 buffer and definition-member candidates are inventoried.
  Reverse-word candidates are not unique placement owners. No WRAM-to-VRAM/world
  origin is inferred, even for an exact raster match. Map32 definitions/streams
  are absent from these inputs. World/offscreen/full-area denominators remain null.
* Asset/occurrence/context presence and actual base/local/CPU field are retained.
  Asset-vs-selected source discrepancies are only candidates: per-facing/per-flip,
  deliberate context masks and fallback provenance need further verification.
  Inherit/Wall is not a physical ownership or numeric height oracle.

## Minimum next outdoor rule provider

First establish a pinned final-state world-to-upload mapping and writer/overlay
ownership, with ROM Map32 streams available separately and verifiable animation
selectors. Then supply independently attested assembly semantics and per-surface
endpoint/rise contracts, not collision-, color-, role- or hash-derived elevation.
Keep feet, tops, corners, adjoining supports, unequal projected face spans,
multiple placement bases and underpasses separate. Historical castle 0/32 is a
conditional candidate only and is **not loaded by this tool**. The fixture-only
`wall_contract` inventory does not discover or solve assemblies. No real assembly
or numeric wall endpoint is currently checked. Missing shape is human authoring;
transport/admission, wrong-base and wrong-local-shape classifications require
independent evidence before repair routing. No agent configuration is needed.

Strict one-assembly binding also requires comparable captured priority and complete
visible ownership of every member and adjoining support, not just a reverse-word
candidate or a >=90% origin match. With the present producer priority loss this
proof is unsupported; do not compensate by selecting a historical candidate base.
