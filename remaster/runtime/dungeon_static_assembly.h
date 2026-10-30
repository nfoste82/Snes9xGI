#ifndef REMASTER_DUNGEON_STATIC_ASSEMBLY_H
#define REMASTER_DUNGEON_STATIC_ASSEMBLY_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// Bounded declaration/effect evidence, NOT a writer trace or a geometry grant.
// Addresses are USA LoROM addresses; the adapter must verify the entire ROM.
// Unknown handlers have a whole-room effect, never an inferred small rectangle.
struct RemasterDungeonDeclaration {
	uint32_t address=0;
	uint16_t raw=0;
	uint8_t third=0, phase=0, subtype=0, id=0, x=0, y=0, width=0, height=0;
	bool door=false;
	bool operator==(const RemasterDungeonDeclaration &b) const {
		return address==b.address && raw==b.raw && third==b.third && phase==b.phase &&
			subtype==b.subtype && id==b.id && x==b.x && y==b.y && width==b.width && height==b.height && door==b.door;
	}
};
struct RemasterDungeonConstructionDeclarations {
	bool complete=false;
	uint16_t room=0xffff;
	uint8_t floor=0, layout=0;
	std::array<uint8_t,14> header={};
	std::vector<RemasterDungeonDeclaration> records;
	bool operator==(const RemasterDungeonConstructionDeclarations &b) const {
		return complete==b.complete && room==b.room && floor==b.floor && layout==b.layout && header==b.header && records==b.records;
	}
};

inline RemasterDungeonConstructionDeclarations S9xRemasterDecodeDungeonDeclarations(
	const uint8_t *rom,size_t size,bool verifiedUsa,uint16_t room)
{
	RemasterDungeonConstructionDeclarations out;
	if(!rom || size!=1048576 || !verifiedUsa || room>=320)return out;
	auto read=[&](uint32_t a,int n,uint32_t &v) {
		if(a>0xffffff || !(a&0x8000) || n<1 || n>3 || (a&0xffff)+unsigned(n)>0x10000)return false;
		const size_t at=((a>>16)&127)*32768+(a&32767);
		if(at+size_t(n)>size)return false;
		v=0;for(int k=0;k<n;k++)v|=uint32_t(rom[at+k])<<(k*8);return true;
	};
	uint32_t stream=0,meta=0,v=0;
	if(!read(0x1f8000+room*3,3,stream) || !read(0x04f502+room*2,2,meta))return out;
	meta=meta==0xffef?0x82edc5:0x040000|meta;
	for(int k=0;k<14;k++){if(!read(meta+k,1,v))return {};out.header[k]=uint8_t(v);}
	if(!read(stream,2,v))return {};
	out.room=room;out.floor=v&255;out.layout=v>>8;
	if((out.layout>>2)>=8)return {};
	uint32_t defaults=0;
	if(!read(0x84ef2f+(out.layout>>2)*3,3,defaults))return {};
	// The ROM loader runs the default layout and three room streams. All
	// sentinels must be bank-local and the complete declaration count bounded.
	auto layer=[&](uint32_t &at,uint8_t phase) {
		const uint32_t bank=at&0xff0000;bool doors=false;
		for(int count=0;count<2048;count++) {
			if((at&0xff0000)!=bank || !read(at,2,v))return false;
			const uint16_t word=uint16_t(v);
			if(word==0xffff){at+=2;return true;}
			if(word==0xfff0 && !doors){doors=true;at+=2;continue;}
			if(out.records.size()>=2048)return false;
			RemasterDungeonDeclaration d;d.address=at;d.phase=phase;d.raw=word;d.door=doors;
			if(doors) {
				if(phase==0)return false; // no default-layout door dispatch
				d.id=word>>8;d.x=(word>>4)&15;d.y=word&3;at+=2;
			} else {
				if(!read(at+2,1,v))return false;d.third=uint8_t(v);at+=3;
				if((word&0xfc)==0xfc) {
					d.subtype=2;d.id=d.third&63;d.x=((word&3)<<4)|(word>>12);d.y=((word>>8)&15)*4+(d.third>>6);
				} else {
					d.x=(word&255)>>2;d.y=word>>10;d.width=word&3;d.height=(word>>8)&3;
					d.subtype=d.third<0xf8?0:1;
					d.id=d.subtype?((d.third&7)<<4)|(d.height<<2)|d.width:d.third;
				}
			}
			out.records.push_back(d);
		}
		return false;
	};
	if(!layer(defaults,0))return {};
	stream+=2;
	for(uint8_t phase=1;phase<=3;phase++)if(!layer(stream,phase))return {};
	out.complete=true;return out;
}

