# Continuous diagnostic frame recordings

**View > Remaster Recording… (Command–Shift–R)** opens the native recorder and
timeline. Load the matching profile and start a game, then **Start Recording**.
It records completed rendered gameplay frames across room/world boundaries until
**Stop Recording**. Pausing automatically suspends frame recording; paused redraws,
layer-refresh steps and frame advance do not append gameplay frames. Starting while
paused is allowed and waits for resumed gameplay. No emulation is advanced to record.

Output is a uniquely named `recording_<UUID>.s9xrmr` under
`~/Library/Application Support/Snes9x/Remaster/`. The panel displays the path,
completion/error state and limits. Stop drains the worker asynchronously. An error
stops recording rather than silently dropping frames; an unfinished/truncated final
record can be opened with an explicit recovered-prefix warning. Flushes are not a
power-loss durability guarantee.

## Inspect and edit

Pause emulation, stop recording and choose **Open Recording…** in the panel. A
matching **current editing profile must already be loaded**. Opening never installs
or overwrites a recorded profile. The panel displays the editing path. Use the
timeline slider or **◀ Frame / Frame ▶**, existing GI/debug views and tile clicks.
Resuming releases timeline inspection and returns to live emulation.

The default is **CURRENT PROFILE PREVIEW**. Existing capture-preview functionality
updates local authored layers, profile matches/materials, context bindings and Area
Settings. It does **not** rerun the complete room geometry/transition solve. Recorded
placement bases, occurrence overrides, ownership, visible graph statuses and captured
emitter positions can be stale relative to new metadata; the panel explicitly labels
them as recorded/not re-solved. Saving changes the current profile, never the recording.

Check **Recorded Evidence** for the original immutable runtime interpretation. The
inspector labels the mode and reports original placement base/geometry/visual status
separately. Editing still targets the named current profile; uncheck Recorded Evidence
to see its preview. The frozen original is independently retained and never passed as
the mutable editor packet. **Export Original Frame + Evidence…** writes an ordinary
compatible `.s9xrmf`, its exact recorded `.profile.toml` and `.rte1` runtime evidence
companion. RTE1 is not the legacy dungeon-sidecar format; do not pass it to existing
ALTPDI1 tools or pair it with live RAM. Full-room solve-history presentation is not
implemented; the native debug views consume the recorded per-pixel diagnostics.

## Stored evidence and format

Version1 magic is `S9XRMR\0\1`. Explicit little-endian tagged length-delimited records:
1 compressed dictionary block, 2 completed frame, 3 final completion/count,
4 profile/settings revision event. Per-frame references retain authored/decoded tile
records once per exact identity/content revision, including metadata arrays, materials,
groups and selected context geometry. Profile text revisions are diagnostic dependencies,
not a selectable/installed recorded-profile workflow. Frames bind their exact active
profile/settings revision; unsaved runtime publication is retained too. Saved profiles,
Area Settings edits and opening the settings panel emit separate revision events even
while paused. All area-light and area-scene maps are stored, not a guessed UI subset:
directional enabled/vector/color/brightness, original RGB contribution, emitter count
and inheritance/default context. Effective blended light values remain in each packet.

Dynamic frame color, main/sub/scene ownership, observed BG/depth, placements, flips,
selected context, effective offsets, occurrence solve/visual status, lights and hidden
emitters use the existing v40 packet serializer. Runtime-only room/plane diagnostics
and selected context layers are stored separately. No `.s9xrmf` format changes.

RTE1 records exact completed-boundary context/module/submodule/previous/current room,
entry/session/epoch, per-BG scroll, available final maps/attributes/VRAM/templates,
actor/OAM inputs, retained outgoing maps, applied/current-BG1/previous map bases,
occupied owners, structural/mixed/fallback outputs and available plane diagnostics.
It does not use sprite-support eligibility or the legacy writer's later session/RAM.
Optional overworld input evidence uses the existing value-owned adapter, including
raw WRAM/VRAM/register mirrors, Map16 definitions, palette/OAM and renderer line scroll.
These are completed snapshots, **not** a raster-write history, deterministic emulator
re-emulation, authoritative historic ROM-writer provenance or whole-room approval.
Room-name/area fields remain context labels, not proof that every visible pixel belongs
to that room; outgoing/incoming packet pixels and retained map identities are preserved.

Compression is bounded PackBits-style literal/run encoding over 16KiB chunks.
Dictionary lookups verify bytes after hashing (FNV is not an authenticity signature).
Authored data always interns; dynamic chunks intern until the dynamic half-budget is
used, then use inline compressed blocks. Thus the format is **not** a directory of
complete captures appended every frame. Disk rate is scene-dependent: busy frames
may remain several MiB/frame. The first iteration makes no long-duration/60fps budget
claim; queue saturation stops with an explicit error.

Bounds: 2GiB file, 36,000 displayed frames, 36,000 revision events, 32MiB packet/blob,
four queued packets (128MiB accounted queue), 64MiB dictionary and 262,144 dictionary
blocks. Reader indexes offsets rather than retaining the recording in memory, checks
record/dictionary/dimension/table/expansion bounds, then lazily decodes selected frames.
Current embedded packets only; historic standalone captures retain their old reader.
Runtime production inputs are trusted in-process; malformed serialized inputs fail
without replacing the displayed frame. Profile copies are retained at publication
boundaries, not on every frame. Recording does not initiate extra room solves.

Tests: `remaster/tests/frame_recording_test.cpp`, curated `--suite full`. Native
arm64 Debug compilation is checked. Interactive menu, timing, transition traversal,
scrub/click/edit/save and real-recording disk-rate review remain manual checks.

Deferred: pretrigger rolling buffer, complete recorded/current/other profile selector,
full offline runtime re-solve, raster-write history, richer whole-room evidence UI,
portable external RTE1 analysis adapters and adaptive disk-rate optimization.
