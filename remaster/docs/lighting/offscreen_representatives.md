# Independent offscreen representative emitters

Profile v36 adds `offscreen_representative_emitters` in `lighting_space` and
`area_scene_settings.room_NNN` / shared `area_scene_settings.overworld`.

* Profile omitted: use **1**, independently of visible representative quality.
  Older profiles therefore load with offscreen quality1. The former profile-level
  `-1` value is accepted for compatibility but also resolves to1.
* Area omitted / `-1`: inherit the profile offscreen setting. The two overworld
  identities share settings.
* Explicit `0`: exact individual hidden source pixels, **not disabled lighting**.
* Explicit `1..8`: existing representative approximation at this independent quality.

Profile Controls exposes an approximation checkbox and numeric field/slider;
there is no visible-quality inheritance checkbox. Area Settings adds the controls
with **Use profile default**. In both windows, the offscreen block sits directly
below the visible Representative Emitters block. Unchecking approximation stores explicit zero; inherited
controls are disabled. Settings support save merging, Undo and live/paused-live
publication through the existing profile/frame path. Area Undo changes only this
field and preserves unrelated settings. Returning a newly created default-only
area record to inheritance removes it; intentional baseline records are retained.
Profile-wide lighting keys merge independently, but each area's scene settings
are a **whole-section merge record**: concurrent edits to different fields of the
same area conflict rather than merging fieldwise. Different areas can merge.
The counters now label visible emitting
pixels explicitly; direct samples include visible and hidden sources.

"Offscreen" uses the existing hidden-source channel, **including HUD-covered
emission**. No provenance, compartment, bucket or combine rule is changed here.
An explicit quality override may change lighting; it is not image equivalence.
Different visible/hidden qualities can also change the approximation as a torch
crosses the viewport or HUD boundary. No temporal blend is added here; inspect
moving-camera and near-field shadows before choosing a lower hidden default.
The visible exact source path remains independent, including visible `0` + hidden
`1..8`. GPU normalization consumes hidden representative area even when visible
sources use the exact pipeline. Hidden representative depth adaptation likewise
uses its sentinel identity independently of visible clustering. Serial and batched
exact-visible pipelines receive the same adaptive-depth specialization as the
clustered pipelines (`S9X_REMASTER_FULL_DEPTH_SAMPLES` disables it). Exact visible
samples retain their original four-stratum semantics. Visibility/diagnostic modes
retain exact sampling.

Captures optionally append **OER1**: four-byte tag + one strict effective count
byte `0..8` (appendix-capable frame schemas v28+). Live/profile-applied packets pin the effective count. Old packets
without the appendix inherit their captured visible count; synthetic packets can
retain that behavior with the in-memory `-1` default. Duplicate, truncated and
out-of-range appendices fail. Older readers reject the new appendix. Authored
inheritance stays in the profile, not in the effective capture record.

Regression: `remaster/tests/offscreen_representatives_test.cpp` exhaustively checks
9×10×10×10 inheritance combinations, valid zero, strict profile/capture parsing,
old capture fallback, scene snapshots, actual EndFrame publication and merge.
It includes a historical v28 byte-layout fixture, default-only section cleanup
through the production edit/Undo helper, unrelated valid-zero preservation, and
same-area/different-field merge conflicts.
`macosx/remaster-lighting-test.mm` checks production Metal transport with hidden
representatives and exact visible pipeline, including batched area normalization.
`macosx/remaster-profile-controls-layout-test.py` checks controls and independent
gating. The portable regression is included in the curated full gate.

The default value1 may be omitted when saving, because omission now means1;
nondefault counts are saved explicitly. Saved frame captures without OER1 still
retain their historical visible-count fallback; loading an old **profile** is
different from replaying an old immutable **capture**.

No profile artwork, geometry or canonical profile is modified. Native interactive
UI, save/Undo and paused preview smoke testing still require a user session; no
emulator launch/restart or profile save is part of this implementation validation.

## Independent default1 validation (2026-10-28)

The updated profile-default regression passes, including old-profile omission,
9,000 resolution combinations, exact zeros, serialization, publication and merge.
The same regression passes ASan/UBSan; source-layout checks pass11/11. The arm64
Release build passes, and the quick integration gate passes16/16, manifest
`build/remaster-tests/20261028T231458Z-sti8hj9z/results.json`. The full/Metal
results below predate this default change and are not a rerun of those semantics.

## Historical implementation validation (2026-10-28)

The results below predate the independent profile default of1. References to
inheriting visible quality describe the initial implementation, not current
profile-loading behavior. Old immutable captures still retain that fallback.

Startup HEAD `32a59dc69c958e65ce11a8f6f6b26d9a6e2bdba0` plus preexisting dirty
visible-face CPU/profile work. New regression baseline failed compilation because
the independent setting/API did not exist. After implementation, exhaustive
inheritance and 64-source witness pass (visible0/hidden1: one representative;
visible0/hidden0:64 exact records; inherited visible2:two representatives).
Production Metal on Apple M3 Max passed1400/1400 checks, including independent
exact-visible hidden-area normalization, both direct and batched paths.

ASan/UBSan passed new offscreen-quality, area-scene, offscreen-continuity,
retired-emitter-patch, emitter-patches, held-lamp and link-lamp-brightness
regressions. An extra run of the old `offscreen_emitters_test.cpp` failed its
"EndFrame appends held lamp before publication" assertion; the failure is retained,
not an acceptance pass or a changed oracle. Profile Controls source-layout checks
passed10/10. Debug arm64 app built with signing disabled. Full curated integration
gate is owned by the parent after the shared source freeze; not claimed here.

No room/scene corpus approval or measured live performance improvement is claimed.
The initial visible-inherited execution preserved the old quality. The current
profile default1 can lower hidden quality relative to an older profile's visible
count; that is an intentional quality/performance change, not image equivalence.

The precision-sensitive hidden adaptive-depth comparison additionally has a
baseline-failing RGBA32Float witness: the archived first implementation fails
four far-depth serial/batched cases when adaptation is enabled, while the
correction passes with adaptation on and off. Artifacts and source pins are in
`build/remaster-tests/offscreen-depth-sensitivity/`; the verified run log is
`build/remaster-tests/offscreen-depth-sensitivity-verified-run.log`. Half-float
readback alone rounded away that discrepancy and was not a sensitive oracle.
The tracked depth witness now uses RGBA32Float while other production-format
checks remain RGBA16Float. Its recompiled final harness passed1406/1406 with
adaptive depth disabled and enabled. Before this test-only promotion, the
stable production source passed the curated full+Metal gate79/79, manifest
`build/remaster-tests/20261028T221200Z-zt3i94f4/results.json`. Final Release and
Debug arm64 builds passed; no fresh native interaction or transition acceptance
is implied.
