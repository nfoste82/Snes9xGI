// Real pinned map/profile/graphics through production EndFrame, with independently
// fingerprinted ROM declarations. No synthetic writer or certificate is supplied.
#define REMASTER_WALL_BAND_DIAGNOSTICS 1
#include "../runtime/remaster.h"
#include "../../sha256.cpp"
#include <iostream>
#include <stdexcept>
static void staticInputs(RemasterDungeonFloorContext &c) {
	const char *path=std::getenv("ALTTP_STATIC_TEST_ROM");
	if(!path)throw std::runtime_error("ALTTP_STATIC_TEST_ROM required");
	std::ifstream f(path,std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
	if(rom.size()!=1048576)throw std::runtime_error("ROM size");
	unsigned char hash[32];sha256sum(rom.data(),static_cast<unsigned>(rom.size()),hash);
	const unsigned char expected[]={0x66,0x87,0x1d,0x66,0xbe,0x19,0xad,0x2c,0x34,0xc9,0x27,0xd6,0xb1,0x4c,0xd8,0xeb,0x6f,0xc3,0x18,0x19,0x65,0xb6,0xe5,0x17,0xcb,0x36,0x1f,0x73,0x16,0x00,0x9c,0xfb};
	if(std::memcmp(hash,expected,32))throw std::runtime_error("ROM fingerprint");
#ifdef REMASTER_STATIC_DECLARATION_ASSESSMENT
	c.constructionDeclarations=std::make_shared<const RemasterDungeonConstructionDeclarations>(
		S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size(),true,c.roomIndex));
#endif
}
#define REMASTER_ENDFRAME_STATIC_INPUTS(c) staticInputs(c)
#define main staticReinjectionMain
#include "../tools/ALttP/alttp_reinject_endframe_probe.cpp"
#undef main
int main(int argc,char **argv) {
	const int result=staticReinjectionMain(argc,argv);if(result)return result;
	const auto &s=S9xRemasterState();
#ifdef REMASTER_STATIC_DECLARATION_ASSESSMENT
	check(s.dungeonStaticAssessmentRoom==8 && s.dungeonStaticDeclarationsComplete,"actual ROM declarations missing");
	check(s.dungeonStaticCandidates.size()==2,"expected two scoped declarations");
	for(const auto &c:s.dungeonStaticCandidates) {
		check(c.rejection==RemasterDungeonStaticRejection::CompetingDeclaration,"unknown effects must reject before graph");
		std::cout<<"REJECT source="<<c.source<<" origin="<<c.origin<<" unbounded="<<c.unboundedDeclarations.size()<<'\n';
	}
#else
	std::cerr<<"FAIL no production declaration assessment\n";return 1;
#endif
	check(!S9xRemasterDungeonResearchEnabled(),"CPU observer/research activated");
	const auto &m=s.runtimeDungeonHeightMap;
	check(m.stairAttachments.empty() && m.doorframeAttachments.empty(),"rejected evidence leaked attachment");
	// Independent acceptance oracle intentionally remains failing until an actual
	// complete static certificate + occupied contacts can authorize the repair.
	if(std::getenv("ALTTP_REQUIRE_STATIC_HEIGHT_REPAIR")) {
		check(m.planeOffsets[0][55*64+43]-m.planeOffsets[1][50*64+43]==24,"UNFIXED quantized rise must be 24, not configured 50");
		bool frame=false;for(const auto &a:m.doorframeAttachments)if(a.cell==59*64+46 && a.plane==0 && a.base==m.planeOffsets[0][58*64+46])frame=true;
		check(frame,"UNFIXED cave north-foot attachment");
	}
	uint16_t rgb=0;RemasterFrame frame;S9xRemasterBeginFrame(1,1,1,1);
	S9xRemasterEndFrame(&rgb,1,1,1,&frame);
#ifdef REMASTER_STATIC_DECLARATION_ASSESSMENT
	check(S9xRemasterState().dungeonStaticCandidates.empty() && !S9xRemasterState().dungeonStaticDeclarationsComplete,"game/context switch retained assessment");
#endif
	std::cout<<"PASS real EndFrame input, fail-closed declarations, full channel/capture roundtrip; HEIGHT REPAIR NOT PASSED\n";
}
