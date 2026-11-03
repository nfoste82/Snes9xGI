#ifndef REMASTER_LIVE_TRACE_H
#define REMASTER_LIVE_TRACE_H
#include "lighting_trace.h"
#include <condition_variable>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifndef S9X_REMASTER_PROFILE_REVISION
#define S9X_REMASTER_PROFILE_REVISION "unrecorded"
#endif

// Request-only, value-owned, bounded diagnostic rows. No frame I/O, no borrowed
// packet pointers, no latest-metric joins. IDs are process-wide, not row indexes.
namespace RemasterLiveTrace {
struct Row {
 RemasterLightingTrace::Frame frame;
 uint64_t emulated=0;
 double refreshMs=0, gpuStart=0,gpuEnd=0,submit=0,queue=0,drawable=0,presentation=0,beginUptime=0;
 double refreshStart=0, periodMs=0, audioMs=0, limiterMs=0, pacingMs=0, requestedSleepMs=0, presentedTime=0,pacingStart=0;
 uint32_t width=0,height=0,samples=0;
 int status=0;
 bool refreshDone=false,rendererDone=false,submitted=false,gpuDone=false;
 bool renderExpected=true,pacingDone=false,presentExpected=false,presentDone=false,presentationFailed=false;
 std::vector<std::pair<std::string,double>> gpuStages;
};
class Session {
 std::vector<Row> rows;
 size_t limit;
 bool cancelled=false;
 bool lifecycle;
 Row *find(uint64_t id) {
  auto i=std::lower_bound(rows.begin(),rows.end(),id,[](const Row &r,uint64_t key){return r.frame.id<key;});
  return i!=rows.end() && i->frame.id==id?&*i:nullptr;
 }
public:
 explicit Session(size_t n,bool fullLifecycle=false):limit(std::max<size_t>(1,std::min<size_t>(600,n))),lifecycle(fullLifecycle) { rows.reserve(limit); }
 uint64_t begin(uint64_t emulated,bool renderExpected=true,double start=0,double periodMs=0) {
  if(cancelled || rows.size()>=limit)return 0;
  Row r;r.frame.id=RemasterLightingTrace::nextId();r.frame.begin=RemasterLightingTrace::now();r.emulated=emulated;
  r.renderExpected=renderExpected;r.refreshStart=start?start:r.frame.begin;
  r.periodMs=rows.empty()?0:periodMs;
  rows.push_back(r);return r.frame.id;
 }
 bool exhausted() const { return cancelled || rows.size()==limit; }
 void cancel() { cancelled=true; }
 void producer(const RemasterLightingTrace::Frame &f) { if(auto *r=find(f.id))r->frame=f; }
 void renderer(const RemasterLightingTrace::Frame &f,bool submitted) {
  if(auto *r=find(f.id)){r->frame=f;r->rendererDone=true;r->submitted=submitted;}
 }
 void refresh(uint64_t id,double ms) { if(auto *r=find(id)){r->refreshMs=ms;r->refreshDone=true;} }
 void pacing(uint64_t id,double audio,double limiter,double total,double requested,double start=0) {
  if(auto *r=find(id)){r->audioMs=audio;r->limiterMs=limiter;r->pacingMs=total;r->requestedSleepMs=requested;r->pacingStart=start;r->pacingDone=true;}
 }
 void expectPresent(uint64_t id) { if(auto *r=find(id))r->presentExpected=true; }
 void presented(uint64_t id,double time) { if(auto *r=find(id)){r->presentedTime=time;r->presentDone=time>0;} }
 void failed(uint64_t id) { if(auto *r=find(id))r->presentationFailed=true; }
 void stages(uint64_t id,const std::vector<std::pair<std::string,double>> &stages) { if(auto *r=find(id))r->gpuStages=stages; }
 void gpu(uint64_t id,double start,double end,double submit,double queue,double drawable,double cpu,int status,double beginUptime=0) {
  if(auto *r=find(id)){r->gpuStart=start;r->gpuEnd=end;r->submit=submit;r->queue=queue;r->drawable=drawable;r->presentation=cpu;r->status=status;r->gpuDone=true;r->beginUptime=beginUptime;}
 }
 bool complete() const {
  if(!exhausted())return false;
   for(const auto &r:rows)if(!r.refreshDone ||
    (r.renderExpected && !r.rendererDone) || (r.submitted&&!r.gpuDone&&!r.presentationFailed) ||
    (lifecycle && !r.pacingDone) || (r.presentExpected&&!r.presentDone&&!r.presentationFailed))return false;
  return true;
 }
 void write(std::ostream &out) const {
  out<<std::setprecision(15);
  for(size_t i=0;i<rows.size();++i) {
   const auto &r=rows[i];
   const char *state=!r.renderExpected?"emulator_skip":r.presentationFailed?"presentation_failed":!r.rendererDone?"unfinished":!r.submitted?"slot_drop":!r.gpuDone?"unfinished":r.status!=4?"gpu_failed":"completed";
   out<<"{\"schema\":2,\"sequence\":"<<i<<",\"frame_id\":"<<r.frame.id<<",\"emulated_frame\":"<<r.emulated
       <<",\"build\":\""<<__DATE__<<" "<<__TIME__<<"\",\"revision\":\""<<S9X_REMASTER_PROFILE_REVISION<<"\""
       <<",\"state\":\""<<state<<"\",\"refresh_complete\":"<<int(r.refreshDone)<<",\"refresh_inclusive_ms\":"<<r.refreshMs
       <<",\"refresh_start_monotonic\":"<<r.refreshStart<<",\"frame_period_ms\":"<<r.periodMs
       <<",\"render_expected\":"<<int(r.renderExpected)<<",\"pacing_complete\":"<<int(r.pacingDone)
       <<",\"audio_sync_ms\":"<<r.audioMs<<",\"frame_limiter_ms\":"<<r.limiterMs
       <<",\"pacing_total_ms\":"<<r.pacingMs<<",\"requested_sleep_ms\":"<<r.requestedSleepMs
       <<",\"pacing_start_monotonic\":"<<r.pacingStart
       <<",\"present_expected\":"<<int(r.presentExpected)<<",\"present_complete\":"<<int(r.presentDone)
       <<",\"presented_uptime\":"<<r.presentedTime
       <<",\"gpu_evidence\":"<<int(r.gpuDone)<<",\"replay_evidence\":false,\"gpu_stages_ms\":{";
   for(size_t k=0;k<r.gpuStages.size();++k)out<<(k?",":"")<<'"'<<r.gpuStages[k].first<<"\":"<<r.gpuStages[k].second;
   out<<"},\"metrics\":";
   std::ostringstream metrics;
   RemasterLightingTrace::write(metrics,r.frame,r.gpuStart,r.gpuEnd,r.beginUptime,r.submit,r.queue,r.drawable,r.presentation,
     r.frame.width,r.frame.height,r.frame.spheres,r.frame.samples,r.frame.patches,r.status);
   auto text=metrics.str();if(!text.empty()&&text.back()=='\n')text.pop_back();
   out<<text<<"}\n";
  }
 }
};
struct Controller {
 std::mutex mutex;std::condition_variable changed;
 std::shared_ptr<Session> session;
};
// Deliberately process-lifetime: late GPU callbacks and the bounded writer cannot
// race static destruction at shutdown. No storage allocated unless used.
inline Controller &controller() { static auto *c=new Controller;return *c; }
inline bool configured() { static const bool value=S9X_REMASTER_PROFILING && std::getenv("S9X_REMASTER_LIVE_TRACE")!=nullptr;return value; }
inline bool available() { return S9X_REMASTER_PROFILING!=0; }
// Worker/UI publish only a small event; emulator consumes it at its own frame
// boundary. Never mutate GFX/InfoString from the detached writer or UI thread.
enum Notice { NoNotice, Started, Completed, WriteFailed };
inline std::atomic<int> &pendingNotice() { static std::atomic<int> value{NoNotice};return value; }
inline const char *takeNotice() {
 switch(pendingNotice().exchange(NoNotice)) {
  case Started:return "10-second perf capture started";
  case Completed:return "Capture completed.";
  case WriteFailed:return "Performance capture failed to write.";
  default:return nullptr;
 }
}
// Optional explicit path permits a UI capture without relaunch/environment setup.
inline bool request(const std::string &explicitPrefix="",unsigned frames=120) {
 if(!available() || (explicitPrefix.empty()&&!configured()))return false;
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);
 if(c.session)return false; // never overwrite a still-owned capture
 unsigned n=std::max(1u,std::min(600u,frames));
 if(const char *v=std::getenv("S9X_REMASTER_LIVE_TRACE_FRAMES")) { char *end=nullptr;auto parsed=std::strtoul(v,&end,10);if(end!=v&&!*end)n=unsigned(std::min<unsigned long>(600,std::max<unsigned long>(1,parsed))); }
 auto session=std::make_shared<Session>(n,true);c.session=session;
 RemasterLightingTrace::boundedActive().store(true,std::memory_order_relaxed);
 const std::string prefix=explicitPrefix.empty()?std::getenv("S9X_REMASTER_LIVE_TRACE"):explicitPrefix;
 pendingNotice().store(Started);
 std::thread([session,prefix] {
  auto &c=controller();std::unique_lock<std::mutex> lock(c.mutex);
  // Finite capture even if emulation is paused/stopped or presentation fails.
   c.changed.wait_for(lock,std::chrono::seconds(10),[&]{return session->complete();});
  session->cancel();
  RemasterLightingTrace::boundedActive().store(false,std::memory_order_relaxed);
  const auto path=prefix+"."+std::to_string(RemasterLightingTrace::nextId())+".jsonl";
  // All rows are frozen under the join mutex; I/O happens only on this worker.
   c.session.reset();lock.unlock();
   std::ostringstream buffer;session->write(buffer);
   std::ofstream out(path,std::ios::out|std::ios::app);out<<buffer.str();out.close();
   pendingNotice().store(out.fail()?WriteFailed:Completed);
   std::fprintf(stderr,"Per-frame performance capture %s: %s\n",out.fail()?"FAILED":"written",path.c_str());
  }).detach();
 return true;
}
inline RemasterLightingTrace::Frame &refreshFrame() { static thread_local RemasterLightingTrace::Frame f;return f; }
inline uint64_t &pacingFrameId() { static thread_local uint64_t id=0;return id; }
inline bool pacingActive() { return S9X_REMASTER_PROFILING && pacingFrameId()!=0; }
inline void beginRefresh(uint64_t emulated,bool renderExpected=true) {
 if(!S9X_REMASTER_PROFILING || !RemasterLightingTrace::boundedActive().load(std::memory_order_relaxed))return;
 auto &f=refreshFrame();f={};
 const double start=RemasterLightingTrace::now();
 static thread_local double previousStart=0;
 const double period=previousStart>0?(start-previousStart)*1000:0;previousStart=start;
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);
 if(c.session){f.id=c.session->begin(emulated,renderExpected,start,period);if(f.id)f.begin=start;}
 pacingFrameId()=f.id;
}
template<class F> inline void update(F fn) {
 if(!S9X_REMASTER_PROFILING || !RemasterLightingTrace::boundedActive().load(std::memory_order_relaxed))return;
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);if(c.session)fn(*c.session);c.changed.notify_all();
}
inline void endRefresh() {
 auto &f=refreshFrame();if(!f.id)return;
 const double ms=(RemasterLightingTrace::now()-f.begin)*1000;
 update([&](Session &s){s.refresh(f.id,ms);});f={};
}
inline void producer(const RemasterLightingTrace::Frame &f) {
 if(!f.id)return;
 // Stock presentation must inherit completed assembly, not the empty frame-start
 // token: renderer publication otherwise overwrites all producer evidence.
 if(refreshFrame().id==f.id)refreshFrame()=f;
 update([&](Session &s){s.producer(f);});
}
inline void renderer(const RemasterLightingTrace::Frame &f,bool submitted) { if(f.id)update([&](Session &s){s.renderer(f,submitted);}); }
inline void pacing(double audio,double limiter,double total,double requested,double start=0) {
 const auto id=pacingFrameId();pacingFrameId()=0;
 if(id)update([&](Session &s){s.pacing(id,audio,limiter,total,requested,start);});
}
inline void expectPresent(uint64_t id) { if(id)update([&](Session &s){s.expectPresent(id);}); }
inline void presented(uint64_t id,double time) { if(id)update([&](Session &s){s.presented(id,time);}); }
inline void failed(uint64_t id) { if(id)update([&](Session &s){s.failed(id);}); }
inline void gpuStages(uint64_t id,const std::vector<std::pair<std::string,double>> &stages) {
 if(id && !stages.empty())update([&](Session &s){s.stages(id,stages);});
}
inline void gpu(uint64_t id,double start,double end,double submit,double queue,double drawable,double cpu,int status,double beginUptime=0) {
 if(id)update([&](Session &s){s.gpu(id,start,end,submit,queue,drawable,cpu,status,beginUptime);});
}
}
#endif
