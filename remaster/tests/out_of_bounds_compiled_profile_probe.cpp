// Real compiler output -> production parser -> effective frame channels.
#include "../runtime/lighting_field_cache.h"
#include <cstdlib>
#include <iostream>
static void require(bool b){if(!b)std::exit(1);}
int main(int argc,char **argv) {
 require(argc==2);RemasterProfile p;std::vector<RemasterProfileDiagnostic> d;
 if(!S9xRemasterLoadProfile(argv[1],p,d)) {
  for(const auto &e:d)std::cerr<<e.line<<": "<<e.message<<'\n';return 1;
 }
 require(p.assets.size()==6);RemasterFrame f;f.width=6;f.height=1;
 f.originalRgb555.resize(6);f.mainPixels.resize(6);f.subPixels.resize(6);
 for(int n=1;n<=6;n++) {
  RemasterTileContentId id{uint64_t(n),1,4};const auto &a=p.assets.at(id);
  require(a.tileRole==RemasterTileRole::OutOfBounds);
  require(a.hasHeight==(n>=2 && n<=4));require(a.hasNormals==(n==5));require(a.hasOcclusion==(n==6));
  if(n==2)require(a.height==std::array<int16_t,64>{});
  if(n==3)for(int h:a.height)require(h==5);
  if(n==4)for(int k=0;k<64;k++)require(a.height[k]==k);
  RemasterFrameTileInstance i;i.tileId=id;i.source=RemasterSourceType::Background;f.tileInstances.push_back(i);
  f.mainPixels[n-1]={uint32_t(RemasterSourceType::Background)<<24,uint32_t(n),0};
 }
 S9xRemasterApplyProfileToFrame(p,f);RemasterFrameChannelResolver resolver(f);
 for(int n=0;n<6;n++) {
  const auto &m=resolver.effective[n];require(m.tileRole==RemasterTileRole::OutOfBounds);
  require(m.hasHeight==(n>=1 && n<=3));require(m.hasNormals==(n==4));require(m.hasOcclusion==(n==5));
 }
 RemasterLightingFieldCache fields;fields.update(f,false);
 for(int n=0;n<6;n++)require(!fields.height[n*2+1]);
 std::vector<uint8_t> bytes;RemasterFrame replay;
 require(S9xSerializeRemasterFrame(f,bytes) && S9xDeserializeRemasterFrame(bytes,replay));
 fields.update(replay,false);for(int n=0;n<6;n++)require(!fields.height[n*2+1]);
 std::cout<<"PASS compiled OOB: 6 assets parser/frame/channel-presence/unknown/capture\n";
}
