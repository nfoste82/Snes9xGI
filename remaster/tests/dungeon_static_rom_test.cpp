#include "../runtime/dungeon_static_assembly.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <iterator>

int main(int argc,char **argv) {
	if(argc!=2){std::cerr<<"usage: verified-USA-ROM\n";return 2;}
	std::ifstream f(argv[1],std::ios::binary);
	std::vector<uint8_t> rom{std::istreambuf_iterator<char>(f),{}};
	// Tool caller pins SHA256; decoder additionally checks layout and all bounds.
	assert(!S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size(),false,8).complete);
	assert(!S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size()-1,true,8).complete);
	assert(!S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size(),true,320).complete);
	const auto d=S9xRemasterDecodeDungeonDeclarations(rom.data(),rom.size(),true,8);
	assert(d.complete && d.records.size()==95 && d.layout==0 && d.header[5]==0 && d.header[6]==0);
	int caves=0,stairs=0;
	for(const auto &r:d.records) {
		if(r.address==0x0a864f) {
			assert(r.door && r.id==0x48 && r.x==11 && r.y==2);
		}
		if(r.address==0x0a8651) {
			assert(r.door && r.id==0x0e && r.x==8 && r.y==1);caves++;
			const auto e=S9xRemasterDungeonDeclarationEffect(r);
			assert(e.known && e.x==46 && e.y==58 && e.width==4 && e.height==4 && e.backgrounds==2);
			auto variant=r;variant.id=0x10;
			assert(!S9xRemasterDungeonDeclarationEffect(variant).known); // priority rows cross map
			variant.x=0;assert(S9xRemasterDungeonDeclarationEffect(variant).height==11);
			variant.id=4;assert(S9xRemasterDungeonDeclarationEffect(variant).backgrounds==3);
			variant.y=0;assert(!S9xRemasterDungeonDeclarationEffect(variant).known);
		}
		if(r.address==0x0a85ef) {
			assert(!r.door && r.subtype==1 && r.id==0x1c && r.x==42 && r.y==51);stairs++;
		}
	}
	assert(caves==1 && stairs==1);
	std::array<uint8_t,16384> maps={};
	const auto candidates=S9xRemasterAssessDungeonStaticCandidates(d,maps.data());
	assert(candidates.size()==2);
	for(const auto &c:candidates) {
		assert(c.rejection==RemasterDungeonStaticRejection::CompetingDeclaration);
		assert(!c.unboundedDeclarations.empty());
		std::cout<<(c.stair?"stair":"cave")<<" origin="<<c.origin<<" competing="<<c.competingDeclarations.size()
			<<" unbounded="<<c.unboundedDeclarations.size()<<" rejected\n";
	}
	// Every malformed pointer/delimiter fails closed, no partial declarations.
	auto bad=rom;
	const size_t pointer=0xf8000+8*3;
	bad[pointer]=bad[pointer+1]=bad[pointer+2]=0;
	assert(!S9xRemasterDecodeDungeonDeclarations(bad.data(),bad.size(),true,8).complete);
	// A bounded disjoint known fill is not confused with a potentially global
	// unknown effect. Neither licenses a certificate without dynamic/contact proof.
	RemasterDungeonConstructionDeclarations small;small.complete=true;
	for(const auto &r:d.records)if(r.address==0x0a8651)small.records.push_back(r);
	auto fill=small.records[0];fill.address++;fill.phase=3;fill.door=false;fill.subtype=0;fill.id=0xc6;fill.x=0;fill.y=0;
	small.records.push_back(fill);
	auto a=S9xRemasterAssessDungeonStaticCandidates(small,maps.data());
	assert(a.size()==1 && a[0].competingDeclarations.empty() && a[0].rejection==RemasterDungeonStaticRejection::UnsupportedStateEffects);
	small.records.back().x=46;small.records.back().y=58;
	a=S9xRemasterAssessDungeonStaticCandidates(small,maps.data());assert(a[0].competingDeclarations.size()==1);
	small.records.back().id=0xff;
	a=S9xRemasterAssessDungeonStaticCandidates(small,maps.data());assert(a[0].unboundedDeclarations.size()==1);
	std::cout<<"PASS ROM declarations, variants, bounded/disjoint/unknown effects and conservative rejection\n";
}
