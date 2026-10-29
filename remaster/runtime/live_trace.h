#ifndef REMASTER_LIVE_TRACE_H
#define REMASTER_LIVE_TRACE_H
#include "lighting_trace.h"
#include <condition_variable>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

// Request-only, value-owned, bounded diagnostic rows. No frame I/O, no borrowed
// packet pointers, no latest-metric joins. IDs are process-wide, not row indexes.
namespace RemasterLiveTrace {
struct Row {
 RemasterLightingTrace::Frame frame;
 uint64_t emulated=0;
 double refreshMs=0, gpuStart=0,gpuEnd=0,submit=0,queue=0,drawable=0,presentation=0,beginUptime=0;
 uint32_t width=0,height=0,samples=0;
 int status=0;
 bool refreshDone=false,rendererDone=false,submitted=false,gpuDone=false;
};
class Session {
 std::vector<Row> rows;
 size_t limit;
 bool cancelled=false;
 Row *find(uint64_t id) { for(auto &r:rows)if(r.frame.id==id)return &r;return nullptr; }
public:
 explicit Session(size_t n):limit(std::max<size_t>(1,std::min<size_t>(600,n))) { rows.reserve(limit); }
 uint64_t begin(uint64_t emulated) {
  if(cancelled || rows.size()>=limit)return 0;
  Row r;r.frame.id=RemasterLightingTrace::nextId();r.frame.begin=RemasterLightingTrace::now();r.emulated=emulated;
  rows.push_back(r);return r.frame.id;
 }
 bool exhausted() const { return cancelled || rows.size()==limit; }
 void cancel() { cancelled=true; }
 void producer(const RemasterLightingTrace::Frame &f) { if(auto *r=find(f.id))r->frame=f; }
 void renderer(const RemasterLightingTrace::Frame &f,bool submitted) {
  if(auto *r=find(f.id)){r->frame=f;r->rendererDone=true;r->submitted=submitted;}
 }
 void refresh(uint64_t id,double ms) { if(auto *r=find(id)){r->refreshMs=ms;r->refreshDone=true;} }
 void gpu(uint64_t id,double start,double end,double submit,double queue,double drawable,double cpu,int status,double beginUptime=0) {
  if(auto *r=find(id)){r->gpuStart=start;r->gpuEnd=end;r->submit=submit;r->queue=queue;r->drawable=drawable;r->presentation=cpu;r->status=status;r->gpuDone=true;r->beginUptime=beginUptime;}
 }
 bool complete() const {
  if(!exhausted())return false;
  for(const auto &r:rows)if(!r.refreshDone || !r.rendererDone || (r.submitted&&!r.gpuDone))return false;
  return true;
 }
 void write(std::ostream &out) const {
  out<<std::setprecision(15);
  for(size_t i=0;i<rows.size();++i) {
   const auto &r=rows[i];
   const char *state=!r.rendererDone?"unfinished":!r.submitted?"slot_drop":!r.gpuDone?"unfinished":"completed";
   out<<"{\"schema\":1,\"sequence\":"<<i<<",\"frame_id\":"<<r.frame.id<<",\"emulated_frame\":"<<r.emulated
      <<",\"state\":\""<<state<<"\",\"refresh_complete\":"<<int(r.refreshDone)<<",\"refresh_inclusive_ms\":"<<r.refreshMs
      <<",\"gpu_evidence\":"<<int(r.gpuDone)<<",\"replay_evidence\":false,\"metrics\":";
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
inline bool configured() { static const bool value=std::getenv("S9X_REMASTER_LIVE_TRACE")!=nullptr;return value; }
inline void request() {
 if(!configured())return;
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);
 if(c.session)return; // never overwrite a still-owned capture
 unsigned n=120;
 if(const char *v=std::getenv("S9X_REMASTER_LIVE_TRACE_FRAMES")) { char *end=nullptr;auto parsed=std::strtoul(v,&end,10);if(end!=v&&!*end)n=unsigned(std::min<unsigned long>(600,std::max<unsigned long>(1,parsed))); }
 auto session=std::make_shared<Session>(n);c.session=session;
 RemasterLightingTrace::boundedActive().store(true,std::memory_order_relaxed);
 const std::string prefix=std::getenv("S9X_REMASTER_LIVE_TRACE");
 std::thread([session,prefix] {
  auto &c=controller();std::unique_lock<std::mutex> lock(c.mutex);
  // Finite capture even if emulation is paused/stopped or presentation fails.
  c.changed.wait_for(lock,std::chrono::seconds(30),[&]{return session->complete();});
  session->cancel();
  RemasterLightingTrace::boundedActive().store(false,std::memory_order_relaxed);
  const auto path=prefix+"."+std::to_string(RemasterLightingTrace::nextId())+".jsonl";
  // All rows are frozen under the join mutex; I/O happens only on this worker.
  std::ostringstream buffer;session->write(buffer);c.session.reset();lock.unlock();
  std::ofstream out(path,std::ios::out|std::ios::app);out<<buffer.str();
 }).detach();
}
inline RemasterLightingTrace::Frame &refreshFrame() { static thread_local RemasterLightingTrace::Frame f;return f; }
inline void beginRefresh(uint64_t emulated) {
 if(!RemasterLightingTrace::boundedActive().load(std::memory_order_relaxed))return;
 auto &f=refreshFrame();f={};
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);
 if(c.session){f.id=c.session->begin(emulated);if(f.id)f.begin=RemasterLightingTrace::now();}
}
template<class F> inline void update(F fn) {
 if(!RemasterLightingTrace::boundedActive().load(std::memory_order_relaxed))return;
 auto &c=controller();std::lock_guard<std::mutex> lock(c.mutex);if(c.session)fn(*c.session);c.changed.notify_all();
}
inline void endRefresh() {
 auto &f=refreshFrame();if(!f.id)return;
 const double ms=(RemasterLightingTrace::now()-f.begin)*1000;
 update([&](Session &s){s.refresh(f.id,ms);});f={};
}
inline void producer(const RemasterLightingTrace::Frame &f) { if(f.id)update([&](Session &s){s.producer(f);}); }
inline void renderer(const RemasterLightingTrace::Frame &f,bool submitted) { if(f.id)update([&](Session &s){s.renderer(f,submitted);}); }
inline void gpu(uint64_t id,double start,double end,double submit,double queue,double drawable,double cpu,int status,double beginUptime=0) {
 if(id)update([&](Session &s){s.gpu(id,start,end,submit,queue,drawable,cpu,status,beginUptime);});
}
}
#endif
