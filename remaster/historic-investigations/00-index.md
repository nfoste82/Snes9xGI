# Historic Investigations

Table of contents for room/capture-specific investigations and older repair,
audit, editor and performance reports. Read only the report relevant to the
current symptom or regression. Return to the [primary documentation index](../../AIDocs/00-index.md)
for current feature guides and workflows.

These reports preserve historical findings, input pins, reproduction commands
and validation limits. **This is not a current backlog or a list of resolved
issues.** A reported defect may have been repaired later; an archived repair may
still have independent open coverage gaps. Revalidate against current code and
pinned inputs before treating any historical count or diagnosis as current.
Commands use repository-root paths unless explicitly stated otherwise. Generated
evidence under `build/` and local capture directories has not moved.

Current priority (owner confirmed 2026-10-26): correct heights for every dungeon
room and overworld area. The 5 ms indirect-lighting target remains deferred.

## Room001, Room002 and Room012

- [Room001 detection investigation and Room061 regression](room001_detection_investigation_report.md)
- [Room001 plane1 source/seam detection](room001_source_seam_detection_report.md)
- [Room002 upper walls and Esc/resume lifecycle](dungeon_room002_resume_report.md)
- [Room002/012 wall follow-up](dungeon_walls002012_report.md)

## Room050

- [Settled-state structural evidence gaps](dungeon_room050_report.md)
- [Statue assembly repair](dungeon_statue_assembly_report.md)
- [Tall wall-mounted assemblies, capture007](dungeon_tall_wall_mount_report.md)
- [Offscreen emitter floor-base audit](offscreen_emitter_bases_report.md)
- [Torch animation metadata](torch_animation_metadata_report.md)

## Room051

- [Platform border and throne floor support](dungeon_room051_obstacles_report.md)
- [Capture035 floor continuity](dungeon_floor_continuity_report.md)
- [Capture039 stairs and side-contact pillars](dungeon_room051039_report.md)
- [Capture049 runtime omissions](dungeon_room051049_report.md)
- [Capture052 authored walls and stair receivers](dungeon_room051052_report.md)
- [Capture059 direct authored floor-contact walls](dungeon_room051059_report.md)
- [Capture061 east/west wall stacks](dungeon_room051061_report.md)
- [Capture071 original runtime investigation](dungeon_room051071_runtime_report.md): Wall and stair diagnoses precede the following repairs; not a current failure inventory.
- [Capture071 corner cap-region repair](dungeon_corner_cap_region_report.md): Repairs the original BG1 structural contradiction.
- [Complete stair assembly attachment](dungeon_stair_attachment_report.md): Follow-up to missing stair placement support; preserves authored ramps.
- [Lower-wall authored metadata versus production fields](dungeon_room051_authored_fields_report.md)
- [Capture092 effective geometry](room051092_geometry_report.md)
- [Captures098–103 geometry and follower investigation](room051_098_103_geometry_report.md): The reported crossing was subsequently reproduced with capture104.
- [Capture104 follower crossing repair](room051104_follower_report.md)
- [Capture107 west-facing curtain wall support](room051107_wall_support_report.md)

## Room060, Room061, Room062 and Room071

- [Room060 capture058 obstacle support](dungeon_obstacle060058_report.md)
- [Room060 capture079 corner attachment](dungeon_corner079_report.md)
- [Room060 capture092 performance repair](room060092_performance_report.md)
- [Room060/071 runtime-height follow-up](dungeon_runtime_height_bugs_report.md)
- [Room061 capture057 columns](dungeon_column057_report.md)
- [Room061 capture066 per-member obstacle policy](dungeon_policy_own066_report.md)
- [Room061 capture010 doorway band propagation](room061_doorway_propagation_report.md)
- [Room061 detection follow-up and capture014 correction](room061_detection_followup_report.md): Newer terminal-source evidence supersedes earlier conditional diagonal passes.
- [Room061/062 action-state heights](dungeon_action_height_report.md)
- [Room061/060 stair lighting follow-up](dungeon_stair_light_report.md)
- [Room071 captures001/007/008 static stair repairs](dungeon_stair071_static_report.md)
- [Room071 wall projection follow-up](dungeon_wall_projection071_report.md)

## Shared geometry, doorways and lifecycle

- [Directional doorway support implementation](doorway_support_report.md)
- [Doorway detection audit, rooms055/060/061](doorway_detection_report.md): Room061 reporting and diagonal evidence were corrected in its follow-up above.
- [Door opening floor datum repair](door_opening_height_report.md)
- [Global authored obstacle footprints](dungeon_authored_obstacle_report.md)
- [Outgoing room geometry retention](dungeon_departure_geometry_report.md)
- [Transition height timing](dungeon_transition_timing_report.md)
- [Straight inter-room stair transitions](dungeon_straight_stair_transition_report.md)
- [Teleport-door outgoing geometry](dungeon_teleport_door_transition_report.md)
- [Dungeon dialog height lifecycle](dungeon_dialog_height_report.md)

## Movable sprite obstacles and editor investigations

- [Original pinned Room051 movable-obstacle audit](sprite_obstacle_report.md): Several original authoring/producer gaps have later follow-ups below.
- [Progressive movable-obstacle authoring](sprite_obstacle_authoring_report.md)
- [Scrolling actor/OAM ownership](sprite_obstacle_movement_report.md)
- [Mostly clipped producer culling](sprite_obstacle_culling_report.md)
- [Paused obstacle support and member Z](paused_obstacle_report.md)
- [Obstacle grouping and exact occurrence transfer](obstacle_grouping_ui_report.md)
- [Shared obstacle identity and occurrence focus](obstacle_shared_identity_report.md)
- [Selected-occurrence orientation and supporting end](obstacle_orientation_report.md)
- [Support Contact checkbox repair](obstacle_contact_checkbox_report.md)
- [Bulk Support Contact editor repair](support_contact_editor_report.md)
- [Initial Tile Locations selection/publication](context_location_selection_report.md)
- [Door-art occlusion persistence and limited recovery](profile_occlusion_recovery_report.md)

## Rendering and performance investigations

- [Outdoor transparent-pixel backdrop ground](backdrop_ground_report.md)
- [Classification reuse at plane-only solve boundaries](dungeon_classification_plane_report.md)
- [Geometry-equivalent artwork cache](dungeon_geometry_cache_report.md)
- [Incremental lighting field preparation](lighting_field_cache_report.md)