struct RemasterDungeonEffectBound {
	bool known=false;
	uint8_t backgrounds=3; // bit0 BG1, bit1 BG2
	int x=0,y=0,width=64,height=64;
	bool intersects(int xx,int yy,int w,int h) const {
		return x<xx+w && xx<x+width && y<yy+h && yy<y+height;
	}
};

inline RemasterDungeonEffectBound S9xRemasterDungeonDeclarationEffect(const RemasterDungeonDeclaration &d)
{
	// Deliberately small verified set. No dimension guesses for other handlers.
	// ROM table names are misleading: BG2 pointers start at WRAM $4000
	// (actual BG1), BG1 pointers at $2000 (actual BG2). Use actual map names.
	const uint8_t bg=d.phase==3?2:1;
	if(!d.door && d.subtype==1 && (d.id==0x1b || d.id==0x1c) && d.x<=60 && d.y<=60)
		return {true,3,d.x,d.y,4,4}; // unconditional both-BG regular intra-room stair
	if(!d.door && d.subtype==0 && d.id>=0xc5 && d.id<=0xca &&
		d.x+4*(d.width+1)<=64 && d.y+4*(d.height+1)<=64)
		return {true,bg,d.x,d.y,4*(d.width+1),4*(d.height+1)};
	if(d.door && d.y==1 && d.x<12 && (d.id==0x0e || d.id==0x10 || d.id==4)) {
		static const uint16_t south[]={0xd1c,0xd3c,0xd5c,0xb9c,0xbbc,0xbdc,0x1d1c,0x1d3c,0x1d5c,0x1b9c,0x1bbc,0x1bdc};
		const int cell=south[d.x]/2;
		if(cell/64+(d.id==0x0e?4:11)>64)return {}; // cross-map pointer effects not modeled
		// type10/type04 additionally prioritize seven BG2 rows after the
		// footprint; type04 copies the far row from BG1 to BG2. Bound BOTH.
		return {true,uint8_t(d.id==4?3:2),cell%64,cell/64,4,d.id==0x0e?4:11};
	}
	return {};
}

enum class RemasterDungeonStaticRejection : uint8_t {
	UnsupportedStateEffects, CompetingDeclaration
};
struct RemasterDungeonStaticCandidate {
	uint32_t declaration=0;
	uint16_t origin=0,source=0;
	uint8_t phase=0,subtype=0,id=0;
	bool stair=false;
	RemasterDungeonStaticRejection rejection=RemasterDungeonStaticRejection::UnsupportedStateEffects;
	std::vector<uint32_t> competingDeclarations;
	std::vector<uint32_t> unboundedDeclarations;
};

// Assessment never supplies geometry. Even a complete final match must retain
// the explicit dynamic-state/contact blocker. This prevents an incomplete
// applicability table from becoming an accidental production certificate.
inline std::vector<RemasterDungeonStaticCandidate> S9xRemasterAssessDungeonStaticCandidates(
	const RemasterDungeonConstructionDeclarations &declarations,const uint8_t *maps)
{
	std::vector<RemasterDungeonStaticCandidate> out;
	if(!declarations.complete || !maps)return out;
	for(const auto &d:declarations.records) {
		const bool stair=!d.door && d.subtype==1 && (d.id==0x1b || d.id==0x1c);
		const bool cave=d.door && d.y==1 && d.x<12 && (d.id==0x0e || d.id==0x10 || d.id==4);
		if(!stair && !cave)continue;
		const auto region=S9xRemasterDungeonDeclarationEffect(d);
		if(!region.known)continue; // even the candidate's footprint is unsupported
		RemasterDungeonStaticCandidate c;c.declaration=d.address;c.origin=region.y*64+region.x;
		c.source=stair?(d.id==0x1b?0x10c8:0x10e8):0x26f6;
		c.phase=d.phase;c.subtype=d.subtype;c.id=d.id;c.stair=stair;
		for(const auto &other:declarations.records)if(other.address!=d.address) {
			const auto effect=S9xRemasterDungeonDeclarationEffect(other);
			if((effect.backgrounds&region.backgrounds) && effect.intersects(region.x,region.y,4,4)) {
				c.competingDeclarations.push_back(other.address);
				if(!effect.known)c.unboundedDeclarations.push_back(other.address);
			}
		}
		// Known intersecting operations also require part-level equivalence;
		// this assessment does not turn an effect bound into ownership.
		c.rejection=c.competingDeclarations.empty()?RemasterDungeonStaticRejection::UnsupportedStateEffects:
			RemasterDungeonStaticRejection::CompetingDeclaration;
		out.push_back(c);
	}
	return out;
}
#endif
