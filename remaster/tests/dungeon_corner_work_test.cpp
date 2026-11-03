#define ALTTP_TEST_CORNER_WORK
#include "../runtime/remaster.h"
#include <cstdio>
#include <cstdlib>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(){
	auto words=std::make_unique<std::array<uint8_t,16384>>();
	auto graphics=std::make_unique<std::array<uint8_t,65536>>();
	auto templates=std::make_unique<std::array<uint8_t,12876>>();
	// Every corner's mandatory first word is absent. No geometry is skipped
	// merely because metadata or collision is missing.
	for(int off=0xb66;off<=0xd46;off+=0x20)for(int p=0;p<16;p++)(*templates)[off+p*2]=17;
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=true;c.constructedMaps=words->data();c.graphics=graphics->data();c.dungeonTemplateData=templates->data();
	RemasterProfile profile;auto out=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	S9xRemasterDungeonPlacements(c,profile,*out);
	std::printf("corner origins=%llu expected=0\n",(unsigned long long)S9xRemasterDungeonCornerOrigins());
	if(S9xRemasterDungeonCornerOrigins())std::abort();
}
