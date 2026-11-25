#include "../runtime/placement_offset_editor.h"
#include "../runtime/profile_merge.h"
#include "../runtime/lighting_field_cache.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if(!(x)){std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);std::abort();} } while(0)
int main() {
 RemasterFrame f;f.width=64;f.height=8;f.area.id=320;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 f.mainPixels.resize(512);f.subPixels.resize(512);f.originalRgb555.resize(512);
 RemasterFrameTileInstance i;i.tileId={123,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=1;i.savedPlacementOffsetStatus=1;
 f.tileInstances.assign(10,i);
 for(uint32_t id=1;id<=10;++id)f.contextLocations[id]={320,uint16_t(199+id),201,1};
 f.contextLocations[3]=f.contextLocations[1]; // wrapped duplicate: once per coordinate
 f.tileInstances[1].hFlip=true;
 f.tileInstances[4].tileId={456,1,4}; // second type
 f.contextLocations[6].area=321; // different world
 f.tileInstances[6].sourceIndex=0;f.contextLocations[7].background=0;
 f.tileInstances[7].savedPlacementOffsetStatus=0; // unverified
 f.tileInstances[8].source=RemasterSourceType::Object;
 for(unsigned n=0;n<512;++n){unsigned cell=n%64/8;auto &p=f.mainPixels[n];p={0x02010000,cell<3?cell+1:cell+2,uint8_t((n/64)*8+n%8)};}
 // ID4 exists but is wholly hidden/unobserved; ID10 also unobserved. Cells for
 // other world, BG1, unverified and OBJ must never join the editable selection.
 std::set<uint32_t> ids;std::set<RemasterTileContentId> types;
#ifdef BASELINE_LOCATION_ONLY
 auto pick=[&](size_t n,bool extend){return S9xRemasterPickPlacementOffset(f,n,extend,ids);};
#else
 auto pick=[&](size_t n,bool extend){return S9xRemasterPickPlacementOffset(f,n,extend,ids,&types);};
#endif
 CHECK(pick(8,false)==2);CHECK((ids==std::set<uint32_t>{1,2,3}));
 CHECK(types.size()==1);
 RemasterProfile p;p.romSha256=f.profileRomSha256;p.gameTitle="test";p.schemaVersion=37;p.authoredOverworldOffsets=true;
 auto a=f.contextLocations[1],b=f.contextLocations[2],hidden=f.contextLocations[4],other=f.contextLocations[6];
 p.overworldOffsets[a]=-40;p.overworldOffsets[b]=24;p.overworldOffsets[hidden]=99;p.overworldOffsets[other]=-72;
 const auto original=p;std::string error;
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,1,true,error));CHECK(p.overworldOffsets[a]==-39 && p.overworldOffsets[b]==25);
 CHECK(p.overworldOffsets[hidden]==99 && p.overworldOffsets[other]==-72);
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,-1,true,error));CHECK(p.overworldOffsets[a]==-40 && p.overworldOffsets[b]==24);
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,-8,false,error));CHECK(p.overworldOffsets[a]==-8 && p.overworldOffsets[b]==-8);
 p.overworldOffsets[b]=32767;CHECK(!S9xRemasterEditPlacementOffsets(p,f,ids,1,true,error));CHECK(p.overworldOffsets[a]==-8);
 p.overworldOffsets[a]=-32767;CHECK(!S9xRemasterEditPlacementOffsets(p,f,ids,-1,true,error));CHECK(p.overworldOffsets[b]==32767);
 p=original;CHECK(pick(24,true)==5);CHECK(ids.size()==4 && types.size()==2);
 const auto sparse=p;CHECK(!p.overworldOffsets.count(f.contextLocations[5]));
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,0,false,error));CHECK(p.overworldOffsets.at(f.contextLocations[5])==0);
 std::string undo;std::vector<RemasterProfileDiagnostic>d;CHECK(S9xRemasterSerializeProfile(sparse,undo,d));
 CHECK(S9xRemasterParseProfile(undo,p,d));CHECK(!p.overworldOffsets.count(f.contextLocations[5]));CHECK(S9xRemasterPlacementMapsEqual(p.overworldOffsets,sparse.overworldOffsets));
 auto disk=sparse;disk.overworldOffsets[other]=-73;auto edited=sparse;
 CHECK(S9xRemasterEditPlacementOffsets(edited,f,ids,7,false,error));RemasterProfileMergeResult merge;
 CHECK(S9xRemasterMergeProfiles(sparse,edited,disk,{},false,merge,d));CHECK(merge.saved.overworldOffsets[a]==7 && merge.saved.overworldOffsets[other]==-73);
 RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.geometryByFlip=true;m.flipGeometry[0].hasHeight=m.flipGeometry[1].hasHeight=true;
 m.flipGeometry[0].height.fill(0);m.flipGeometry[1].height.fill(7);f.assetMetadata={m};
 CHECK(pick(24,true)==1 && types.size()==1); // removed type cannot remain preview
 CHECK(S9xRemasterPlacementLocalGeometry(f,2).height[0]==7);
 S9xRemasterPreviewPlacementOffsets(p,f);RemasterLightingFieldCache cache,forced;cache.update(f,true);
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,1,true,error));S9xRemasterPreviewPlacementOffsets(p,f);
 cache.update(f,true);forced.update(f,true,true);CHECK(cache.height==forced.height);
 CHECK(f.tileInstances[0].savedPlacementOffset==-39 && f.tileInstances[1].savedPlacementOffset==25);
 CHECK(pick(0,true)==0 && ids.empty() && types.empty());
