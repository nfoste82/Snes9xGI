#include "../runtime/placement_offset_debug.h"
#include <cstdio>
#include <cstdlib>
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); std::abort(); } } while(0)
int main(int argc,char **argv) {
 RemasterFrame f; f.width=16; f.height=8; f.area.id=320;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 f.originalRgb555.resize(128); f.mainPixels.resize(128); f.subPixels.resize(128);
 RemasterFrameTileInstance a; a.tileId={123,1,4}; a.source=RemasterSourceType::Background; a.sourceIndex=1;
 a.savedPlacementOffset=-40; a.savedPlacementOffsetStatus=2; a.heightOffset=99;
 f.tileInstances={a,a}; f.tileInstances[1].savedPlacementOffset=24;
 f.contextLocations[1]={320,200,201,1}; f.contextLocations[2]={320,201,201,1};
 for(size_t n=0;n<128;n++) { auto &p=f.mainPixels[n]; p.owner=(uint32_t(RemasterSourceType::Background)<<24)|(1u<<16); p.instanceId=n%16<8?1:2; p.tilePixel=(n/16)*8+n%8; }
 std::vector<float> field; S9xRemasterPlacementOffsetField(f,false,field);
 for(size_t n=0;n<128;n++) { CHECK(field[n*2]==(n%16<8?-40:24)); CHECK(field[n*2+1]==2); }
 f.tileInstances[0].hasPlacementHeight=true; f.tileInstances[0].placementHeight.fill(7);
 std::vector<float> other; S9xRemasterPlacementOffsetField(f,false,other); CHECK(other==field);
 f.tileInstances[0].savedPlacementOffset=0; f.tileInstances[0].savedPlacementOffsetStatus=1;
 S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[0]==0 && other[1]==1);
 std::vector<uint8_t> bytes; CHECK(S9xSerializeRemasterFrame(f,bytes)); RemasterFrame restored;
 CHECK(S9xDeserializeRemasterFrame(bytes,restored)); CHECK(restored.tileInstances[0].savedPlacementOffsetStatus==1);
 CHECK(restored.tileInstances[1].savedPlacementOffset==24);
 // Truncation and malformed scalar/status are never silently accepted.
 auto truncated=bytes; truncated.resize(truncated.size()/2); CHECK(!S9xDeserializeRemasterFrame(truncated,restored));
 f.tileInstances[0].savedPlacementOffsetStatus=3; f.tileInstances[0].savedPlacementOffset=-17;
 S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[0]==-17 && other[1]==3);
 CHECK(S9xRemasterSavedPlacementDescription(f,1).find("(calculated)")!=std::string::npos);
 CHECK(S9xSerializeRemasterFrame(f,bytes)); CHECK(S9xDeserializeRemasterFrame(bytes,restored));
 CHECK(restored.tileInstances[0].savedPlacementOffset==-17 && restored.tileInstances[0].savedPlacementOffsetStatus==3);
 f.schemaVersion=44; CHECK(!S9xSerializeRemasterFrame(f,bytes));
 f.schemaVersion=REMASTER_FRAME_SCHEMA_VERSION;
 f.tileInstances[0].savedPlacementOffsetStatus=4; CHECK(!S9xSerializeRemasterFrame(f,bytes));
 f.tileInstances[0].savedPlacementOffsetStatus=1; f.tileInstances[0].savedPlacementOffset=1; CHECK(!S9xSerializeRemasterFrame(f,bytes));
 f.tileInstances[0].savedPlacementOffset=0;
 // Transparent BG1 contributes no visible ownership: visible BG2 in sub/scene
 // survives; opaque BG1 or OBJ is a boundary, not metadata fallback permission.
 f.scenePixels=f.mainPixels; f.sceneRgb555=f.originalRgb555;
 f.subPixels[0]=f.mainPixels[0]; f.mainPixels[0]={};
 S9xRemasterPlacementOffsetField(f,true,other); CHECK(other[1]==1);
 f.mainPixels[0]=f.subPixels[0]; f.mainPixels[0].owner=0x02000000;
 S9xRemasterPlacementOffsetField(f,true,other); CHECK(other[1]==0);
 f.mainPixels[0]=f.subPixels[0]; f.subPixels[0]={};
 // HUD and visible OBJ must not borrow an offset from an underlay.
 auto originalOwner=f.mainPixels[0].owner; f.mainPixels[0].owner=0x02020000;
 S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[1]==0); f.mainPixels[0].owner=originalOwner;
 f.tileInstances[0].source=RemasterSourceType::Object;
 S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[1]==0);
 f.tileInstances[0]=a; f.contextLocations.erase(1);
 S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[1]==0);
 f.area.id=1; S9xRemasterPlacementOffsetField(f,false,other); CHECK(other[1]==0);
 if(argc==2) { RemasterFrame old; CHECK(S9xReadRemasterFrame(argv[1],old)); CHECK(old.schemaVersion<=40);
  for(const auto &i:old.tileInstances)CHECK(i.savedPlacementOffsetStatus==0 && i.savedPlacementOffset==0);
  S9xRemasterPlacementOffsetField(old,true,other); for(size_t n=1;n<other.size();n+=2)CHECK(other[n]==0);
 }
 std::puts("placement offset debug: PASS (signed scalars, local isolation, default, unsupported, capture)");
}
