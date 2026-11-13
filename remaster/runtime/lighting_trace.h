#ifndef REMASTER_LIGHTING_TRACE_H
#define REMASTER_LIGHTING_TRACE_H

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ostream>
#ifdef __APPLE__
#include <mach/mach.h>
#endif

// Set to 0 for shipping/performance-baseline builds. No timer scopes or capture
// activation remain active; keep the value consistent across translation units.
#ifndef S9X_REMASTER_PROFILING
#define S9X_REMASTER_PROFILING 1
#endif

// Opt-in, producing-frame lineage. CPU intervals are elapsed service spans, not
// scheduler thread-CPU time. Exclusive accounting removes nested work exactly
// once; waits have their own stages and are not lighting service. No GPU wait is
// included in CPU service. GPU timestamps use the host uptime clock on macOS.
namespace RemasterLightingTrace {
enum Stage { EndFrame, Finalize, Room, Classification, Solve, Apply, Visible,
    Actors, Offscreen, Fields, Selection, Mesh, Setup, Upload, Scan, Spheres,
    Hidden, Cluster, Sources, Encode, LockWait, SlotWait, Recognition,
    Scene, Owners, Highlights, Normals, MeshCopy, ContextLocations, ContextBindings,
    Terrain, DoorRecognition, FieldInputs, FieldPixels, FieldSnapshot,
    VisibleMap, VisibleTemplates, VisibleReuse, VisibleSlopes, VisibleExposure,
    VisibleGraph, VisiblePublish, VisibleClone, FinalizePixels, FinalizeMetadata,
    ApplyInputs, ApplyLegacy, ApplyPublish, OffscreenIdentity, OffscreenAdmission,
    OffscreenPublish, FieldMetadata, FieldInstances, Count };
static constexpr const char *names[] = {"endframe", "finalize", "room", "classification",
    "solve", "apply", "visible", "actors", "offscreen", "fields", "selection", "mesh",
    "setup", "upload", "scan", "spheres", "hidden", "cluster", "sources", "encode",
    "lock_wait", "slot_wait", "recognition", "scene_hud", "owners", "highlights", "normals", "mesh_copy",
    "context_locations", "context_bindings", "terrain", "door_recognition", "field_inputs", "field_pixels", "field_snapshot",
    "visible_map", "visible_templates", "visible_reuse", "visible_slopes", "visible_exposure",
    "visible_graph", "visible_publish", "visible_clone", "finalize_pixels", "finalize_metadata",
    "apply_inputs", "apply_legacy", "apply_publish", "offscreen_identity", "offscreen_admission",
    "offscreen_publish", "field_metadata", "field_instances"};
enum RoomReason : uint64_t {
    RoomChange=1ull<<0, SnapshotInvalid=1ull<<1, ProfileChange=1ull<<2,
    ProducerChange=1ull<<3, RomChange=1ull<<4, EntryChange=1ull<<5,
    PlaneChange=1ull<<6, CollisionChange=1ull<<7, MapChange=1ull<<8,
    AttributeChange=1ull<<9, GraphicsChange=1ull<<10, TemplateChange=1ull<<11,
    StairChange=1ull<<12, GraphicsBaseChange=1ull<<13, MissingClassification=1ull<<14,
    ResearchClassification=1ull<<15, BackgroundChange=1ull<<16,
    VerifiedChange=1ull<<17, Unpublishable=1ull<<18,
    ReplacementEvidenceChange=1ull<<19, GraphicsDependenciesChange=1ull<<20,
    MovementSeedChange=1ull<<21
};
inline double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline bool legacyEnabled() { return S9X_REMASTER_PROFILING && std::getenv("S9X_REMASTER_LIGHTING_TRACE") != nullptr; }
inline std::atomic<bool> &boundedActive() { static std::atomic<bool> a{false}; return a; }
inline bool enabled() { return S9X_REMASTER_PROFILING && (legacyEnabled() || boundedActive().load(std::memory_order_relaxed)); }
inline uint64_t nextId() { static std::atomic<uint64_t> id{0}; return ++id; }
struct Frame {
    uint64_t id=0;
    double begin=0, producerEnd=0;
    std::array<double, Count> exclusive{};
    std::array<uint64_t, Count> calls{};
    std::array<uint64_t, Count> bytesRead{},bytesWritten{},allocationCount{},allocationBytes{},freeCount{};
    uint64_t cpuCacheBytes=0,footprintBytes=0;
    uint64_t epochBefore=0, epochAfter=0, inputKey=0, profileKey=0;
    uint64_t selected=0, updated=0, resolved=0, uploadBytes=0, allocations=0;
    uint64_t projectionDecisions=0;
    uint64_t candidateHits=0,candidateBuilds=0;
    uint64_t fieldReasons=0, highlightCount=0, hiddenPixels=0, visibleFaces=0;
    uint64_t restores=0, dependencyReuses=0;
    uint64_t roomReasons=0, classificationReasons=0;
    uint32_t classificationOutcome=0, projectionOutcome=0, instanceCount=0, instanceBytes=0;
    uint32_t highlightPath=0, producerFlags=0;
    uint32_t width=0,height=0, spheres=0,samples=0,patches=0;
    uint32_t bounces=0,connections=0,representatives=0;
    bool directional=false;
    bool hybridEnabled=false, hybridEnvOverrides=false;
    float hybridCutoff=0;
    uint32_t hybridTailSamples=0;
    const char *directReuse="miss", *directReuseReason="not-encoded";
    const char *perRayOutcome="bypass";
    uint64_t perRayBytes=0, perRayTraced=0, perRayReused=0;
    uint64_t roomRayChunkHits=0,roomRayChunksRetained=0,roomRayChunksReturning=0;
    uint64_t roomRayAllocatedBytes=0;
    uint64_t perRayReceiver=0, perRaySample=0, perRayAged=0, perRaySegment=0;
    uint32_t room=0xffff, plane=0, submodule=0, collision=0;
    int scrollX=0, scrollY=0;
};
struct Scope;
// Function-local storage also supports the app's C++14 build configuration.
inline Scope *&activeScope() { static thread_local Scope *value=nullptr; return value; }
struct Scope {
    Frame *frame;
    Stage stage;
    Scope *parent=nullptr;
    double start=0, children=0;
    Scope(Frame *f, Stage s):frame(S9X_REMASTER_PROFILING && f && f->id ? f : nullptr),stage(s) {
        if(frame) { parent=activeScope(); activeScope()=this; start=now(); }
    }
    ~Scope() { stop(); }
    void stop() {
        if(!frame)return;
        const double elapsed=now()-start;
        frame->exclusive[stage]+=(elapsed-children)*1000;
        frame->calls[stage]++;
        if(parent && parent->frame==frame)parent->children+=elapsed;
        activeScope()=parent; frame=nullptr;
    }
};
inline Frame *current() { return activeScope() ? activeScope()->frame : nullptr; }
inline bool memoryEnabled() { static const bool value=std::getenv("S9X_REMASTER_MEMORY_METRICS")!=nullptr;return value; }
inline void traffic(Stage stage,uint64_t read,uint64_t written=0) {
    if(!memoryEnabled())return;
    if(auto *f=current()){f->bytesRead[stage]+=read;f->bytesWritten[stage]+=written;}
}
inline void allocation(Stage stage,uint64_t bytes) {
    if(!memoryEnabled())return;
    if(auto *f=current()){++f->allocationCount[stage];f->allocationBytes[stage]+=bytes;}
}
inline uint64_t footprint() {
#ifdef __APPLE__
    task_vm_info_data_t info={};mach_msg_type_number_t count=TASK_VM_INFO_COUNT;
    if(task_info(mach_task_self(),TASK_VM_INFO,reinterpret_cast<task_info_t>(&info),&count)==KERN_SUCCESS)return info.phys_footprint;
#endif
    return 0;
}
inline double cpuMs(const Frame &f) {
    double total=0; for(int i=0;i<Count;i++)if(i!=LockWait && i!=SlotWait)total+=f.exclusive[i];
    return total;
}
inline void write(std::ostream &out,const Frame &f,double gpuStart,double gpuEnd,
    double beginUptime,double submitUptime,double queueMs,double drawableMs,double presentationCpuMs,
    uint32_t width,uint32_t height,uint32_t spheres,uint32_t samples,uint32_t patches,int status) {
    const double gpuMs=std::max(0.0,(gpuEnd-gpuStart)*1000);
    out << "{\"frame_id\":" << f.id << ",\"room\":" << f.room << ",\"plane\":" << f.plane
        << ",\"submodule\":" << f.submodule << ",\"collision\":" << f.collision
        << ",\"scroll\":[" << f.scrollX << ',' << f.scrollY << "],\"width\":" << width << ",\"height\":" << height
        << ",\"cpu_service_ms\":" << cpuMs(f)+presentationCpuMs << ",\"gpu_ms\":" << gpuMs
        << ",\"service_sum_ms\":" << cpuMs(f)+presentationCpuMs+gpuMs
        << ",\"wall_to_gpu_end_ms\":" << (gpuEnd-beginUptime)*1000
        << ",\"gpu_start_uptime\":" << gpuStart << ",\"gpu_end_uptime\":" << gpuEnd
        << ",\"begin_uptime\":" << beginUptime << ",\"submit_uptime\":" << submitUptime
        << ",\"queue_ms\":" << queueMs << ",\"drawable_ms\":" << drawableMs
        << ",\"submit_to_gpu_ms\":" << (gpuStart-submitUptime)*1000
        << ",\"presentation_cpu_ms\":" << presentationCpuMs
        << ",\"epoch_before\":" << f.epochBefore << ",\"epoch_after\":" << f.epochAfter
        << ",\"input_key\":" << f.inputKey << ",\"profile_key\":" << f.profileKey
        << ",\"selected_pixels\":" << f.selected << ",\"updated_pixels\":" << f.updated
        << ",\"resolved_instances\":" << f.resolved << ",\"upload_bytes\":" << f.uploadBytes
        << ",\"projection_decisions\":" << f.projectionDecisions
        << ",\"candidate_hits\":" << f.candidateHits << ",\"candidate_builds\":" << f.candidateBuilds
        << ",\"field_reasons\":" << f.fieldReasons << ",\"highlight_count\":" << f.highlightCount
        << ",\"highlight_path\":" << f.highlightPath << ",\"producer_flags\":" << f.producerFlags
        << ",\"hidden_pixels\":" << f.hiddenPixels << ",\"visible_faces\":" << f.visibleFaces
         << ",\"restores\":" << f.restores << ",\"dependency_reuses\":" << f.dependencyReuses
         << ",\"room_reasons\":" << f.roomReasons << ",\"classification_reasons\":" << f.classificationReasons
         << ",\"classification_outcome\":" << f.classificationOutcome << ",\"projection_outcome\":" << f.projectionOutcome
         << ",\"instance_count\":" << f.instanceCount << ",\"instance_bytes\":" << f.instanceBytes
        << ",\"bounces\":" << f.bounces << ",\"connections\":" << f.connections
        << ",\"representatives\":" << f.representatives << ",\"directional\":" << int(f.directional)
        << ",\"hybrid_direct_enabled\":" << int(f.hybridEnabled)
        << ",\"hybrid_direct_cutoff\":" << f.hybridCutoff
        << ",\"hybrid_direct_tail_samples\":" << f.hybridTailSamples
        << ",\"hybrid_direct_env_overrides\":" << int(f.hybridEnvOverrides)
        << ",\"direct_reuse\":\"" << f.directReuse << "\",\"direct_reuse_reason\":\"" << f.directReuseReason << '"'
        << ",\"room_ray_chunk_hits\":" << f.roomRayChunkHits << ",\"room_ray_chunks_retained\":" << f.roomRayChunksRetained
        << ",\"room_ray_chunks_returning\":" << f.roomRayChunksReturning
        << ",\"room_ray_allocated_bytes\":" << f.roomRayAllocatedBytes
        << ",\"per_ray_cache\":\"" << f.perRayOutcome << "\",\"per_ray_bytes\":" << f.perRayBytes
        << ",\"per_ray_traced\":" << f.perRayTraced << ",\"per_ray_reused\":" << f.perRayReused
        << ",\"per_ray_receiver_changed\":" << f.perRayReceiver << ",\"per_ray_sample_changed\":" << f.perRaySample
        << ",\"per_ray_aged\":" << f.perRayAged << ",\"per_ray_changed_segment\":" << f.perRaySegment
        << ",\"allocations\":" << f.allocations << ",\"spheres\":" << spheres
        << ",\"samples\":" << samples << ",\"patches\":" << patches << ",\"status\":" << status
        << ",\"exclusive_ms\":{";
    for(int i=0;i<Count;i++)out << (i?",":"") << '"' << names[i] << "\":" << f.exclusive[i];
    out << "},\"calls\":{";
    for(int i=0;i<Count;i++)out << (i?",":"") << '"' << names[i] << "\":" << f.calls[i];
    out << '}';
    if(memoryEnabled()) {
        out << ",\"memory_metrics\":{\"method\":\"logical-major-buffers-v1\",\"phys_footprint_bytes\":" << f.footprintBytes
            << ",\"cpu_cache_bytes\":" << f.cpuCacheBytes
            << ",\"gpu_upload_bytes\":" << f.uploadBytes
            << ",\"per_ray_accounted_bytes\":" << f.perRayBytes
            << ",\"per_ray_pair_read_counter_estimate_bytes\":" << (f.perRayTraced+f.perRayReused)*16
            << ",\"per_ray_pair_write_counter_estimate_bytes\":" << f.perRayTraced*16
            << ",\"allocation_method\":\"explicit-major-buffer-events-not-malloc-hook\",\"stages\":{";
        for(int i=0;i<Count;i++)out << (i?",":"") << '"' << names[i] << "\":{\"read_bytes\":" << f.bytesRead[i]
            << ",\"written_bytes\":" << f.bytesWritten[i] << ",\"explicit_allocations\":" << f.allocationCount[i]
            << ",\"explicit_allocation_bytes\":" << f.allocationBytes[i] << ",\"cpp_free_events\":" << f.freeCount[i] << '}';
        out << "}}";
    }
    out << "}\n";
}
}
#endif
