# Hybrid Direct (opt-in)

Profile Controls provides **Hybrid Direct (off = exact)**, **Exact cutoff (% of
receiver’s unoccluded weight)** (0.5–10%, slider steps 0.5%), and **Rays for weaker
lights** (1–32). Initial defaults are 2% and 8 rays. Settings apply live through
the existing scene-settings transaction, with Undo/Redo and Save Profile.
Off preserves exhaustive Direct. Existing profiles and captures remain off.

Profile schema41 stores `hybrid_direct`, `hybrid_direct_cutoff` (fraction, not
percent), and `hybrid_direct_tail_samples` in `[lighting_space]`. Frame schema44
records the effective enable byte, cutoff float and tail-count byte. Older frame
versions default off. These controls do not change representative emitters,
artwork, geometry, or indirect connections.

For each receiver, three scans retain the expanded source records and adaptive
depth/sphere quadrature used by exhaustive Direct. First compute total unoccluded
weight and exact normalization. Trace every record whose weight reaches the
cutoff fraction, then stratify samples over the remaining tail CDF. Repeated
selections share a visibility query with multiplicity. Stable pixel-hashed jitter
means unchanged inputs have unchanged output; it is not temporal denoising.
The estimator targets exhaustive transport in expectation, subject to float/half
rounding. Individual frames are approximate and may show shadow noise or popping.
The prototype's strict convergence gate did not pass on every real fixture;
this integration is not visual acceptance or a correctness/quality promotion.

`S9X_REMASTER_HYBRID_DIRECT=0/1`, `S9X_REMASTER_HYBRID_TAU`, and
`S9X_REMASTER_HYBRID_TAIL_SAMPLES` override profile/capture values for tests.
The cutoff override accepts0..1 and rays1..4096 for research compatibility.
`S9X_REMASTER_EXHAUSTIVE_DIRECT` takes precedence. The historical
`S9X_REMASTER_DIRECT_SAMPLES` experiment, Visibility, GI diagnostics and debug
sphere retain their existing precedence. Diagnostic seed/radius/count switches
remain compatible with the private prototype; animated seeds/count readback
disable Direct-result reuse.

`RemasterHybridDirectSettings` groups all persisted settings. Effective settings
resolve once per frame in `RemasterEffectiveHybridSettings`, before source
preparation and completion-gated result-key construction. A future room override
layer belongs between the packet settings and environment overrides there; no
per-room hybrid overrides are implemented now. Enabled settings and seed enter
the exact Direct-result key. Turning off changes back to the unchanged exhaustive
key; settings changes cannot reuse an incompatible Direct result. Live scene
publication also advances the normal settings/frame invalidation boundary.

Field selection/update uses eight synchronous workers for full or broadly dirty
frames; small changes stay serial. All dependency/material/HUD tables are prepared
before dispatch, outputs are disjoint pixel ranges, and counters are per-worker.
No translation/overlap reuse is used. FieldInputs/FieldPixels/FieldSnapshot elapsed
sub-stages preserve exclusive trace accounting; worker code does not write trace
state. Unchanged donor geometry/claims are retained during emission-only refresh.
