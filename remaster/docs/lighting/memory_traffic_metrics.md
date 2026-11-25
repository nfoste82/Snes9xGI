# CPU memory traffic diagnostics

Opt in with `S9X_REMASTER_MEMORY_METRICS=1` while collecting a legacy or bounded
per-frame trace. Normal builds do not call task_info or accumulate traffic when
the switch is absent. The JSONL `memory_metrics` object accompanies the ordinary
exclusive CPU stage times and upload counters.

`logical-major-buffers-v1` counts explicit major buffer scans/copies in bytes,
not DRAM transactions. A memcmp counts both operands at full length (an upper
bound when it rejects early). Random object/map traversals, allocator headers,
cache lines, prefetch, instruction fetch and stack traffic are not complete.
Do not label these values measured memory bandwidth, or zero as no memory access.
The current coverage is incomplete; missing stage instrumentation is an evidence
gap. Keep the same accounting definitions when comparing builds.

`phys_footprint_bytes` is a task_vm_info snapshot at the producing EndFrame boundary
when opt-in metrics are enabled. Offline packet-only frames may have zero because
there is no EndFrame producer; zero means unavailable, not zero residency. Metal driver residency and process footprint
are distinct from logically accounted CPU/per-ray cache storage. Cache totals
exclude allocator overhead and must not be added to footprint.

Explicit allocation events are not a malloc interceptor. Until all allocation
sites are instrumented, zero events is not evidence of no allocations or frees.
The output describes its method rather than presenting malloc churn as measured.

Per-ray read/write estimates are lower bounds, 16 bytes per recorded reused or
traced pair and 16 bytes per recorded traced pair respectively. Diagnostic counts
must be enabled separately; counter-free timing traces cannot recover pair traffic.
Version/segment scans, receiver/sample data, table probes and GPU cache effects
are excluded. `gpu_upload_bytes` reuses the existing renderer's accounted memcpy
uploads and is not all Metal traffic.

Hardware validation should use an available xctrace CPU Counters template with
L1D/L2 misses, load/store instructions and bandwidth. Record availability and
privilege failures; do not substitute logical counts for hardware counters.
