#include "../runtime/placement_offset_editor.h"
#include "../runtime/profile_merge.h"
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <cstdio>
#define CHECK(x) do{if(!(x)){std::printf("FAIL %d %s\n",__LINE__,#x);std::abort();}}while(0)
int main(){RemasterFrame f;f.area.id=320;f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
RemasterProfile p;p.romSha256=f.profileRomSha256;p.gameTitle="test";p.authoredOverworldOffsets=true;p.schemaVersion=37;
RemasterFrameTileInstance i;i.tileId={123,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=1;i.savedPlacementOffsetStatus=1;f.tileInstances={i,i,i};
f.contextLocations[1]={320,200,201,1};f.contextLocations[2]={320,201,201,1};f.contextLocations[3]=f.contextLocations[1];
auto a=f.contextLocations[1],b=f.contextLocations[2],dark=a;dark.area=321;p.overworldOffsets[a]=-40;p.overworldOffsets[b]=24;p.overworldOffsets[dark]=80;
std::string error;CHECK(S9xRemasterEditPlacementOffsets(p,f,{1,2,3},1,true,error));CHECK(p.overworldOffsets[a]==-39 && p.overworldOffsets[b]==25 && p.overworldOffsets[dark]==80);
CHECK(S9xRemasterEditPlacementOffsets(p,f,{1,2},-8,false,error));CHECK(p.overworldOffsets[a]==-8 && p.overworldOffsets[b]==-8);
p.overworldOffsets[b]=32767;CHECK(!S9xRemasterEditPlacementOffsets(p,f,{1,2},1,true,error));CHECK(p.overworldOffsets[a]==-8);
CHECK(S9xRemasterEditPlacementOffsets(p,f,{1},0,false,error));CHECK(!p.overworldOffsets.count(a));
int n;for(auto s:{"","1.0","32768","-32768","1x","999999999999999999999"," 1"})CHECK(!S9xRemasterParsePlacementScalar(s,n));CHECK(S9xRemasterParsePlacementScalar("-32767",n)&&n==-32767);
f.tileInstances[1].source=RemasterSourceType::Object;CHECK(!S9xRemasterEditPlacementOffsets(p,f,{1,2},1,true,error));CHECK(!p.overworldOffsets.count(a));
f.tileInstances[1]=i;S9xRemasterPreviewPlacementOffsets(p,f);CHECK(f.tileInstances[1].savedPlacementOffset==32767);CHECK(f.tileInstances[1].heightOffset==32767);
auto snapshot=f;S9xRemasterPreviewPlacementOffsets(p,f);CHECK(f.tileInstances[1].heightOffset==snapshot.tileInstances[1].heightOffset);
// Retained live/replay current-profile packets: remove only our scalar, retain
// independent datum and immutable original. Sparse enabled zero is available.
f.width=8;f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);f.originalRgb555.resize(64);
for(unsigned k=0;k<64;k++)f.mainPixels[k]={0x02010000,2,uint8_t(k)};
f.tileInstances[1].heightOffset+=7;const auto recorded=f;
RemasterLightingFieldCache cache,forced;cache.update(f,true);
p.authoredOverworldOffsets=false;S9xRemasterApplyProfileToFrame(p,f);S9xRemasterPreviewPlacementOffsets(p,f);
CHECK(f.tileInstances[1].heightOffset==7 && f.tileInstances[1].savedPlacementOffset==0 && f.tileInstances[1].savedPlacementOffsetStatus==0);
std::vector<float> field;S9xRemasterPlacementOffsetField(f,true,field);CHECK(field[0]==0 && field[1]==0);
cache.update(f,true);forced.update(f,true,true);CHECK(cache.height==forced.height && cache.height[0]==7/255.f);
S9xRemasterPreviewPlacementOffsets(p,f);CHECK(f.tileInstances[1].heightOffset==7);
p.authoredOverworldOffsets=true;S9xRemasterApplyProfileToFrame(p,f);S9xRemasterPreviewPlacementOffsets(p,f);
CHECK(f.tileInstances[1].heightOffset==32774 && f.tileInstances[1].savedPlacementOffsetStatus==2);
cache.update(f,true);forced.update(f,true,true);CHECK(cache.height==forced.height && cache.height[0]==32774/255.f);
p.overworldOffsets.erase(b);S9xRemasterPreviewPlacementOffsets(p,f);
CHECK(f.tileInstances[1].heightOffset==7 && f.tileInstances[1].savedPlacementOffsetStatus==1);
CHECK(recorded.tileInstances[1].heightOffset==32774 && recorded.tileInstances[1].savedPlacementOffset==32767);
p.overworldOffsets[b]=32767;
// Actual click backend used by native UI. Same hash, distinct coordinates/flips;
// deselect representative B must focus selected A, not hash-equivalent B.
f.width=16;f.mainPixels.resize(128);f.subPixels.resize(128);f.originalRgb555.resize(128);
for(unsigned k=0;k<128;k++)f.mainPixels[k]={0x02010000,k%16<8?1u:2u,uint8_t((k/16)*8+k%8)};
f.tileInstances[1].hFlip=true;
std::set<uint32_t> ids;
CHECK(S9xRemasterPickPlacementOffset(f,0,false,ids)==1);
CHECK(S9xRemasterPickPlacementOffset(f,8,true,ids)==2 && ids.size()==2);
CHECK(S9xRemasterPickPlacementOffset(f,8,true,ids)==1 && ids==std::set<uint32_t>{1});
CHECK(S9xRemasterPickPlacementOffset(f,0,true,ids)==0 && ids.empty());
RemasterFrameAssetMetadata meta;meta.tileId=i.tileId;meta.geometryByFlip=true;
meta.flipGeometry[0].hasHeight=meta.flipGeometry[1].hasHeight=true;
meta.flipGeometry[0].height.fill(0);meta.flipGeometry[1].height.fill(7);f.assetMetadata={meta};
CHECK(S9xRemasterPlacementLocalGeometry(f,1).hasHeight && S9xRemasterPlacementLocalGeometry(f,1).height[0]==0);
CHECK(S9xRemasterPlacementLocalGeometry(f,2).height[0]==7);
f.scenePixels=f.mainPixels;f.sceneRgb555=f.originalRgb555;
f.tileInstances[1].hFlip=false; // scene alias must agree in transform (producer contract)
for(auto &pixel:f.scenePixels)pixel.instanceId=2;
CHECK(S9xRemasterPlacementOffsetPick(f,0)==2); // divergent stock ID1 / scene ID2
f.mainPixels[0].owner=0x02020000;CHECK(S9xRemasterPlacementOffsetPick(f,0)==0);
f.mainPixels[0].owner=0x02010000;
f.scenePixels.clear();f.sceneRgb555.clear();
const auto rom=p.romSha256;p.romSha256="different-target";
S9xRemasterApplyProfileToFrame(p,f);S9xRemasterPreviewPlacementOffsets(p,f);
CHECK(f.tileInstances[1].savedPlacementOffsetStatus==0 && f.tileInstances[1].heightOffset==7);
p.romSha256=rom;S9xRemasterApplyProfileToFrame(p,f);S9xRemasterPreviewPlacementOffsets(p,f);
CHECK(f.tileInstances[1].savedPlacementOffsetStatus==2 && f.tileInstances[1].heightOffset==32774);
std::string text;std::vector<RemasterProfileDiagnostic>d;CHECK(S9xRemasterSerializeProfile(p,text,d));RemasterProfile q;CHECK(S9xRemasterParseProfile(text,q,d));CHECK(q.overworldOffsets.size()==p.overworldOffsets.size());for(auto e:p.overworldOffsets)CHECK(q.overworldOffsets.at(e.first)==e.second);
auto base=p,edited=p,disk=p;CHECK(S9xRemasterEditPlacementOffsets(edited,f,{1},-16,false,error));disk.overworldOffsets[dark]=81;
RemasterProfileMergeResult merged;CHECK(S9xRemasterMergeProfiles(base,edited,disk,{},false,merged,d));CHECK(merged.saved.overworldOffsets.at(a)==-16 && merged.saved.overworldOffsets.at(dark)==81);
p.authoredOverworldOffsets=false;CHECK(!S9xRemasterEditPlacementOffsets(p,f,{1},1,true,error));p.authoredOverworldOffsets=true;
f.area.id=1;CHECK(!S9xRemasterEditPlacementOffsets(p,f,{1},1,true,error));f.area.id=320;
f.tileInstances[0].savedPlacementOffsetStatus=0;f.tileInstances[0].savedPlacementCoordinateVerified=false;CHECK(!S9xRemasterEditPlacementOffsets(p,f,{1},1,true,error));
std::puts("PASS placement offset transactions: exact coords/dedup/worlds/mixed +/-/set/atomic bounds/strict input/preview/save/merge/guards");}