#ifndef BASELINE_LOCATION_ONLY
 CHECK(pick(8,false)==2);
 // Viewport mutation: ID2 becomes wholly hidden, ID4 comes into view. Refresh
 // changes highlighted/editable targets without writing anything to the profile.
 auto before=p.overworldOffsets;
 for(auto &pixel:f.mainPixels)if(pixel.instanceId==2)pixel.instanceId=4;
 S9xRemasterRefreshPlacementSelection(f,ids,&types);
 CHECK((ids==std::set<uint32_t>{1,3,4}));CHECK(S9xRemasterPlacementMapsEqual(before,p.overworldOffsets));
 CHECK(S9xRemasterEditPlacementOffsets(p,f,ids,1,true,error));CHECK(p.overworldOffsets[b]==25 && p.overworldOffsets[hidden]==100);
 // Exact locations remain exact, and hidden old IDs cannot leak into an edit.
 types.clear();ids={2,1};S9xRemasterRefreshPlacementSelection(f,ids);CHECK((ids==std::set<uint32_t>{1}));
 CHECK(S9xRemasterPickPlacementOffset(f,0,true,ids)==0 && ids.empty());
 // Scene selector truth rather than stock main IDs; transparent/overwritten
 // instances present only in the vector do not count as visibility.
 f.tileInstances[1].hFlip=false;f.scenePixels=f.mainPixels;f.sceneRgb555=f.originalRgb555;
 for(auto &pixel:f.scenePixels)if(pixel.instanceId==1 || pixel.instanceId==3)pixel.instanceId=2;
 CHECK(pick(0,false)==2);CHECK(!ids.count(1) && !ids.count(3) && ids.count(2) && ids.count(4));
 f.mainPixels[0].owner=0x02020000;CHECK(S9xRemasterPlacementOffsetPick(f,0)==0);
 for(auto &pixel:f.mainPixels)pixel.owner=0x02020000;
 S9xRemasterRefreshPlacementSelection(f,ids,&types);CHECK(ids.empty());
#endif
 std::puts("PASS type selection: visible exact scene IDs, flips, unique coords, worlds, hidden/unsupported, +/-/set, atomic bounds, sparse Undo/merge, cached preview and viewport refresh");
}
