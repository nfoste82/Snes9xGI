#include "../runtime/live_trace.h"
#include <cassert>
#include <sstream>
#include <iostream>
int main() {
 using namespace RemasterLiveTrace;
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
 std::cout << "live trace bounded/drop/async/absent/cancel checks passed\n";
}
