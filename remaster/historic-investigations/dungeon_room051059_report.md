# Room051 own059 direct authored floor-contact walls

**Room051 / pinned own059: reviewable but incomplete.** Direct floor-contact
repair restores 24 wall-foot placement bases and 1,472 actual scene lighting
samples. The 72 outer wall-band placements remain unresolved. No authoring,
profile, arrays, room masks, hash elevation whitelist or commits were changed.

## Exact inputs and corrected diagnosis

Input is `~/Library/Application Support/Snes9x/Remaster/frame_room051_059.s9xrmf`
and its contemporaneous sidecars (the user's `.profile.toml` is the profile
snapshot, not the frame packet). Packet SHA256
`5ab485eda0f838fc4fcaf6a238d9369dc1418b18b275360865394d9693d45937`;
profile `044e25516d1a2ae6384da27a2a331929e18f88ef645eef87005ca83ff0e90f67`,
serialized FNV `eef8e3acedf3300d`; complete ALTPDI1
`53d910819fb9118fd0689a4b59f76bea25a45f0d28ddf307607c911e5b540b7f`.
ROM USA SHA256
`66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb`.
Settled module7/submodule0, Link world(746,2696), plane0,
scroll(626,2588), both graphics bases16384, configured rise50, independently
authored rail relation24. Complete WRAM/PPU maps agree; exact production replay
checks166,816 BG hash/flip/canonical-pixel observations.

The new foot `966069b2157ea21f` really **has effective authored height0..5**
at all24 BG2 uses x19/44, y16..19,22..25,28..31. This is not inactive metadata,
a profile reload failure, or absent authoring. Its displayed floor-facing edge
is all zero; every cross section rises monotonically away from that edge.
H/V placements and effective sources were checked. Contexts and bindings equal
own052; no override is concealing the foot source. Seven asset entries differ
from052: the two newly authored sources plus four pillar height changes and one
occlusion change (exact diff recorded in evidence-summary.json).

The shared structural graph excludes all collision00 wall cells as potential
underpass occluders. The new foot is collision00 and occupies legal upper support
base24, so it is rejected before its exact floor contact is tested. Later wall
projection deliberately clears support-only wall offsets; thus authored relief
appeared at local0..5 rather than effective24..29. This is an actual solving gap,
not a diagnostic coloring failure.

## Reusable repair and limits

`frame.h` adds a **terminal** floor-foot consumer after independent support and
wall solving. It requires explicit Wall with an active local source, reproduced
occupied support on that plane, cardinal explicit zero-local Floor in the same
occupied component, zero contact edge, and monotone nonflat relief on every
transformed cross section. Missing/conflicting graph nodes, persistent/obstacle
ownership and hidden distinct BG1 usages cannot be rescued. In an active overlap,
only BG2 with independently solved strictly higher support than its lower plane
can be its visible foot. Equal/unknown planes remain ambiguous. It adds no
movement edges, datum, wall-stack rise, local ramp or replacement arrays.

Result:24 wall structural bases255→24; lower support remains0 and receives no
wall claim. Production EndFrame plus CPU lighting-field verification checks all
1,472 visible scene-foot samples at base24 plus unchanged local relief0..5.
Complete BG2 solved/unknown walls564/96→588/72; conflicts remain4. BG1 unchanged.

**Why this does not raise the outer shell:** at west x19's outer edge, the
authored samples are `[1,2,3,4,4,5,5,5]` (flipped variants transform this). The
next x18 band's contacting edge is uniformly0. No single placement offset makes
those eight samples agree. A zero-foot contact proves the foot's base, not an
extra assumed8-unit band. Updated pillar relief likewise no longer supplies the
prior exact tangent seams: these inputs cannot reuse own052's 48-band result.
The bands have valid authored0..7 arrays; they are **not missing metadata**.
General capability needed: independently proved assembly/face-region contacts
or explicit seam ownership separating decorative local relief from a structural
datum. Neither proximity nor flattening the foot supplies that evidence. No
user reauthoring was performed or presumed necessary.

## Regression and validation

