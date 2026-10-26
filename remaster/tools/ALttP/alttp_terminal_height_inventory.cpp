// Detection-only terminal consumer. Same input convention as runtime-room-heights.
// Emits canonical-indexed local/actual samples for every placement on both BGs.
#include "../../runtime/remaster.h"
#include "../../../sha256.cpp"
#include <iostream>
#include <stdexcept>
static std::vector<uint8_t> read(const char *p) {
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("cannot read input");
 return {std::istreambuf_iterator<char>(f),{}};
}
int main(int argc,char **argv) { try {
 if(argc<9 || std::string(argv[argc-2])!="--rom")throw std::runtime_error("snapshot attr profile x y plane [stairX stairY highNorth changesPlane]... --rom ROM");
 auto rom=read(argv[argc-1]);argc-=2;
 if((argc-7)%4)throw std::runtime_error("incomplete stair arguments");
 unsigned char sha[32];sha256sum(rom.data(),static_cast<unsigned>(rom.size()),sha);
 const unsigned char expected[]={0x66,0x87,0x1d,0x66,0xbe,0x19,0xad,0x2c,0x34,0xc9,0x27,0xd6,0xb1,0x4c,0xd8,0xeb,0x6f,0xc3,0x18,0x19,0x65,0xb6,0xe5,0x17,0xcb,0x36,0x1f,0x73,0x16,0x00,0x9c,0xfb};
 if(rom.size()!=1048576 || std::memcmp(sha,expected,32))throw std::runtime_error("unsupported ROM fingerprint");
 auto raw=read(argv[1]),attr=read(argv[2]);const size_t offset=14+224*(30+256*6);
 if(raw.size()!=offset+65536+512 || std::memcmp(raw.data(),"ALTPPD1\0",8) || attr.size()!=8202 || std::memcmp(attr.data(),"ALTPAT1\0",8))throw std::runtime_error("invalid complete inputs");
 auto word=[&](size_t i){return raw[i]|raw[i+1]<<8;};
 if((attr[8]|attr[9]<<8)!=word(8))throw std::runtime_error("attribute room mismatch");
 RemasterProfile p;std::vector<RemasterProfileDiagnostic> errors;
 if(!S9xRemasterLoadProfile(argv[3],p,errors))throw std::runtime_error("invalid profile");
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=word(8);
 c.linkX=std::stoi(argv[4]);c.linkY=std::stoi(argv[5]);c.linkPlane=std::stoi(argv[6]);
 c.collisionAttributes=attr.data()+10;c.graphics=raw.data()+offset;
 c.graphicsBase=word(30)*2;c.bg1GraphicsBase=word(20)*2;c.dungeonTemplateData=rom.data()+0x1b52;
 if((word(22)&3)!=3 || (word(32)&3)!=3)throw std::runtime_error("requires two complete 64x64 maps");
 std::array<uint8_t,16384> maps={};
 for(int l=0;l<2;l++)for(int y=0;y<64;y++)for(int x=0;x<64;x++){
  int a=(word(18+l*10)+y/32*2048+x/32*1024+y%32*32+x%32)&32767;
  int t=(l?0:8192)+(y*64+x)*2;maps[t]=c.graphics[a*2];maps[t+1]=c.graphics[a*2+1];}
 c.constructedMaps=maps.data();
 for(int i=7;i<argc;i+=4){if(c.stairCount==32)throw std::runtime_error("too many stairs");auto &s=c.stairs[c.stairCount++];s.x=std::stoi(argv[i]);s.y=std::stoi(argv[i+1]);s.highIsNorth=std::stoi(argv[i+2]);s.changesPlane=std::stoi(argv[i+3]);}
 S9xRemasterDungeonStairsFromAttributes(c);
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto lower=std::make_unique<RemasterDungeonHeightMap>();
 for(int bg=0;bg<2;bg++){
  std::rotate(maps.begin(),maps.begin()+8192,maps.end());std::swap(c.graphicsBase,c.bg1GraphicsBase);c.placementBackgroundIndex=bg;
  if(bg){c.supportRelations=&lower->authoredSupportRelations;c.wallSupportRelations=&lower->authoredWallSupportRelations;c.doorwaySupportRelations=&lower->doorwaySupportRelations;}
  S9xRemasterDungeonPlacements(c,p,*tiles);c.placements=tiles->data();auto m=std::make_unique<RemasterDungeonHeightMap>();
  if(!S9xRemasterBuildDungeonHeightMap(c,p.upperFloorHeight,*m))throw std::runtime_error("unsupported room solve");
  for(int i=0;i<4096;i++){
   const auto &t=(*tiles)[i];RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
   RemasterFrameTileInstance inst;inst.source=RemasterSourceType::Background;inst.sourceIndex=bg;inst.tileId={t.tileHash,1,4};inst.hFlip=t.hflip;inst.vFlip=t.vflip;f.tileInstances.push_back(inst);
   RemasterFrameAssetMetadata md;md.tileId=inst.tileId;md.hasHeight=t.hasHeight;md.height=t.height;md.doorframeMask=t.doorframeMask;md.hasDoorframeMask=t.hasDoorframeMask;
    // The classifier's t.height may already be a derived seven-band shell.
    // Production frames start from immutable profile metadata and receive that
    // derived source ONLY if ApplyDungeonHeightMap actually transports it.
    // Preloading the derived array here hides missing localHeightPreviews.
    auto asset=p.assets.find(inst.tileId);
    md.hasHeight=false;md.height.fill(0);
    if(asset!=p.assets.end()) {
     const auto &a=asset->second;md.tileRole=a.tileRole;md.hasHeight=a.hasHeight;md.height=a.height;
     md.geometryByFlip=a.geometryByFlip;md.flipGeometry=a.flipGeometry;
     md.geometryByFacing=a.geometryByFacing;md.facingGeometry=a.facingGeometry;
    }
    f.assetMetadata.push_back(md);
   for(int k=0;k<64;k++){f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
   auto apply=c;apply.backgroundScrollX=i%64*8;apply.backgroundScrollY=i/64*8;S9xRemasterApplyDungeonHeightMap(f,apply,*m,nullptr,nullptr,bg);
   std::cout<<"{\"bg\":"<<bg+1<<",\"x\":"<<i%64<<",\"y\":"<<i/64<<",\"hash\":\""<<std::hex<<t.tileHash<<std::dec<<"\",\"local\":[";
   for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<t.height[k];}
   std::cout<<"],\"actual\":[";
    for(int k=0;k<64;k++){if(k)std::cout<<',';const auto index=f.mainPixels[k].instanceId-1;const auto &a=f.tileInstances[index];const auto g=S9xRemasterResolveFrameGeometry(md,f,index);std::cout<<a.heightOffset+g.height[k];}
    const auto selected=S9xRemasterResolveFrameGeometry(md,f,f.mainPixels[0].instanceId-1);
    const auto &applied=f.tileInstances[f.mainPixels[0].instanceId-1];
    std::cout<<"],\"has_effective_height\":"<<selected.hasHeight
     <<",\"asset_has_height\":"<<md.hasHeight<<",\"placement_height\":"<<applied.hasPlacementHeight
     <<",\"context_height\":"<<f.contextGeometry.count(f.mainPixels[0].instanceId)
     <<",\"base\":"<<applied.heightOffset<<",\"geometry_status\":"<<int(applied.geometryStatus)
     <<",\"plane_bases\":["<<int(m->planeOffsets[0][i])<<','<<int(m->planeOffsets[1][i])<<"]}\n";
  }
  if(!bg)lower=std::move(m);
 }
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 2;}}
