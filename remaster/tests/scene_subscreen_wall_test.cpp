// Empty cone-free receivers must retain observed physical walls, not lamp masks.
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void check(bool b,const char *s){if(!b){std::cerr<<s<<'\n';std::exit(1);}}
static RemasterFrame fixture() {
 RemasterFrame f;f.width=256;f.height=1;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 RemasterFramePixel empty;empty.owner=uint32_t(RemasterSourceType::Backdrop)<<24;
 f.mainPixels.assign(256,empty);f.scenePixels=f.mainPixels;f.subPixels=f.mainPixels;
 f.originalRgb555.assign(256,123);f.sceneRgb555.assign(256,456);
 RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=0;
 i.tileId={123,1,4};i.hasPaletteArtwork=true;i.heightOffset=24;
 for(int p=0;p<64;p++)i.paletteRgb555[p]=p+1000;
 f.tileInstances.push_back(i);
 RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.tileRole=RemasterTileRole::Wall;
 m.hasHeight=m.hasNormals=true;
 for(int p=0;p<64;p++){m.height[p]=p;m.normalXyz[p*3]=255;m.normalXyz[p*3+1]=128;m.normalXyz[p*3+2]=128;}
 f.assetMetadata.push_back(m);
 f.subPixels[100]={uint32_t(RemasterSourceType::Background)<<24,1,13};
 return f;
}
int main(int argc,char **argv) {
  for(auto role:{RemasterTileRole::Wall,RemasterTileRole::WallCorner,RemasterTileRole::Floor,
    RemasterTileRole::StairTop,RemasterTileRole::StairBottom,RemasterTileRole::Doorway})for(int flips=0;flips<4;flips++) {
   auto f=fixture();f.assetMetadata[0].tileRole=role;auto &i=f.tileInstances[0];i.hFlip=flips&1;i.vFlip=flips&2;
  const auto original=f.originalRgb555;check(S9xRemasterRestoreSceneSubscreenWalls(f)==1,"observed wall restoration");
  check(f.scenePixels[100].instanceId==1 && f.scenePixels[100].tilePixel==13 && f.sceneRgb555[100]==1013,"exact canonical owner/pixel/color");
  check(f.originalRgb555==original && !f.mainPixels[100].instanceId && i.heightOffset==24,"stock and datum preserved");
  check(!S9xRemasterRestoreSceneSubscreenWalls(f),"idempotence");
  RemasterLightingFieldCache fields;fields.update(f,true);
  check(fields.height[201] && fields.authoredNormals[100] && fields.occlusion[201],"effective restored fields");
  check(fields.height[200]==S9xRemasterSignedHeightField(24,13),"relief/base composition");
  std::vector<uint8_t> bytes;check(S9xSerializeRemasterFrame(f,bytes),"serialize repaired ownership");
  RemasterFrame decoded;check(S9xDeserializeRemasterFrame(bytes,decoded) && decoded.scenePixels[100].instanceId==1 && decoded.sceneRgb555[100]==1013,"replay retains consistent scene");
  // A later contextual placement solve may reject the restored occurrence.
  // Receiver restoration must not bypass its absolute-geometry safety gate.
  i.geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;fields.update(f,true);
  check(!fields.height[201] && !fields.authoredNormals[100] && !fields.occlusion[201],"later placement conflict remains inactive");
  }
  // Effective context/placement/flip layers, not hash-level presence, are the
  // receiver contract. These cases deliberately omit the canonical arrays.
  for(int source=0;source<3;source++)for(int flips=0;flips<4;flips++) {
   auto f=fixture();auto &m=f.assetMetadata[0];auto &i=f.tileInstances[0];
   m.tileRole=RemasterTileRole::StairBottom;m.hasHeight=m.hasNormals=false;
   i.hFlip=flips&1;i.vFlip=flips&2;
   if(source==0) {RemasterTileContext c;c.hasHeight=c.hasNormals=true;c.height.fill(-3);c.normalXyz.fill(128);f.contextGeometry[1]=c;}
   if(source==1) {i.hasPlacementHeight=i.hasPlacementNormals=true;i.placementHeight.fill(-3);i.placementNormalXyz.fill(128);}
   if(source==2) {m.geometryByFlip=true;auto &g=m.flipGeometry[S9xRemasterGeometryFlipIndex(i.hFlip,i.vFlip)];g.hasHeight=g.hasNormals=true;g.height.fill(-3);g.normalXyz.fill(128);}
   check(S9xRemasterRestoreSceneSubscreenWalls(f)==1,"effective context/placement/flip stair source");
   RemasterLightingFieldCache fields;fields.update(f,true);
   check(fields.height[201] && fields.height[200]==S9xRemasterSignedHeightField(24,-3),"effective signed stair field");
  }
  // Profile group semantics need not have a matching rendering rule, hence the
  // observed instance's assetGroup can be empty. Explicit conflicting roles win.
  for(auto role:{RemasterTileRole::Unknown,RemasterTileRole::Other,RemasterTileRole::Door}) {
   auto f=fixture();f.assetMetadata[0].tileRole=role;
   std::set<RemasterTileContentId> members{f.tileInstances[0].tileId};
   check(S9xRemasterRestoreSceneSubscreenWalls(f,&members)==(role==RemasterTileRole::Unknown?1:0),"canonical group activation/explicit rejection");
  }
  for(int mode=0;mode<14;mode++) {
  auto f=fixture();auto &i=f.tileInstances[0];auto &m=f.assetMetadata[0];
  switch(mode) {
   case 0:m.tileRole=RemasterTileRole::Unknown;break; // lamp/color-math art
   case 1:m.hasHeight=false;break;
   case 2:m.hasNormals=false;break;
   case 3:i.geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;break;
   case 4:i.geometryStatus=RemasterOccurrenceGeometryStatus::Conflict;break;
   case 5:i.hasPaletteArtwork=false;break;
   case 6:f.scenePixels[100]=f.subPixels[100];break; // existing scene wins
   case 7:f.mainPixels[100]={uint32_t(RemasterSourceType::Background)<<24|2<<16,1,13};break; // HUD
   case 8:f.mainPixels[100]=f.subPixels[100];break; // never replace main receiver
   case 9:i.sourceIndex=1;break;
   case 10:f.subPixels[100].tilePixel=64;break;
   case 11:f.profileRomSha256="unsupported";break;
   case 12:i.geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;i.placementTileRole=RemasterTileRole::Other;break;
   case 13: {RemasterTileContext c;c.hasHeight=false;c.hasNormals=false;f.contextGeometry[1]=c;break;}
  }
  const auto color=f.sceneRgb555;const auto id=f.scenePixels[100].instanceId;
   if(mode==1 || mode==2 || mode==7) {
    check(S9xRemasterRestoreSceneSubscreenWalls(f)==1,"independent channel/HUD admission");
   } else check(!S9xRemasterRestoreSceneSubscreenWalls(f) && color==f.sceneRgb555 && id==f.scenePixels[100].instanceId,"negative eligibility/priority");
 }
 for(int a=1;a<argc;a++) {
  auto f=std::make_unique<RemasterFrame>();check(S9xReadRemasterFrame(argv[a],*f),"capture read");
  RemasterLightingFieldCache fields;fields.update(*f,true);
  size_t n=0;
  for(size_t k=0;k<f->scenePixels.size();k++) {
   const auto &p=f->scenePixels[k];if(!p.instanceId)continue;
   const auto &i=f->tileInstances[p.instanceId-1];
   if(i.source!=RemasterSourceType::Background || i.sourceIndex!=0)continue;
   check(fields.height[k*2+1] && fields.authoredNormals[k] && fields.occlusion[k*2+1],"capture restored fields");
   check(f->sceneRgb555[k]==i.paletteRgb555[p.tilePixel],"capture restored color");n++;
  }
  std::cout<<argv[a]<<" restored_bg1_pixels="<<n<<'\n';
 }
 std::cout<<"scene subscreen wall ownership/color/fields regression PASS\n";
}
