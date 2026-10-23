#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

int main()
{
	using S=RemasterDungeonFloorContext::Surface;
	auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	auto map=std::make_unique<RemasterDungeonHeightMap>();
	std::vector<uint8_t> attr(8192,1);
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;
	c.placements=tiles->data();c.collisionAttributes=attr.data();c.linkX=20*8;c.linkY=19*8;
	for(int rise:{37,50,83})for(int flip=0;flip<4;flip++) {
		*tiles={};attr.assign(8192,1);
		for(int y:{20,26})for(int yy=y;yy<y+3;yy++)for(int x=20;x<24;x++) {
			auto &t=tiles->at(yy*64+x);t.surface=S::Floor;t.hasHeight=true;attr[yy*64+x]=0;
		}
		for(int y=23;y<26;y++)for(int x=20;x<24;x++) {
			auto &t=tiles->at(y*64+x);t.surface=S::Wall;t.hasHeight=true;t.wallTemplateOffset=0x2e8;
			for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
		}
		const int cell=24*64+24;auto &door=tiles->at(cell);
		door.surface=S::Door;door.hasHeight=door.hasDoorframeMask=true;
		door.doorTemplateOffset=0x2716;door.doorAssembly=cell+1;door.doorDirection=0;
		door.hflip=flip&1;door.vflip=flip&2;
		for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
			int k=(door.vflip?7-y:y)*8+(door.hflip?7-x:x);
			door.height[k]=x<4?7-y:39-y; // opening can be an existing stair
			if(x<4)door.doorframeMask|=UINT64_C(1)<<k;
		}
		assert(S9xRemasterBuildDungeonHeightMap(c,rise,*map));
		assert(map->doorframeAttachments.size()==1 && map->doorframeAttachments[0].cell==cell);
		assert(map->doorframeAttachments[0].base==8 && map->doorframeAttachments[0].plane==0);
		// The application accepts signed solved bases, not configured-rise
		// multiples or a hardcoded floor offset. Pin a translated solved band.
		map->doorframeAttachments[0].base+=rise;
		// A hidden other-plane attachment is retained for support auditing but
		// cannot veto the visible structural owner's frame. Active Link plane
		// alone is not visual ownership at a bridge/underpass overlap.
		map->doorframeAttachments.push_back({cell,1,-19,door.doorframeMask,door.doorTemplateOffset,false});
		const auto authored=door.height;
		RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
		RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=1;
		instance.hFlip=door.hflip;instance.vFlip=door.vflip;instance.heightOffset=3;f.tileInstances.push_back(instance);
		RemasterFrameAssetMetadata a;a.tileRole=RemasterTileRole::Door;a.hasHeight=a.hasDoorframeMask=true;
		a.height=door.height;a.doorframeMask=door.doorframeMask;f.assetMetadata.push_back(a);
		for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
			auto &p=f.mainPixels[y*8+x];p.instanceId=1;p.tilePixel=(door.vflip?7-y:y)*8+(door.hflip?7-x:x);
		}
		c.backgroundScrollX=24*8;c.backgroundScrollY=24*8;
		S9xRemasterApplyDungeonHeightMap(f,c,*map);
		assert(f.tileInstances.size()==2); // mixed frame/opening is not tile-wide
		for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
			const auto &p=f.mainPixels[y*8+x];const auto &i=f.tileInstances[p.instanceId-1];
			assert(!i.hasPlacementHeight);
			assert(i.heightOffset+f.assetMetadata[0].height[p.tilePixel]==3+int(authored[p.tilePixel])+(x<4?8+rise:0));
		}
		assert(f.assetMetadata[0].height==authored && door.height==authored);
		// Two genuinely visible owners disagree: preserve authored geometry.
		map->doorframeAttachments.back().visible=true;
		RemasterFrame ambiguous;ambiguous.width=ambiguous.height=8;ambiguous.mainPixels.resize(64);ambiguous.subPixels.resize(64);
		ambiguous.tileInstances.push_back(instance);ambiguous.assetMetadata.push_back(a);
		for(int k=0;k<64;k++){ambiguous.mainPixels[k].instanceId=1;ambiguous.mainPixels[k].tilePixel=k;}
		S9xRemasterApplyDungeonHeightMap(ambiguous,c,*map);
		assert(ambiguous.tileInstances.size()==1 && ambiguous.tileInstances[0].heightOffset==3);
		// No attachment from absent/empty mask, unsupported dispatch or support.
		for(int bad=0;bad<4;bad++) {
			auto saved=door;
			if(bad==0)door.hasDoorframeMask=false;
			if(bad==1)door.doorframeMask=0;
			if(bad==2)door.doorTemplateOffset=0;
			if(bad==3)door.doorDirection=0xfe;
			assert(S9xRemasterBuildDungeonHeightMap(c,rise,*map));assert(map->doorframeAttachments.empty());door=saved;
		}
		for(int y=23;y<26;y++)tiles->at(y*64+23).wallTemplateOffset=0;
		assert(S9xRemasterBuildDungeonHeightMap(c,rise,*map));assert(map->doorframeAttachments.empty());
		// The same door seam can be owned by a proved corner branch rather
		// than the straight perimeter handler. Require exact same seam solve.
		for(int y=23;y<26;y++)tiles->at(y*64+23).wallTemplateOffset=0xc06;
		assert(S9xRemasterBuildDungeonHeightMap(c,rise,*map));
		assert(map->doorframeAttachments.size()==1 && map->doorframeAttachments[0].base==8);
	}
	std::cout<<"PASS doorframe signed band seams, rises37/50/83, H/V masks, stair opening preservation, absent/empty/unsupported/ambiguous ownership rejection\n";
	// Complete template ownership, priority-only writes, and final overwrites.
	auto maps=std::make_unique<std::array<uint8_t,16384>>();
	auto gfx=std::make_unique<std::array<uint8_t,65536>>();
	auto tpl=std::make_unique<std::array<uint8_t,12876>>();tpl->fill(0xff);
	RemasterProfile profile;uint8_t pixels[64]={};RemasterTileContentId id{S9xRemasterHashTile(4,pixels),1,4};
	auto &a=profile.assets[id];a.tileId=id;a.tileRole=RemasterTileRole::Door;a.hasHeight=a.hasDoorframeMask=true;a.doorframeMask=~UINT64_C(0);
	c.constructedMaps=maps->data();c.graphics=gfx->data();c.dungeonTemplateData=tpl->data();
	for(int d=0;d<4;d++) {
		const int starts[]={0x2716,0x29f0,0x2c6a,0x2e6a};const int w=d<2?4:3,h=d<2?3:4;
		for(int flip=0;flip<4;flip++) {
			maps->fill(0xff);tpl->fill(0xff);
			for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
				const int i=(10+y)*64+10+x,n=starts[d]+2*(x*h+y);
				(*maps)[2*i]=(*tpl)[n]=0;(*tpl)[n+1]=flip<<6;(*maps)[2*i+1]=(flip<<6)|0x20;
			}
			S9xRemasterDungeonPlacements(c,profile,*tiles);
			assert(tiles->at(10*64+10).doorDirection==d && tiles->at(10*64+10).doorAssembly==651);
			(*maps)[2*((10+h-1)*64+10+w-1)]=1;
			S9xRemasterDungeonPlacements(c,profile,*tiles);assert(!tiles->at(10*64+10).doorTemplateOffset);
		}
	}
	std::cout<<"PASS exact twelve-word ownership all four directions/HV transforms, priority-only preservation and partial overwrite rejection\n";
	// High-range/remapped dispatch splits the same twelve words across BGs.
	// Outer band belongs to BG2; two inner bands belong to BG1. Test every
	// direction and each background, not merely the reported east doorway.
	for(int d=0;d<4;d++)for(int variant=0;variant<2;variant++)for(int bg=0;bg<2;bg++) {
		const int starts[]={0x2716,0x29f0,0x2c6a,0x2e6a};const int off=starts[d]+variant*24;
		const int w=d<2?4:3,h=d<2?3:4;maps->fill(1);tpl->fill(0xff);
		c.placementBackgroundIndex=bg;
		for(int x=0;x<w;x++)for(int y=0;y<h;y++) {
			const int owner=(d==0?y==0:d==1?y==h-1:d==2?x==0:x==w-1)?1:0;
			const int i=(10+y)*64+10+x,n=off+2*(x*h+y),target=(owner==bg?0:8192)+2*i;
			(*maps)[target]=(*tpl)[n]=0;(*maps)[target+1]=0x20;(*tpl)[n+1]=0;
		}
		S9xRemasterDungeonPlacements(c,profile,*tiles);
		int count=0;for(const auto &t:*tiles)if(t.splitDoorPart){count++;assert(t.doorDirection==d && t.doorTemplateOffset==off);}
		assert(count==(bg?4:8));
		// Opposite-background final overwrite destroys all ownership evidence.
		const int target=bg?8192:0;(*maps)[target+2*(11*64+11)]=1;
		S9xRemasterDungeonPlacements(c,profile,*tiles);
		for(const auto &t:*tiles)assert(!t.splitDoorPart);
	}
	std::cout<<"PASS split lower/remapped template ownership, all directions/BGs, priority-only writes and cross-background overwrite rejection\n";
}
