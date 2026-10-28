#ifndef S9X_REMASTER_DUNGEON_WALL_BAND_H
#define S9X_REMASTER_DUNGEON_WALL_BAND_H

// Complete classified assemblies, not camera exposure or hash-wide roles.
// This header consumes Placement read-only and never writes physical support.
#include "frame.h"
#include "dungeon_visible_face.h"

namespace S9xRemasterVisibleFace {
struct WallBand {
	uint8_t downhill=0;
	uint64_t mask=0;
	int assembly=0, direction=-1, phase=0;
	bool corner=false, frame=false;
};
inline WallBand wallBand(const RemasterDungeonFloorContext::Placement &t,
	int cell,int axis,int down,bool section) {
	using Surface=RemasterDungeonFloorContext::Surface;
	WallBand out;
	if(!t.hasHeight || t.stairSourceAmbiguous || t.authoredObstacle || t.obstacleAssembly || t.diagonalHeightPreview)return out;
	static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
	if(section)out.downhill=axis?(down>0?8:4):(down>0?2:1);
	const int source=t.wallTemplateOffset;
	const bool perimeter=source==0x2c8 || source==0x2d8 || source==0x2e8 || source==0x2f8 ||
		source==0x308 || source==0x318 || source==0x328 || source==0x338;
	out.corner=t.surface==Surface::Wall && source>=0xb66 && source<=0xd46;
	out.frame=(t.surface==Surface::Door || (t.surface==Surface::ClosedDoor && t.closedDoorAssembly)) &&
		t.doorAssembly && t.doorTemplateOffset && t.doorDirection<4;
	if(!section && !(t.surface==Surface::Wall && perimeter) && !out.corner && !out.frame)return {};
	out.mask=out.frame && !t.closedDoorAssembly?(t.hasDoorframeMask?t.doorframeMask:0):UINT64_MAX;
	if(!out.mask)return {};
	Region r{t.height,t.hflip,t.vflip,true};
	if(!out.downhill && !out.frame) {
		// Monotone authored relief supplies one or two explicit facings. Flat
		// perimeter caps use the exact handler direction, never default normals.
		for(int axis=0;axis<2;axis++) {
			int sign=0;bool good=true;
			for(int y=0;y<8;y++)for(int x=0;x<8;x++)if((axis?x:y)<7) {
				int d=sample(r,x+(axis?1:0),y+(axis?0:1))-sample(r,x,y);
				if(d && sign && (d>0?1:-1)!=sign)good=false;
				if(d)sign=d>0?1:-1;
			}
			if(good && sign)out.downhill|=axis?(sign>0?4:8):(sign>0?1:2);
		}
		if(!out.downhill && perimeter) {
			int d=source==0x2c8 || source==0x308?3:source==0x2d8 || source==0x318?2:
				source==0x2e8 || source==0x328?1:0;
			int x=dx[d]*(t.hflip?-1:1),y=dy[d]*(t.vflip?-1:1);
			for(int k=0;k<4;k++)if(dx[k]==x && dy[k]==y)out.downhill=1<<k;
		}
		if(!out.corner && (out.downhill&(out.downhill-1)))return {};
		// A folded corner has two facings; its surviving straight branches
		// legitimately have only one. Both are independently classified ROM
		// members. Flat/nonmonotone art still has no demonstrated facing here.
		if(out.corner && !out.downhill)return {};
	}
	if(out.frame) {
		out.assembly=t.doorAssembly;out.direction=t.doorDirection;
		// Direction is room-facing/downhill. ROM origin defines each band even
		// when its pixels form disconnected jambs around an open void.
		int d=t.doorDirection^1;out.downhill=1<<d;
		int origin=t.doorAssembly-1;
		int band=t.doorDirection==0?origin/64+2-cell/64:t.doorDirection==1?cell/64-origin/64:
			t.doorDirection==2?origin%64+2-cell%64:cell%64-origin%64;
		if(band<0 || band>2)return {};
		int maximum=INT16_MIN;
		for(int k=0;k<64;k++)if(out.mask&(UINT64_C(1)<<k))maximum=std::max(maximum,int(t.height[k]));
		int block=maximum>=0?maximum/8*8:0;bool phased=block>0 && block==8*band;
		for(int k=0;k<64;k++)if(out.mask&(UINT64_C(1)<<k))phased&=
			(!t.closedDoorAssembly && t.height[k]==0) || (t.height[k]>=block && t.height[k]<=block+7);
		if(phased && !t.closedDoorAssembly)for(int k=0;k<64;k++)if(!t.height[k])out.mask&=~(UINT64_C(1)<<k);
		out.phase=8*band-(phased?block:0);
	}
	return out;
}
inline uint8_t wallEdgeSamples(const WallBand &a,const Region &ar,const WallBand &b,const Region &br,int dx,int dy) {
	uint8_t mask=0;
	for(int k=0;k<8;k++) {
		int ax=dx?(dx>0?7:0):k,ay=dy?(dy>0?7:0):k,bx=dx?(dx>0?0:7):k,by=dy?(dy>0?0:7):k;
		int ak=(ar.vflip?7-ay:ay)*8+(ar.hflip?7-ax:ax),bk=(br.vflip?7-by:by)*8+(br.hflip?7-bx:bx);
		if((a.mask&(UINT64_C(1)<<ak)) && (b.mask&(UINT64_C(1)<<bk)))mask|=1<<k;
	}
	return mask;
}
inline uint8_t cornerFaceSamples(const Region &a,const Region &b,int dx,int dy,uint8_t mask) {
	int am=*std::max_element(a.height.begin(),a.height.end()),bm=*std::max_element(b.height.begin(),b.height.end());
	for(int k=0;k<8;k++) {
		int ax=dx?(dx>0?7:0):k,ay=dy?(dy>0?7:0):k,bx=dx?(dx>0?0:7):k,by=dy?(dy>0?0:7):k;
		if((am<bm && sample(a,ax,ay)==am) || (bm<am && sample(b,bx,by)==bm))mask&=~(1<<k);
	}
	return mask;
}
// A HIGH-corner outer band can be a unit face clipped by a ceiling. This
// proves the region geometrically, before examining the proposed tangent
// neighbor (in particular, not by selecting whichever deltas happen to agree).
// Require the ordered downhill branch as independent assembly evidence.
inline uint64_t highCornerFaceMask(const RemasterDungeonFloorContext::Placement *tiles,
	int cell,int dir) {
	const auto high=[](int s){return (s>=0xb66 && s<=0xbc6 && (s-0xb66)%0x20==0) ||
		(s>=0xc66 && s<=0xcc6 && (s-0xc66)%0x20==0);};
	const auto &t=tiles[cell];
	if(!high(t.wallTemplateOffset) || !t.hasHeight || t.stairSourceAmbiguous)return UINT64_MAX;
	Region r{t.height,t.hflip,t.vflip,true};
	const int lo=*std::min_element(t.height.begin(),t.height.end()),hi=*std::max_element(t.height.begin(),t.height.end());
	if(hi<=lo || hi>=lo+7)return UINT64_MAX;
	static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
	uint64_t face=0;
	for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
		const int q=dir==0?y:dir==1?7-y:dir==2?x:7-x;
		if(sample(r,x,y)!=std::min(hi,lo+q))return UINT64_MAX;
		if(lo+q<hi)face|=UINT64_C(1)<<((t.vflip?7-y:y)*8+(t.hflip?7-x:x));
	}
	const int x=cell%64+dx[dir],y=cell/64+dy[dir];
	if(x<0 || x>=64 || y<0 || y>=64)return UINT64_MAX;
	const auto &n=tiles[y*64+x];
	if(!high(n.wallTemplateOffset) || (t.wallTemplateOffset-0xb66)/0x40!=(n.wallTemplateOffset-0xb66)/0x40 ||
		!n.hasHeight || n.stairSourceAmbiguous || n.surface!=RemasterDungeonFloorContext::Surface::Wall)return UINT64_MAX;
	Region nr{n.height,n.hflip,n.vflip,true};int delta=INT_MIN;
	for(int k=0;k<8;k++) {
		int ax=dx[dir]?(dx[dir]>0?7:0):k,ay=dy[dir]?(dy[dir]>0?7:0):k;
		int bx=dx[dir]?7-ax:ax,by=dy[dir]?7-ay:ay;
		// The downstream member must itself be an exact unit continuation.
		if(sample(nr,bx+dx[dir],by+dy[dir])-sample(nr,bx,by)!=-1)return UINT64_MAX;
		int d=sample(r,ax,ay)-1-sample(nr,bx,by);
		if(delta!=INT_MIN && delta!=d)return UINT64_MAX;delta=d;
	}
	return face;
}
}
#endif