`dungeon_wall_floor_contact_test.cpp`:160 axes/BGs/HV cases with irregular relief,
missing source, flat/nonzero foot, nonmonotone overwrite, unknown/nonzero-local
neighbor, conflicting floor datum, equal-plane overlap and assembly ownership
negatives. ASan/UBSan passes. Existing144 tangent seam cases, independent BG/plane
ownership suite and effective context/placement/flip receiver tests pass. The
new test is registered in the full portable runner; quick16/16 also passes.
`dungeon_wall_floor_probe.cpp` supplies immutable own-input EndFrame replay,
complete8192 placement/local/base inventories and exact own059 effective field
assertions; the hash check is fixture verification only, never a production rule.
Native arm64 Debug build succeeds; `git diff --check` passes. Parent performs
independent final full/build verification. No fresh interactive gameplay review.

Available corpus:146 Application Support packets,73 with complete sidecars;
72 settled/equal pinned states across7rooms replay through the exact solver.
One room002 capture fails complete WRAM/PPU/state consistency;73 packets lack
complete sidecars. Five Room051 states056..060 each gain24 feet (120claims),
no solved placement losses or conflict changes. The strict production EndFrame
probe additionally accepts22states;51 historical snapshots fail profile
serialization pins (legacy/current serializer mismatch or stale evidence), so
they are not claimed as production-packet proof. The byte-pinned solver replay
accepts their unchanged complete metadata without rewriting it. Twelve existing
build bundles audited:11 accepted, unchanged;061011 rejected by pinned-state
consistency. This is the available corpus, not320rooms/all states. Exact before/
after per-state records and rejection logs are retained in corpus.json,
solver-corpus.json and build-corpus.json.

Mandatory validation v2 ran on complete matching ALTPPD1/ALTPAT1, pinned state,
complete observed-seed static doorway graph, pinned generated profile, exact
runtime solution and ALTPHM3: **exit1, not approved**. Supports886/886 (before
authoring the foot, own052 lacked24 local sources), plane denominators[886,0],
overlap0 for reachable graph, fallback26/26 with direction/ramp/source records;
588solved/72unknown BG2 walls,4runtime conflicts,2compiled missing/2mismatches
at stair rail endpoints. Runtime's independent structural support scope is
[962,3720], distinct from doorway reachability; overlapping upper/lower arrays
remain24/0 at all24 feet. Do not substitute viewport or hash-height coverage for
either denominator. BG1 remains0solved/288unknown walls with419conflict nodes.
All8192 placements audited: BG1 nonempty missing local2, unknown role86;
BG2 nonempty missing local0, unknown role118, empty460 explicitly accounted.
No unknown-role or transparent art counted as solved geometry.

Fixtures with pinned own059 metadata:055 supports439/439 but exit1 (one conflict,
16unknown walls);060 rejects corner face seam disagreement;061 rejects diagonal
source without independent ROM proof. These remain blockers, not passing rooms.

Artifacts under `build/room051059-repair/`: replay manifest/pinned profile,
complete raw/attributes/state/graph, runtime JSON, map, validation-v2 JSON/SVG,
before/after EndFrame packets/inventories, per-BG placement arrays, evidence pins,
corpus records, tests and Debug build log. Generated proposals are review-only.

## Independent final verification

The parent-session full portable plus production Metal gate passes **33/33**,
including the new direct floor-contact regression, on unchanged final sources.
Results: `build/room051059-parent-verification/20261023T160330Z-hb4wt5y5/results.json`.
The arm64 Release build succeeds; product `build/release/Snes9x.app`, log
`/private/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/room051-059-release.log`.
The current user profile and all seven own059 capture inputs remain byte-identical
to the initial baseline. Whitespace checks pass. Independent baseline-only
sanitized full suite passes31/31; this earlier snapshot result is not a claim
that the final full suite was run with sanitizers. The new narrow regression's
ASan/UBSan pass is recorded above. Existing Python runner-contract tests have a
stale hard-coded full+Metal selection count (31 vs baseline32); no production
test was disabled to hide that unrelated failure. Complete-room v2 and fresh
interactive verification remain outstanding as described above.
