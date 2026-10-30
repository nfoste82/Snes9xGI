// Read-only declaration/effect capability inventory. Never a geometry approval.
#include "../../runtime/dungeon_static_assembly.h"
#include "../../../sha256.cpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
int main(int argc,char **argv) {try {
	if(argc!=2)throw std::runtime_error("usage: verified-USA-ROM");
	std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
	if(rom.size()!=1048576)throw std::runtime_error("ROM size");
	unsigned char hash[32];sha256sum(rom.data(),static_cast<unsigned>(rom.size()),hash);
	const unsigned char expected[]={0x66,0x87,0x1d,0x66,0xbe,0x19,0xad,0x2c,0x34,0xc9,0x27,0xd6,0xb1,0x4c,0xd8,0xeb,0x6f,0xc3,0x18,0x19,0x65,0xb6,0xe5,0x17,0xcb,0x36,0x1f,0x73,0x16,0x00,0x9c,0xfb};
	if(std::memcmp(hash,expected,32))throw std::runtime_error("ROM fingerprint");
	std::array<uint8_t,16384> maps={};int decoded=0,candidates=0;
	std::cout<<"{\"format\":\"alttp-static-declaration-capability-v1\",\"approval\":false,\"rooms\":[";
	for(int room=0;room<320;room++) {
		const auto d=S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size(),true,room);
		if(d.complete)decoded++;
		const auto claims=S9xRemasterAssessDungeonStaticCandidates(d,maps.data());candidates+=claims.size();
		if(room)std::cout<<',';
		std::cout<<"{\"room\":"<<room<<",\"declarations_complete\":"<<(d.complete?"true":"false")<<",\"declarations\":"<<d.records.size()<<",\"candidates\":[";
		for(size_t n=0;n<claims.size();n++) {
			const auto &c=claims[n];if(n)std::cout<<',';
			std::cout<<"{\"address\":"<<c.declaration<<",\"origin\":"<<c.origin<<",\"source\":"<<c.source<<",\"stair\":"<<(c.stair?"true":"false")<<",\"variant\":"<<int(c.id)<<",\"competing\":"<<c.competingDeclarations.size()<<",\"unbounded\":[";
			bool comma=false;for(const auto &r:d.records)if(std::find(c.unboundedDeclarations.begin(),c.unboundedDeclarations.end(),r.address)!=c.unboundedDeclarations.end()) {
				if(comma)std::cout<<',';comma=true;
				std::cout<<"["<<r.address<<','<<int(r.phase)<<','<<r.door<<','<<int(r.subtype)<<','<<int(r.id)<<','<<int(r.x)<<','<<int(r.y)<<"]";
			}
			std::cout<<"],\"accepted\":false}";
		}
		std::cout<<"]}";
	}
	std::cout<<"],\"decoded\":"<<decoded<<",\"candidates\":"<<candidates<<",\"accepted\":0,\"unsupported_dynamic_effects\":true,\"plane_contact_proof\":false}\n";
	return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
