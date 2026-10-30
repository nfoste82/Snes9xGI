// Production EndFrame must retain bounded declaration diagnostics without
// treating declarations or final-word coincidence as a geometry certificate.
#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;

int main() {
	// Fixture has no ROM: absent evidence must be explicit, never guessed.
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=8;
	RemasterProfile p;S9xRemasterSetProfile(p);S9xRemasterSetLiveFramesEnabled(true);
	uint16_t rgb=0;RemasterFrame frame;
	S9xRemasterBeginFrame(1,1,1,1);
	S9xRemasterEndFrame(&rgb,1,1,1,&frame,&c);
#ifdef REMASTER_STATIC_DECLARATION_ASSESSMENT
	assert(S9xRemasterState().dungeonStaticAssessmentRoom==8);
	assert(!S9xRemasterState().dungeonStaticDeclarationsComplete);
	assert(S9xRemasterState().dungeonStaticCandidates.empty());
	assert(!S9xRemasterDungeonResearchEnabled());
	std::cout<<"PASS absent declarations stay unresolved in production EndFrame\n";
#else
	std::cerr<<"FAIL production EndFrame has no static declaration assessment\n";
	return 1;
#endif
}
