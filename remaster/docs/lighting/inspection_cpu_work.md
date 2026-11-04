# Inspection CPU work and packet reuse

Ordinary running profile inspection does not require projected height geometry
or lighting sources. `requestedInspectableFrames` retains fresh stock ownership,
profile interpretation, context locations and the existing room-solve/continuity
lifecycle, but no longer requests height projection, sprite support publication,
visible-face cloning or offscreen lights by itself.

Full geometry is still required by live lighting, recording, frame captures,
explicit completed-frame output, room diagnostics and producer-side paused
refreshes (`requestedFullGeometry`). Paused capture/picking already runs a fresh
producer frame. A paused lighting redraw also refreshes if the last packet was
lightweight. No GUI lighting setting is changed by this policy.

`RemasterFrame::fullGeometry` is an ephemeral capability, not serialized profile
data. Lightweight packets are rejected by capture serialization and lighting
draws rather than treating absent projection as valid zero geometry. Files read
from disk retain the normal full-capture contract.

Finalization reuses only exclusively producer-owned raster and instance-vector
capacity from the previous completed packet. It constructs fresh optional
metadata every time; no old contexts, claims, lights or geometry survive. The
Metal upload boundary still copies data before asynchronous presentation; no
borrowed CPU packet storage is shared with completion callbacks. Stock
presentation policy is unchanged.

Regression: `remaster/tests/frame_consumer_work_test.cpp` covers off/on output,
maintained collision invalidation, paused/diagnostic/capture demand, lightweight
serialization rejection, retained value copies, and fresh/reused packet parity.

Initial controlled capture030 measurements (own profile, O2, fixed-input
EndFrame only; not gameplay FPS) reduced lighting-off inspection from roughly
11–12 ms to 4–5 ms. Full-frame allocation requests fell from 31.9 MB to 4.8 MB
after warm-up; requested bytes are not retained memory or memory bandwidth.
See `build/steady-state-performance/REPORT.md` and its implementation artifacts
for source/input pins, individual timings, validation status and remaining work.

Do not gate room solving, transition observations, support recording or input
invalidation on this presentation demand. Those preserve correct reactivation
and remain independent of lighting. This change does not resolve cold solves or
promise 60 FPS; PPU assembly and other per-frame work remain.
