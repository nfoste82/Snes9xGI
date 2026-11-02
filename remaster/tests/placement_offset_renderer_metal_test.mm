// Actual DrawRemasterFrame/slot/upload/kernel/presentation, no app/GUI launch.
#include "../runtime/placement_offset_editor.h"
#define main placementOffsetProbeMain
#include "../tools/lighting_critical_path_probe.mm"
#undef main
int main() { @autoreleasepool { try {
 auto check=[](bool ok,const char *s){if(!ok)throw std::runtime_error(s);};
 metalDevice=MTLCreateSystemDefaultDevice();check(metalDevice,"device");metalCommandQueue=[metalDevice newCommandQueue];
 NSError *error=nil;auto source=[NSString stringWithContentsOfFile:@"macosx/shaders.metal" encoding:NSUTF8StringEncoding error:&error];
 auto library=[metalDevice newLibraryWithSource:source options:nil error:&error];check(library,"library");
 InitializeRemasterMetalPipelines(library,MTLPixelFormatRGBA8Unorm);
 RemasterFrame f;f.width=16;f.height=8;f.area.id=320;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 f.originalRgb555.resize(128);f.mainPixels.resize(128);f.subPixels.resize(128);
 RemasterFrameTileInstance i;i.tileId={123,1,4};i.source=RemasterSourceType::Background;i.sourceIndex=1;
 i.savedPlacementOffset=-40;i.savedPlacementOffsetStatus=2;i.heightOffset=-40;
 f.tileInstances={i,i};f.tileInstances[1].savedPlacementOffset=24;f.tileInstances[1].heightOffset=24;
 f.contextLocations[1]={320,200,201,1};f.contextLocations[2]={320,201,201,1};
 RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.hasHeight=true;m.height.fill(0);f.assetMetadata={m};
 for(unsigned n=0;n<128;n++) {auto &p=f.mainPixels[n];p.owner=0x02010000;p.instanceId=n%16<8?1:2;p.tilePixel=(n/16)*8+n%8;}
 metalLayer=[CAMetalLayer layer];metalLayer.bounds=CGRectMake(0,0,16,8);
 presentationCallbackGate=std::make_shared<RemasterPresentationCallbackGate>();
 auto draw=[&](RemasterLightingView view) {
  check(DrawRemasterFrame(f,RemasterDebugMode::Original,nullptr,true,view,false,false),"DrawRemasterFrame");DrainRemasterPresentation();
  auto &r=remasterResources[debugLastResourceSlot];auto tex=view==RemasterLightingView::HeightOffsets?r.savedOffsetsOutput:r.output;
  check(tex,"output");std::vector<uint8_t> b(128*4);[tex getBytes:b.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,8) mipmapLevel:0];return b;
 };
 SetRemasterHeightPreviewRange(-128,128);
 auto heightBefore=draw(RemasterLightingView::Height);
 auto offsets=draw(RemasterLightingView::HeightOffsets);
 for(unsigned n=0;n<128;n++)check(std::abs(int(offsets[n*4])-(n%16<8?88:151))<=1,"scalar output");
 auto heightAfter=draw(RemasterLightingView::Height);check(heightBefore==heightAfter,"Height view changed after offset view");
 f.assetMetadata[0].height.fill(7);auto changed=draw(RemasterLightingView::HeightOffsets);check(offsets==changed,"local height leaked");
  f.tileInstances[0].savedPlacementOffsetStatus=0;changed=draw(RemasterLightingView::HeightOffsets);check(changed[0]==56 && changed[1]==0 && changed[2]==71,"unsupported");
  f.tileInstances[0].savedPlacementOffsetStatus=2;
  RemasterProfile profile;profile.romSha256=f.profileRomSha256;profile.authoredOverworldOffsets=true;
  profile.overworldOffsets[f.contextLocations[1]]=-40;profile.overworldOffsets[f.contextLocations[2]]=24;
  const auto evidence=f;
  for(bool enabled:{true,false,true}) {
   profile.authoredOverworldOffsets=enabled;S9xRemasterApplyProfileToFrame(profile,f);S9xRemasterPreviewPlacementOffsets(profile,f);
   auto image=draw(RemasterLightingView::HeightOffsets);
   check(enabled?image[0]==88:(image[0]==56 && image[1]==0 && image[2]==71),"mode scalar GPU stale");
   check(f.tileInstances[0].heightOffset==(enabled?-40:0),"mode CPU base stale");
   draw(RemasterLightingView::Height);
  }
   check(evidence.tileInstances[0].savedPlacementOffset==-40,"immutable evidence");
   // Actual type click -> coordinate batch -> incremental fields -> GPU scalar
   // and final highlight. Same type across flips, own signed offsets retained.
   f.tileInstances[1].hFlip=true;
   std::set<RemasterTileContentId> types;std::set<uint32_t> selected;
   check(S9xRemasterPickPlacementOffset(f,8,false,selected,&types)==2 && selected.size()==2,"type expansion");
   f.placementOffsetSelection=true;f.contextLocationSelection=true;f.selectedContextInstances=selected;
   std::string editError;
   check(S9xRemasterEditPlacementOffsets(profile,f,selected,1,true,editError),"type transaction");
   S9xRemasterPreviewPlacementOffsets(profile,f);
   auto batchScalar=draw(RemasterLightingView::HeightOffsets);
   std::vector<uint8_t> batchHighlight(128*4);
   [debugLastPresentationTexture getBytes:batchHighlight.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,8) mipmapLevel:0];
   for(unsigned n=0;n<128;++n) {
    check(std::abs(int(batchScalar[n*4])-(n%16<8?89:152))<=1,"type own signed scalar GPU");
    check(batchHighlight[n*4]!=batchScalar[n*4] || batchHighlight[n*4+1]!=batchScalar[n*4+1] || batchHighlight[n*4+2]!=batchScalar[n*4+2],"type visible highlight");
   }
   check(S9xRemasterPickPlacementOffset(f,8,true,selected,&types)==0 && selected.empty(),"type deselect");
   f.selectedContextInstances=selected;draw(RemasterLightingView::HeightOffsets);
   [debugLastPresentationTexture getBytes:batchHighlight.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,8) mipmapLevel:0];
   check(batchHighlight==batchScalar,"type deselect highlight leak");
   f.tileInstances[1].hFlip=false;
  // Synthetic scene alias: stock ID1 vs scene ID2 with same actual placement.
  // Only offset-category highlight changes selector; legacy categories unchanged.
  f.scenePixels=f.mainPixels;f.sceneRgb555=f.originalRgb555;
  for(unsigned n=0;n<128;n++)if(n%16<8)f.scenePixels[n].instanceId=2;
  f.placementOffsetSelection=true;f.contextLocationSelection=true;f.selectedContextInstances={2};
  auto scalar=draw(RemasterLightingView::HeightOffsets);
  std::vector<uint8_t> highlighted(128*4);
  [debugLastPresentationTexture getBytes:highlighted.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,8) mipmapLevel:0];
  check(highlighted!=scalar,"post-highlight presentation missing selected scene alias");
  f.selectedContextInstances={1};draw(RemasterLightingView::HeightOffsets);
  [debugLastPresentationTexture getBytes:highlighted.data() bytesPerRow:64 fromRegion:MTLRegionMake2D(0,0,16,8) mipmapLevel:0];
  check(highlighted==scalar,"deselected stock alias incorrectly highlighted");
 std::cout<<"PASS actual production DrawRemasterFrame: signed scalar upload/readback, local0/7 identical, Height switchback exact, unsupported mask\n";
 return 0;
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;} } }
