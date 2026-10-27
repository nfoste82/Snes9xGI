#ifndef REMASTER_LIGHTING_TRACE_H
#define REMASTER_LIGHTING_TRACE_H

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <ostream>

// Opt-in, producing-frame lineage. CPU intervals are elapsed service spans, not
// scheduler thread-CPU time. Exclusive accounting removes nested work exactly
// once; waits have their own stages and are not lighting service. No GPU wait is
// included in CPU service. GPU timestamps use the host uptime clock on macOS.
namespace RemasterLightingTrace {
enum Stage { EndFrame, Finalize, Room, Classification, Solve, Apply, Visible,
    Actors, Offscreen, Fields, Selection, Mesh, Setup, Upload, Scan, Spheres,
    Hidden, Cluster, Sources, Encode, LockWait, SlotWait, Recognition, Count };
static constexpr const char *names[] = {"endframe", "finalize", "room", "classification",
    "solve", "apply", "visible", "actors", "offscreen", "fields", "selection", "mesh",
    "setup", "upload", "scan", "spheres", "hidden", "cluster", "sources", "encode",
    "lock_wait", "slot_wait", "recognition"};
inline double now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
inline bool enabled() { return std::getenv("S9X_REMASTER_LIGHTING_TRACE") != nullptr; }
inline uint64_t nextId() { static std::atomic<uint64_t> id{0}; return ++id; }
struct Frame {
    uint64_t id=0;
    double begin=0, producerEnd=0;
    std::array<double, Count> exclusive{};
    std::array<uint64_t, Count> calls{};
    uint64_t epochBefore=0, epochAfter=0, inputKey=0, profileKey=0;
    uint64_t selected=0, updated=0, resolved=0, uploadBytes=0, allocations=0;
    uint64_t projectionDecisions=0;
    uint64_t candidateHits=0,candidateBuilds=0;
    uint32_t bounces=0,connections=0,representatives=0;
    bool directional=false;
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
    Scope(Frame *f, Stage s):frame(f && f->id ? f : nullptr),stage(s) {
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
        << ",\"bounces\":" << f.bounces << ",\"connections\":" << f.connections
        << ",\"representatives\":" << f.representatives << ",\"directional\":" << int(f.directional)
        << ",\"allocations\":" << f.allocations << ",\"spheres\":" << spheres
        << ",\"samples\":" << samples << ",\"patches\":" << patches << ",\"status\":" << status
        << ",\"exclusive_ms\":{";
    for(int i=0;i<Count;i++)out << (i?",":"") << '"' << names[i] << "\":" << f.exclusive[i];
    out << "},\"calls\":{";
    for(int i=0;i<Count;i++)out << (i?",":"") << '"' << names[i] << "\":" << f.calls[i];
    out << "}}\n";
}
}
#endif
