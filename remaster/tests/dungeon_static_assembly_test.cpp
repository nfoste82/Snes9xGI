// Declaration research is offline-only and never grants production geometry.
#include "../runtime/remaster.h"
#include "../tools/ALttP/dungeon_static_assembly.h"
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
	const auto declarations=S9xRemasterDecodeDungeonDeclarations(nullptr,0,false,8);
	assert(!declarations.complete);
	assert(S9xRemasterAssessDungeonStaticCandidates(declarations,nullptr).empty());
	assert(!S9xRemasterDungeonResearchEnabled());
	assert(!S9xRemasterState().runtimeDungeonHeightValid);
	std::cout<<"PASS offline absent declarations stay unresolved; production grants no geometry\n";
}
