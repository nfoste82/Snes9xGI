# Tile Locations: initial context selection

The Room051 `frame_room051_071.s9xrmf` capture contains two visible occurrences
of `v1:4bpp:fda59404d2049e61`, but neither has a retained context location.
This is not an absent/transparent decoded tile.

Input SHA-256: capture
`e7cb4ff894ae1cd411be05d666bd7f5d1a23c7b4d233c555c92cf942793b6970`;
companion profile
`9ff34398d990e899eec578c02443013f4b0762bb7a1be35c02906de5a1a48134`.

`S9xRemasterEndFrame` located background occurrences, then discarded the records
unless the current profile already declared at least two named contexts for the
identity. The native Tile Locations click handler requires that location record,
so users could not select a normal identity to establish contexts in the first
place. This also affected tiles having just one named context.

The producer now retains all independently located BG occurrences. Binding
resolution and profile serialization still enforce the existing context rules;
no identity-wide or room-specific geometry is assigned. No authored profile was
changed. Legacy packets already missing locations need a fresh live capture.

## Verification

`remaster/tests/context_location_publication_test.cpp` exercises production
EndFrame publication with zero, one and two contexts, then frame serialization,
reopening, visible picking and context selection, all four H/V transforms and
main/subscreen ownership. The fixture-free regression is
included in the full curated gate. An optional capture argument verifies the
reported identity is present and reports its retained location count.

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -O0 -g \
  remaster/tests/context_location_publication_test.cpp \
  -o build/context_location_publication_test
build/context_location_publication_test \
  "$HOME/Library/Application Support/Snes9x/Remaster/frame_room051_071.s9xrmf"
```

Result: production regression passes; supplied capture has two visible doorway
occurrences and zero stored location records. This is a selection repair, not
dungeon-height coverage approval. Fresh interactive click/context/Undo review
is still required.

ASan/UBSan runs also pass, as do existing context binding and rendered-scroll
regressions, six Python runner tests and twenty inspector layout/source checks.
Existing binding, scroll and tile-context regressions also pass under ASan/UBSan.
The arm64 Debug and Release app builds succeed
(`build/room051071-{debug,release}-build.log`). The optimized (`-O2`) location
publication regression also passes. The current authored profile is byte-identical
to the supplied capture's companion profile (SHA-256 pinned above).
Additional standalone animation browsing, authored/effective inspector height,
layer editor and tile-context regressions pass. Assignment followed by profile
Undo is covered by the location-publication regression without losing pick evidence.

An additional broad ALttP Python discovery run is not green: 247 tests, two
failures, four errors and four skips. Errors are absent Pillow and a missing
historical live016 profile backup; failures are diagonal authored-band fixture
expectations and doorframe annotation precedence. Those tools/fixtures were not
changed by this selection fix. The first aggregate C++ gate was invalidated by
concurrent height-source edits, not accepted as a successful validation run.
The next full+Metal invocation passed its first25cases before the outer shell's
120second timeout; it is also not accepted as a complete gate. A final invocation
uses no outer timeout while retaining the runner's per-test timeout.
That final stable run passes **35/35**, including production Metal checks:
`build/room051071-final-tests/20261023T205833Z-q4ehba1j/results.json`.
The opt-in `doorframe_test.cpp` also fails at line 117 in its manually truncated
legacy-v28 packet fixture; its ordinary current-frame roundtrips pass before
that assertion. The fixture was not rewritten as part of this repair.
Separate doorway-placement, tile-semantics, role-only and occlusion-persistence
regressions pass (the last requires a temporary-profile output argument).
