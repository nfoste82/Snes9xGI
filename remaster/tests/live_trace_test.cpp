#include "../runtime/live_trace.h"
#include <cassert>
#include <sstream>
#include <iostream>
int main() {
 using namespace RemasterLiveTrace;
 assert(!takeNotice());
 pendingNotice().store(Started);
 assert(std::string(takeNotice())=="10-second perf capture started");
 assert(!takeNotice());
 std::thread writer([]{pendingNotice().store(Completed);});writer.join();
 assert(std::string(takeNotice())=="Capture completed.");
 pendingNotice().store(WriteFailed);
 assert(std::string(takeNotice())=="Performance capture failed to write.");
 assert(!takeNotice());
 Session s(2);
 auto a=s.begin(10), b=s.begin(11);
 assert(a && b && !s.begin(12));
 RemasterLightingTrace::Frame f; f.id=b; f.fieldReasons=3;
 s.renderer(f, false); s.refresh(b,4);
 f.id=a; s.renderer(f,true); s.refresh(a,5);
 s.gpu(a,1,2,3,4,5,6,7);
 assert(s.complete());
 std::ostringstream out; s.write(out);
 assert(out.str().find("\"field_reasons\":3")!=std::string::npos);
 assert(out.str().find("slot_drop")!=std::string::npos);
 Session missing(1); auto c=missing.begin(1); missing.refresh(c,2);
 std::ostringstream absent; missing.write(absent);
 assert(absent.str().find("unfinished")!=std::string::npos);
 assert(absent.str().find("\"replay_evidence\":false")!=std::string::npos);
 missing.cancel(); assert(!missing.begin(2));
 // A completion from an unrelated/previous session cannot update any row.
 missing.gpu(a,1,2,3,4,5,6,7);
 Session sequence(120);std::vector<uint64_t> ids;
 for(unsigned i=0;i<120;++i){ids.push_back(sequence.begin(i));assert(ids.back());}
 assert(!sequence.begin(120));
 for(unsigned i=0;i<120;++i){f.id=ids[i];sequence.producer(f);sequence.renderer(f,true);sequence.refresh(f.id,i);}
 // Reverse GPU completion order; producer order must remain stable in output.
 for(unsigned i=120;i-->0;)sequence.gpu(ids[i],1,2,3,4,5,6,7);
 assert(sequence.complete());std::ostringstream ordered;sequence.write(ordered);
 auto text=ordered.str();size_t pos=0;
 for(unsigned i=0;i<120;++i){auto next=text.find("\"emulated_frame\":"+std::to_string(i)+",",pos);assert(next!=std::string::npos);pos=next+1;}
 // A skipped emulated frame is evidence, not an unfinished renderer submission.
 Session lifecycle(3,true);
 auto skipped=lifecycle.begin(1,false,10,999);
 auto drawn=lifecycle.begin(2,true,10.016,16);
 auto failed=lifecycle.begin(3,true,10.032,16);
 lifecycle.refresh(skipped,.2);lifecycle.pacing(skipped,70,8,78,7);
 f.id=drawn;f.roomReasons=RemasterLightingTrace::PlaneChange|RemasterLightingTrace::MapChange;
 f.classificationOutcome=2;f.classificationReasons=RemasterLightingTrace::MapChange;
 lifecycle.producer(f);lifecycle.renderer(f,true);lifecycle.refresh(drawn,90);
 lifecycle.expectPresent(drawn);lifecycle.gpu(drawn,11,11.002,10.99,0,0,.1,4);
 lifecycle.pacing(drawn,0,0,0,0);
 f.id=failed;lifecycle.renderer(f,true);lifecycle.refresh(failed,5);
 lifecycle.failed(failed);lifecycle.pacing(failed,0,1,1,1);
 assert(!lifecycle.complete()); // GPU completion alone isn't screen presentation.
 lifecycle.presented(drawn,11.02);assert(lifecycle.complete());
 std::ostringstream details;lifecycle.write(details);auto json=details.str();
 assert(json.find("\"schema\":2")!=std::string::npos);
 assert(json.find("emulator_skip")!=std::string::npos);
 assert(json.find("presentation_failed")!=std::string::npos);
 assert(json.find("\"audio_sync_ms\":70")!=std::string::npos);
 assert(json.find("\"classification_reasons\":256")!=std::string::npos);
 assert(json.find("\"frame_period_ms\":0")!=std::string::npos); // first row has no predecessor
 // Stock fallback reads the TLS token after assembly publication.
 RemasterLiveTrace::refreshFrame()={};RemasterLiveTrace::refreshFrame().id=9999;
 f={};f.id=9999;f.roomReasons=RemasterLightingTrace::MapChange;
 f.calls[RemasterLightingTrace::EndFrame]=1;f.exclusive[RemasterLightingTrace::EndFrame]=91;
 RemasterLiveTrace::producer(f);
 assert(RemasterLiveTrace::refreshFrame().calls[RemasterLightingTrace::EndFrame]==1);
 assert(RemasterLiveTrace::refreshFrame().roomReasons==RemasterLightingTrace::MapChange);
 assert(RemasterLiveTrace::refreshFrame().exclusive[RemasterLightingTrace::EndFrame]==91);
 RemasterLiveTrace::refreshFrame()={};
 std::cout << "live trace bounded/drop/async/absent/cancel checks passed\n";
}
