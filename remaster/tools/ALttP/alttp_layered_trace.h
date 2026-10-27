// Detection-only trace callback. Include after remaster.h in a capture probe.
// No layer substitution: retain independent source/channel identities.
#include <iostream>
inline const RemasterDungeonFloorContext *alttpTraceContext=nullptr;
inline void alttpLayeredTrace(const RemasterFrame &f,const char *stage) {
 if(!alttpTraceContext)return;const auto &c=*alttpTraceContext;
 RemasterLightingFieldCache cpu;if(!f.scenePixels.empty())cpu.update(f,true,true);
 for(size_t at=0;at<f.mainPixels.size();at++) {
  const int x=(c.backgroundScrollX+at%f.width)&511,y=(c.backgroundScrollY+at/f.width)&511;
  std::cout<<"{\"stage\":\""<<stage<<"\",\"sx\":"<<at%f.width<<",\"sy\":"<<at/f.width<<",\"x\":"<<x<<",\"y\":"<<y;
  for(int bg=0;bg<2;bg++) {
   const int a=(bg?0:8192)+2*(y/8*64+x/8),w=c.constructedMaps[a]|c.constructedMaps[a+1]<<8;
   const int base=bg?c.graphicsBase:(c.bg1GraphicsBase==0xffff?c.graphicsBase:c.bg1GraphicsBase);
   int xx=w&0x4000?7-x%8:x%8,yy=w&0x8000?7-y%8:y%8,ink=0;
   for(int b=0;b<4;b++)ink|=((c.graphics[uint16_t(base+(w&1023)*32+b/2*16+yy*2+b%2)]>>(7-xx))&1)<<b;
   std::cout<<",\"bg"<<bg+1<<"_word\":"<<w<<",\"bg"<<bg+1<<"_ink\":"<<ink;
  }
  for(int b=0;b<3;b++) {
   const auto &v=b==0?f.mainPixels:b==1?f.subPixels:f.scenePixels;
   std::cout<<",\""<<(b==0?"main":b==1?"sub":"scene")<<"\":{";
   if(at<v.size()) {const auto &p=v[at];std::cout<<"\"owner\":"<<p.owner<<",\"id\":"<<p.instanceId;
    if(p.instanceId && p.instanceId<=f.tileInstances.size() && p.tilePixel<64) {
     const auto &i=f.tileInstances[p.instanceId-1];const auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId);
     const auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},f,p.instanceId-1);int k=p.tilePixel;
     std::cout<<",\"bg\":"<<(i.source==RemasterSourceType::Background?int(i.sourceIndex)+1:0)<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"k\":"<<k<<",\"base\":"<<i.heightOffset<<",\"status\":"<<int(i.geometryStatus)<<",\"role\":"<<int(g.tileRole)
      <<",\"asset_height\":"<<(m&&m->hasHeight)<<",\"asset_normals\":"<<(m&&m->hasNormals)<<",\"asset_occlusion\":"<<(m&&m->hasOcclusion)<<",\"placement_height\":"<<i.hasPlacementHeight<<",\"placement_normals\":"<<i.hasPlacementNormals<<",\"placement_occlusion\":"<<i.hasPlacementOcclusion<<",\"context\":"<<f.contextGeometry.count(p.instanceId)
      <<",\"has_height\":"<<g.hasHeight<<",\"has_normals\":"<<g.hasNormals<<",\"has_occlusion\":"<<g.hasOcclusion<<",\"local\":"<<g.height[k]<<",\"actual\":"<<i.heightOffset+g.height[k]<<",\"normal\":["<<int(g.normalXyz[k*3])<<','<<int(g.normalXyz[k*3+1])<<','<<int(g.normalXyz[k*3+2])<<"],\"occlusion\":"<<int(g.occlusion[k])
      <<",\"has_doorframe_mask\":"<<g.hasDoorframeMask<<",\"doorframe_mask\":"<<((g.doorframeMask>>k)&1)<<",\"has_emission\":"<<g.hasEmission<<",\"has_material_selectors\":"<<g.hasMaterialSelectors<<",\"normal_ignore_transform\":"<<g.normalIgnoreTileTransform<<",\"hflip\":"<<i.hFlip<<",\"vflip\":"<<i.vFlip;
    }
   }std::cout<<'}';
  }
  if(!f.scenePixels.empty())std::cout<<",\"cpu_known\":"<<(cpu.height[at*2+1]>.5)<<",\"cpu_height\":"<<cpu.height[at*2]*255<<",\"cpu_occlusion\":"<<int(cpu.occlusion[at*2])<<",\"cpu_authored_normal\":"<<int(cpu.authoredNormals[at])<<",\"cpu_normal\":["<<cpu.surface[at*4+1]<<','<<cpu.surface[at*4+2]<<','<<cpu.surface[at*4+3]<<"],\"cpu_participation\":["<<int(cpu.participation[at*2])<<','<<int(cpu.participation[at*2+1])<<"]";
  std::cout<<"}\n";
 }
}
