#include "../runtime/remaster.h"
#include <cassert>
#include <cstring>
#include <iostream>

// Run each environment combination in a fresh process: trace configuration is
// intentionally launch-time state. No BeginFrame, packet output or app needed.
int main(int argc,char **argv) {
 assert(argc==2);
 const bool only=std::strcmp(argv[1],"only")==0;
 auto &state=S9xRemasterState();
 state.requestedFramePath="pending-existing-packet";
 state.requestedFrameCount=17;
 state.activeFramePath="active-existing-packet";
 state.activeFrameCount=21;
 state.frameCaptureActive=true;
 state.animationCapture.frameCount=9;
 state.animationCapture.representativeFrame.width=37;
 state.captureWidth=71;
 const auto *evidence=state.animationDungeonEvidence.get();
 const auto *overworld=state.animationOverworldEvidence.get();
 S9xRemasterRequestFrameCapture("must-not-be-written.s9xrmf",120);
 assert(state.activeFramePath=="active-existing-packet" && state.activeFrameCount==21);
 assert(state.frameCaptureActive && state.animationCapture.frameCount==9);
 assert(state.animationCapture.representativeFrame.width==37 && state.captureWidth==71);
 assert(state.animationDungeonEvidence.get()==evidence && state.animationOverworldEvidence.get()==overworld);
 if(only) {
  assert(state.requestedFramePath=="pending-existing-packet" && state.requestedFrameCount==17);
  assert(RemasterLightingTrace::boundedActive().load());
 } else {
  assert(state.requestedFramePath=="must-not-be-written.s9xrmf" && state.requestedFrameCount==120);
 }
 // Ordinary live profiling starts with no animation capture at all. The trace
 // trigger must not create pending work that BeginFrame would later activate.
 state.requestedFramePath.clear();state.requestedFrameCount=1;
 state.activeFramePath.clear();state.activeFrameCount=1;
 state.frameCaptureActive=false;state.animationCapture=RemasterAnimationCapture();
 S9xRemasterRequestFrameCapture("second-must-not-be-written.s9xrmf",0);
 assert(!state.frameCaptureActive && !state.animationCapture.frameCount);
 assert(state.animationCapture.representativeFrame.mainPixels.empty());
 assert(state.animationCapture.tracks.empty());
 if(only)assert(state.requestedFramePath.empty() && state.requestedFrameCount==1);
 else assert(state.requestedFramePath=="second-must-not-be-written.s9xrmf" && state.requestedFrameCount==1);
 std::cout<<"capture trigger mode="<<argv[1]<<" state preservation PASS\n";
}
