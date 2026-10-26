// Own-profile EndFrame replay; per-pixel effective source inventory for BOTH
// presentation and physical scene buffers. No coordinates affect production.
#define main captureFixtureMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv) { try {
 check(captureFixtureMain(argc,argv)==0,"exact replay");
 RemasterFrame f;check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",f),"output packet");
 const auto bytes=read(std::string(argv[1])+".dungeon.json");const std::string text(bytes.begin(),bytes.end());
 auto pos=text.find("\"scroll\":");pos=text.find('[',pos);int sx=std::atoi(text.c_str()+pos+1);pos=text.find(',',pos);int sy=std::atoi(text.c_str()+pos+1);
 std::ofstream o(std::string(argv[2])+".sources.jsonl");
 for(int which=0;which<3;which++) {const auto &pixels=which==0?f.mainPixels:which==1?f.subPixels:f.scenePixels;
  for(size_t at=0;at<pixels.size();at++) {const auto &px=pixels[at];if(!px.instanceId)continue;
   const auto &i=f.tileInstances.at(px.instanceId-1);if(i.source!=RemasterSourceType::Background || i.sourceIndex>1)continue;
   const auto *m=S9xRemasterFrameMetadataForTile(f,i.tileId);
   const auto g=S9xRemasterResolveFrameGeometry(m?*m:RemasterFrameAssetMetadata{},f,px.instanceId-1);
   const int k=px.tilePixel;float nx=(int(g.normalXyz[k*3])-128)/127.f,ny=(int(g.normalXyz[k*3+1])-128)/127.f,nz=(int(g.normalXyz[k*3+2])-128)/127.f;
   S9xRemasterTransformNormalForTileInstance(i,nx,ny,nz,g.normalIgnoreTileTransform);
   o<<"{\"buffer\":"<<which<<",\"bg\":"<<int(i.sourceIndex)+1<<",\"x\":"<<((sx+at%f.width)&511)/8<<",\"y\":"<<((sy+at/f.width)&511)/8
    <<",\"pixel\":"<<k<<",\"hash\":\""<<std::hex<<i.tileId.hash<<std::dec<<"\",\"hflip\":"<<i.hFlip<<",\"vflip\":"<<i.vFlip
    <<",\"group\":\""<<i.assetGroup<<"\",\"role\":"<<int(g.tileRole)<<",\"base\":"<<i.heightOffset<<",\"placement\":"<<i.hasPlacementHeight<<",\"context\":"<<bool(f.contextGeometry.count(px.instanceId))
    <<",\"height\":"<<g.hasHeight<<",\"local\":"<<g.height[k]<<",\"effective\":"<<i.heightOffset+g.height[k]
    <<",\"normals\":"<<g.hasNormals<<",\"normal\":["<<nx<<','<<ny<<','<<nz<<"]}\n";
  }
 }
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} }
