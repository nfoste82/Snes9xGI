// Retain the old approximation as a base-selection oracle; continuity_test
// covers the new signed exact-depth transport with the same placement sources.
#define ALTTP_TEST_LEGACY_OFFSCREEN_SPHERES
#include "remaster.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
static void need(bool b,const char *s){if(!b){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
int main() {
 auto maps=std::make_unique<std::array<uint8_t,16384>>();
 auto graphics=std::make_unique<std::array<uint8_t,65536>>();(*graphics)[32]=128;
 uint8_t indices[64]={};indices[0]=1;RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
 RemasterProfile profile;auto &a=profile.assets[id];a.tileId=id;a.hasEmission=a.hasHeight=true;
 a.height[0]=30;a.emissionDepth=2;a.emissionRgba[0]=255;a.emissionRgba[3]=25;
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=7;c.linkPlane=1;
 c.constructedMaps=maps->data();c.graphics=graphics->data();c.bg1GraphicsBase=c.graphicsBase=0;
 const int cell=16*64+59;(*maps)[cell*2]=(*maps)[8192+cell*2]=1;
 auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=c.roomIndex;map->hasRoomGeometry=map->hasPlaneOffsets=true;
 map->offsets.fill(255);for(auto &v:map->planeOffsets)v.fill(255);for(auto &v:map->structuralPlaneOffsets)v.fill(255);
 map->producerHashes[cell]=id.hash;
 RemasterFrame frame;frame.width=256;frame.height=224;
 auto run=[&]{frame.lights.clear();return S9xRemasterAppendOffscreenRoomLights(frame,c,profile,map.get(),nullptr,S9xRemasterHashTile);};
 a.emissionRgba[3]=0;need(run().placements==0 && run().unknownBases==0,"all-zero authored emission is not unknown source geometry");a.emissionRgba[3]=25;
 a.emissionRgba[0]=0;need(run().placements==0,"black positive-alpha emission is not a physical source");a.emissionRgba[0]=255;
 map->offsets[cell]=0;map->planeOffsets[1][cell]=0;
 need(run().lights==1 && std::fabs(frame.lights[0].z-(30*16.0f/255+1))<0.0001f,"plane1-only solved placement is already consumed with authored relief");
 RemasterDungeonHeightMap::LocalHeightPreview mounted;mounted.cell=cell;mounted.height.fill(7);map->localHeightPreviews.push_back(mounted);
 need(run().lights==1 && std::fabs(frame.lights[0].z-(7*16.0f/255+1))<0.0001f && a.height[0]==30,"offscreen source consumes same placement-local backing as visible without authoring mutation");
 map->localHeightPreviews.clear();
 map->offsets[cell]=255;
 need(run().lights==0,"support beneath unknown artwork is not its placement base");
 map->planeOffsets[0][cell]=50;
 need(run().lights==0,"overlapping support planes cannot select an unknown artwork base");
 map->planeOffsets[0][cell]=map->planeOffsets[1][cell]=255;
 need(run().lights==0,"unknown on every plane remains absent, never defaults to zero");
 map->offsets[cell]=-8;map->structuralPlaneOffsets[0][cell]=-8;
 for(int plane=0;plane<2;plane++){c.linkPlane=plane;need(run().lights==1 && std::fabs(frame.lights[0].z-(22*16.0f/255+1))<0.0001f,"fixture uses signed visible structural base independent of Link plane");}
 (*maps)[cell*2+1]=(*maps)[8193+cell*2]=0xc0;map->producerFlips[cell]=3;
 need(run().lights==1 && frame.lights[0].x==479.5f && frame.lights[0].y==135.5f,"transformed producer retains exact canonical source");
 map->producerFlips[cell]=0;need(run().lights==0,"overwritten flip cannot reuse solved base");map->producerFlips[cell]=3;
 map->producerHashes[cell]++;need(run().lights==0,"changed animation producer cannot reuse solved base");map->producerHashes[cell]=id.hash;
 map->roomIndex++;need(run().lights==0,"changed room cannot reuse solved base");map->roomIndex=c.roomIndex;
 (*maps)[cell*2]=(*maps)[cell*2+1]=0;
 need(run().lights==0,"distinct BG1 emitter cannot borrow solved BG2 geometry");
  frame.lights.clear();need(S9xRemasterAppendOffscreenRoomLights(frame,c,profile,nullptr,map.get(),S9xRemasterHashTile).lights==1,"distinct BG1 uses its own proven map");
 std::array<uint16_t,256> palette={};palette[1]=31;c.emitterPalette=palette.data();a.emissionColorMask=0;
 frame.lights.clear();need(S9xRemasterAppendOffscreenRoomLights(frame,c,profile,nullptr,map.get(),S9xRemasterHashTile).lights==1 && frame.lights[0].red>0 && frame.lights[0].blue==0,"offscreen artwork emission uses current red CGRAM");
 palette[1]=31<<10;
 frame.lights.clear();need(S9xRemasterAppendOffscreenRoomLights(frame,c,profile,nullptr,map.get(),S9xRemasterHashTile).lights==1 && frame.lights[0].red==0 && frame.lights[0].blue>0,"offscreen artwork emission follows palette animation");
 std::puts("Offscreen emitter base/provenance checks passed");
}
