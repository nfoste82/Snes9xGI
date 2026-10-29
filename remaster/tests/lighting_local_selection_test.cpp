#include "lighting_field_cache_step1_reference.h"
#ifdef REMASTER_LOCAL_BASELINE
using RemasterLightingFieldCache = RemasterLightingFieldCacheStep1Reference;
#else
#include "../runtime/lighting_field_cache.h"
#endif
#include "../runtime/surface_mesh_cache.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <random>
static int testMutation=-1,testEvent=-1;
static void need(bool b,const char*s){if(!b){std::cerr<<"FAIL "<<s<<" mutation="<<testMutation<<" event="<<testEvent<<'\n';std::exit(1);}}
static bool pixel(const RemasterFramePixel&a,const RemasterFramePixel&b){return a.owner==b.owner&&a.instanceId==b.instanceId&&a.tilePixel==b.tilePixel;}
template<class A,class B> static void equal(const A&a,const B&b){
 need(a.occlusion==b.occlusion&&a.emission==b.emission&&a.participation==b.participation&&a.opposite==b.opposite&&a.authoredNormals==b.authoredNormals&&a.semanticNormals==b.semanticNormals&&a.height==b.height&&a.surface==b.surface&&a.reflectance==b.reflectance&&a.depths==b.depths&&a.omnidirectional==b.omnidirectional,"complete field arrays");
 need(a.visualClaims==b.visualClaims&&a.channelSources.size()==b.channelSources.size(),"claims and selection sizes");
 for(size_t i=0;i<a.channelSources.size();++i){need(pixel(a.channelSources[i].visible,b.channelSources[i].visible),"visible triple");for(size_t c=0;c<7;++c)need(pixel(a.channelSources[i].donors[c],b.channelSources[i].donors[c]),"donor triple");
 const auto&x=a.samples[i];const auto&y=b.samples[i];need(x.height==y.height&&x.coverage==y.coverage&&x.domain==y.domain&&x.known==y.known&&x.sheet==y.sheet&&x.solidWall==y.solidWall&&x.wallBase==y.wallBase&&x.fallbackGround==y.fallbackGround,"all sample members");}
}
template<class C> static std::vector<RemasterSurfaceMesh::Cell> render(C&c,const RemasterFrame&f,RemasterSurfaceMesh::Cache* retained=nullptr){
 auto mesh=retained?retained->update(f.width,f.height,c.samples,f.lightingCoordinateScale/255.0f):RemasterSurfaceMesh::build(f.width,f.height,c.samples,f.lightingCoordinateScale/255.0f);
 for(size_t i=0;i<mesh.size();++i){mesh[i].emissionDepth=c.depths[i];mesh[i].emissionOmnidirectional=c.omnidirectional[i];if(!c.authoredNormals[i])std::copy_n(mesh[i].normal,3,c.surface.begin()+i*4+1);}
 return mesh;
}
static void meshes(const std::vector<RemasterSurfaceMesh::Cell>&a,const std::vector<RemasterSurfaceMesh::Cell>&b){
 static_assert(sizeof(RemasterSurfaceMesh::Cell)==48,"cell contains twelve floats, no padding");
 need(a.size()==b.size()&&(!a.size()||!std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))),"mesh bytes");
}
static RemasterFrame fixture(int bg,int hv){
 RemasterFrame f;f.width=32;f.height=8;f.lightingCoordinateScale=255;f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 f.originalRgb555.resize(32*8);f.sceneRgb555=f.originalRgb555;
 for(int j=0;j<4;++j){RemasterFrameTileInstance in;in.tileId={uint64_t(10+j),1,4};in.source=j==3?RemasterSourceType::Object:RemasterSourceType::Background;in.sourceIndex=j==2?2:(j^bg)&1;in.hFlip=hv&1;in.vFlip=hv&2;f.tileInstances.push_back(in);RemasterFrameAssetMetadata m;m.tileId=in.tileId;m.hasHeight=m.hasOcclusion=j==1;m.height.fill(0);m.occlusion.fill(0);f.assetMetadata.push_back(m);}
 for(size_t k=0;k<f.originalRgb555.size();++k){RemasterFramePixel a{uint32_t(RemasterSourceType::Background)<<24|uint32_t(bg)<<16,1,uint8_t(k%64)},b{uint32_t(RemasterSourceType::Background)<<24|uint32_t(1-bg)<<16,2,uint8_t(k%64)};
 f.mainPixels.push_back(a);f.scenePixels.push_back(a);f.subPixels.push_back(k<64?b:RemasterFramePixel{});for(int r=0;r<4;++r){f.observedBgPixels[r].push_back(k<64?b:RemasterFramePixel{});f.observedBgDepth[r].push_back(20+r);}}
 return f;
}
static void mutate(RemasterFrame&f,int m){
 const size_t n=f.mainPixels.size();auto &in=f.tileInstances[1];auto &a=f.assetMetadata[1];
 if(m<7){auto *r=m==0?&f.mainPixels:m==1?&f.subPixels:m==2?&f.scenePixels:&f.observedBgPixels[m-3];(*r)[0]={};}
 else if(m<11)++f.observedBgDepth[m-7][0];
 else switch(m){
 case 11:a.hasHeight=!a.hasHeight;break;case 12:a.height[0]=13;break;case 13:a.hasEmission=!a.hasEmission;break;
 case 14:a.hasNormals=true;a.normalXyz.fill(180);break;case 15:a.hasOcclusion=false;break;
 case 16:in.heightOffset+=32;break;case 17:in.hFlip=!in.hFlip;break;case 18:in.vFlip=!in.vFlip;break;
 case 19:in.geometryStatus=RemasterOccurrenceGeometryStatus::Unresolved;break;
 case 20:f.contextGeometry[2].hasHeight=true;f.contextGeometry[2].height.fill(-12);break;
 case 21:f.contextGeometry.clear();break;case 22:in.hasPlacementHeight=true;in.placementHeight.fill(23);break;
 case 23:a.geometryByFlip=true;a.flipGeometry[3].hasHeight=true;break;
 case 24:a.hasDoorframeMask=true;a.doorframeMask=0;break;
 case 25:in.visualClaim.mask=1;in.visualClaim.published=true;break;
 case 26:in.visualClaim.status=RemasterVisualClaimStatus::Conflict;break;
 case 27:a.tileRole=RemasterTileRole::OutOfBounds;break;
 case 28:f.assetMetadata.erase(f.assetMetadata.begin()+1);break;
 case 29:{auto copy=a;copy.hasHeight=false;f.assetMetadata.push_back(copy);break;}
 case 30:f.originalRgb555[0]^=42;break;case 31:in.hasPaletteArtwork=true;in.paletteRgb555.fill(15);break;
 case 32:a.hasEmission=true;a.emissionRgba.fill(255);a.emissionColorMask=0;break;
 case 33:a.emissionDepth+=7;a.emissionOmnidirectional=!a.emissionOmnidirectional;break;
 case 34:in.tileId=f.tileInstances[0].tileId;break;
 case 35:f.tileInstances.push_back(in);break;case 36:f.tileInstances.pop_back();break;
 case 37:f.width=16;f.height=16;break;case 38:f.lightingCoordinateScale=33;break;
 case 39:f.area.id=320;break;case 40:f.profileRomSha256="other";break;
 case 41:f.observedBgPixels[3].clear();break;case 42:f.observedBgDepth[3].clear();break;
 case 43:{RemasterFrameMaterial mat;mat.name="ui";mat.surfaceClass=RemasterSurfaceClass::UserInterface;f.materials.push_back(mat);a.hasMaterialSelectors=true;a.materialSelectors.fill("ui");break;}
 case 44:f.mainPixels[0]={uint32_t(RemasterSourceType::Background)<<24|2<<16,3,0};break;
 case 45:f.mainPixels[0]={uint32_t(RemasterSourceType::Object)<<24,4,0};break;
 case 46:f.subPixels[0].instanceId=999;break;case 47:f.subPixels[0].tilePixel=64;break;
 case 48:in.normalYaw++;break;case 49:in.hasPlacementNormals=true;in.placementNormalXyz.fill(128);break;
 case 50:a.hasMaterialSelectors=true;a.materialSelectors.fill("");break;
 case 51:in.material="ui";break;
 default:f.scenePixels[n-1]={};break;
 }
}
static void proof(){
 {auto f=fixture(0,0);RemasterLightingFieldCache c;c.update(f,true);f.tileInstances[1].heightOffset=12;c.update(f,true);
 std::cout<<"local selected="<<c.selectedPixels<<" full="<<f.mainPixels.size()<<std::endl;
 need(c.selectedPixels<f.mainPixels.size(),"local change must select less than full (baseline failure)");}
 for(int bg=0;bg<2;++bg)for(int hv=0;hv<4;++hv)for(bool scene:{false,true}){
  const auto base=fixture(bg,hv);
  for(int m=0;m<53;++m){auto f=base;RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;RemasterSurfaceMesh::Cache cmCache,rmCache;c.update(f,scene);ref.update(f,scene);render(c,f,&cmCache);render(ref,f,&rmCache);mutate(f,m);
   for(int event=0;event<3;++event){testMutation=m;testEvent=event;c.update(f,scene);ref.update(f,scene);RemasterLightingFieldCache fresh;fresh.update(f,scene);auto cm=render(c,f,&cmCache),rm=render(ref,f,&rmCache),fm=render(fresh,f);
    // Independent fresh consumer exposes a preexisting frozen-reference bug:
    // last-match field production was snapshotted with first-match emplace.
    // Only the duplicate-removal switchback is excluded from warmed parity.
    if(!(m==29&&event>0)){equal(c,ref);meshes(cm,rm);}else need(ref.channelSources[0].donors[0].instanceId==0&&fresh.channelSources[0].donors[0].instanceId==2,"named duplicate switchback negative control: frozen absent vs fresh ID2 authored zero");equal(c,fresh);meshes(cm,fm);f=base;}
  }
 }
 auto f=fixture(0,0);RemasterLightingFieldCache c;c.update(f,true);const auto untouched=c.channelSources[200];f.tileInstances[1].heightOffset=12;c.update(f,true);
 std::cout<<"local selected="<<c.selectedPixels<<" full="<<f.mainPixels.size()<<std::endl;
 need(c.selectedPixels<f.mainPixels.size(),"local change must select less than full (baseline failure)");need(pixel(untouched.visible,c.channelSources[200].visible),"off-target visible unchanged");for(size_t k=0;k<7;++k)need(pixel(untouched.donors[k],c.channelSources[200].donors[k]),"off-target donors unchanged");
 std::mt19937 rng(0x55);RemasterLightingFieldCacheStep1Reference ref;ref.update(f,true);
 for(int k=0;k<150;++k){int m=rng()%28;mutate(f,m);c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);auto cm=render(c,f),rm=render(ref,f),fm=render(fresh,f);equal(c,ref);equal(c,fresh);meshes(cm,rm);meshes(cm,fm);}
 std::cout<<"PASS local selection complete fields/donors/claims/samples/renderer normals/mesh\n";
}
static void edges(){
 // Active material-based HUD predicate requires width==256. Keep original
 // FIRST metadata distinct from LAST field metadata and test both directions.
 for(int mode=0;mode<4;++mode){auto f=fixture(0,0);f.width=256;f.height=1;
  RemasterFrameMaterial ui;ui.name="ui";ui.surfaceClass=RemasterSurfaceClass::UserInterface;f.materials.push_back(ui);
  auto &last=f.assetMetadata[0];last.hasHeight=true;last.height.fill(7);last.hasEmission=true;last.emissionRgba.fill(255);
  auto first=last;first.hasMaterialSelectors=true;first.materialSelectors.fill("ui");
  if(mode<2)f.assetMetadata.insert(f.assetMetadata.begin(),first);
  else if(mode==2)f.assetMetadata[0]=first;
  const bool before=S9xRemasterHeldLampHudPixel(f,0);need(before==(mode!=3),"active HUD before mutation");
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,false);ref.update(f,false);
  if(mode==0)f.assetMetadata[0].materialSelectors.fill("");
  if(mode==1)f.assetMetadata.erase(f.assetMetadata.begin());
  if(mode==2)f.assetMetadata.erase(f.assetMetadata.begin());
  if(mode==3)f.assetMetadata[0]=first;
  const bool after=S9xRemasterHeldLampHudPixel(f,0);need(after==(mode==3),"active HUD flips after metadata edit/add/remove");
  c.update(f,false);ref.update(f,false);RemasterLightingFieldCache fresh;fresh.update(f,false);equal(c,fresh);
  need(c.participation[0]==(after?0:255)&&c.emission[0]==(after?0:mode==2?0:255),"active HUD participation/emission transport");
  if(mode!=2)need(c.height[1]==(after?0:1),"active HUD authored height validity");
  equal(c,ref);
 }
 // Missing metadata appears for an existing, formerly absent fallback; the
 // changed candidate was not a previously selected donor. All seven buffers
 // are populated; only offsets referencing it may require selection.
 for(int r=0;r<7;++r){testMutation=100+r;auto f=fixture(0,0);f.assetMetadata.erase(f.assetMetadata.begin()+1);
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,true);ref.update(f,true);
  RemasterFrameAssetMetadata m;m.tileId=f.tileInstances[1].tileId;m.hasHeight=true;m.height.fill(0);f.assetMetadata.push_back(m);
  auto *buffer=r==0?&f.mainPixels:r==1?&f.subPixels:r==2?&f.scenePixels:&f.observedBgPixels[r-3];(*buffer)[100]=f.subPixels[0];
  c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,ref);equal(c,fresh);need(c.selectedPixels<f.mainPixels.size(),"appearance local work");
 }
 // First original metadata is HUD policy, LAST is field policy. Editing or
 // reordering only the first must still invalidate every affected HUD result.
 for(int change=0;change<3;++change){testMutation=200+change;auto f=fixture(0,0);RemasterFrameMaterial ui;ui.name="ui";ui.surfaceClass=RemasterSurfaceClass::UserInterface;f.materials.push_back(ui);
  auto m=f.assetMetadata[0];m.hasMaterialSelectors=true;m.materialSelectors.fill("ui");f.assetMetadata.insert(f.assetMetadata.begin(),m);
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,true);ref.update(f,true);
  if(change==0)f.assetMetadata[0].materialSelectors.fill("");
  if(change==1)f.assetMetadata[0].hasMaterialSelectors=false;
  if(change==2)std::swap(f.assetMetadata[0],f.assetMetadata[1]);
  c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,fresh);
  // Reordering duplicates with unchanged LAST producer also exposes the frozen
  // cache's FIRST snapshot bug; fresh full consumer remains mandatory.
  if(change!=2)equal(c,ref);else {std::cout<<"duplicate reorder frozen participation="<<int(ref.participation[0])<<" fresh="<<int(fresh.participation[0])<<" frozen height="<<ref.height[1]<<" fresh height="<<fresh.height[1]<<'\n';need(ref.participation[0]==255&&fresh.participation[0]==0,"named last-match duplicate reorder: frozen world255 vs fresh UI0");}need(c.selectedPixels==f.mainPixels.size(),"first-match HUD policy full fallback");
 }
 // HUD underlay depth ties, scene substitution, absent->valid IDs and claims.
 for(int mutation=0;mutation<53;++mutation){testMutation=300+mutation;auto f=fixture(0,3);for(size_t k=0;k<64;++k){f.mainPixels[k]={uint32_t(RemasterSourceType::Background)<<24|2<<16,3,uint8_t(k)};f.observedBgDepth[0][k]=f.observedBgDepth[1][k]=20;}
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,true);ref.update(f,true);mutate(f,mutation);c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,ref);equal(c,fresh);
 }
 std::cout<<"PASS appearance, first-HUD/last-field duplicates, under-HUD ties\n";
 // Global transitions and unchanged retention of renderer-derived normals.
 {auto f=fixture(0,0);auto &in=f.tileInstances[0];auto &m=f.assetMetadata[0];m.tileRole=RemasterTileRole::OutOfBounds;in.visualClaim.mask=UINT64_MAX;in.visualClaim.published=true;in.hasPlacementHeight=true;in.placementHeight.fill(0);in.hasPlacementNormals=true;in.placementNormalXyz.fill(128);
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,true);ref.update(f,true);need(c.semanticNormals[0]==1,"published OOB semantic normal exercised");
  in.visualClaim.mask=1;in.visualClaim.status=RemasterVisualClaimStatus::Conflict;in.visualClaim.component=7;c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,ref);equal(c,fresh);
  in.visualClaim.published=false;c.update(f,true);ref.update(f,true);RemasterLightingFieldCache absent;absent.update(f,true);equal(c,ref);equal(c,absent);need(c.height[1]==0,"unpublished OOB terminal blocks fallback");}
 {auto f=fixture(0,0);RemasterLightingFieldCache c;c.update(f,true);render(c,f);auto surface=c.surface;c.update(f,true);need(c.surface==surface&&c.selectedPixels==0&&c.updatedPixels==0,"unchanged derived normals retained");
  for(bool scene:{false,true}){c.update(f,scene);RemasterLightingFieldCache fresh;fresh.update(f,scene);render(c,f);render(fresh,f);equal(c,fresh);need(c.selectedPixels==f.mainPixels.size(),"scene toggle full fallback");}}
 // Real outdoor backdrop predicate uses BOTH preferred and sub absence, not
 // merely selected donors. Keys must change even if those donors stay absent.
 {auto f=fixture(0,0);f.area.id=320;f.mainPixels[100]={uint32_t(RemasterSourceType::Backdrop)<<24,0,0};f.scenePixels[100]=f.mainPixels[100];
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;c.update(f,true);ref.update(f,true);need(c.samples[100].fallbackGround,"backdrop initially ground");
  f.subPixels[100]={uint32_t(RemasterSourceType::Object)<<24,4,0};c.update(f,true);ref.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,ref);equal(c,fresh);need(c.selectedPixels==1&&!c.samples[100].fallbackGround,"backdrop sub-presence local invalidation");}
 // Main-count/raster/depth shape and instance-count changes remain global.
 for(int mode=0;mode<4;++mode){auto f=fixture(0,0);RemasterLightingFieldCache c;c.update(f,true);if(mode==0)f.observedBgPixels[3].clear();if(mode==1)f.observedBgDepth[3].clear();if(mode==2)f.tileInstances.pop_back();if(mode==3){f.mainPixels.resize(128);f.originalRgb555.resize(128);f.width=16;}
  c.update(f,true);RemasterLightingFieldCache fresh;fresh.update(f,true);equal(c,fresh);need(c.selectedPixels==f.mainPixels.size(),"shape/count global fallback");}
}
static volatile size_t sink=0;
static double median(std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];}
static void benchmark(const char*path,int repetitions){
 auto base=std::make_unique<RemasterFrame>();need(S9xReadRemasterFrame(path,*base),"read benchmark capture");
 for(const std::string mode:{"cold","unchanged","local","global"}){
  RemasterLightingFieldCache c;RemasterLightingFieldCacheStep1Reference ref;auto changed=std::make_unique<RemasterFrame>(*base);
  // Deliberately synthetic bounded mutation, NOT a captured animation frame.
  size_t id=0;for(const auto&p:base->mainPixels)if(p.instanceId&&p.instanceId<=base->tileInstances.size()&&p.tilePixel<64){id=p.instanceId;break;}
  need(id!=0,"capture has an occurrence");changed->tileInstances[id-1].heightOffset+=1;
  if(mode=="global")changed->lightingCoordinateScale+=1;
  c.update(*base,true);ref.update(*base,true);
  std::vector<double> oldTimes,newTimes;size_t selected=0,oldSelected=0;
  for(int k=0;k<repetitions+10;++k){const auto &f=mode=="unchanged"||!(k&1)?*base:*changed;
   double a=0,b=0;
   auto oldRun=[&](){auto t=std::chrono::steady_clock::now();if(mode=="cold"){RemasterLightingFieldCacheStep1Reference cold;cold.update(f,true);sink+=cold.selectedPixels;}else ref.update(f,true);a=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();};
   auto newRun=[&](){auto t=std::chrono::steady_clock::now();if(mode=="cold"){RemasterLightingFieldCache cold;cold.update(f,true);sink+=cold.selectedPixels;}else c.update(f,true);b=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-t).count();};
   if(k&1){newRun();oldRun();}else{oldRun();newRun();}
   // Output proof outside timing, including cold construction/destruction case.
   if(mode=="cold"){c=RemasterLightingFieldCache{};ref=RemasterLightingFieldCacheStep1Reference{};c.update(f,true);ref.update(f,true);}
   RemasterLightingFieldCache fresh;fresh.update(f,true);
   auto cm=render(c,f),rm=render(ref,f),fm=render(fresh,f);equal(c,ref);equal(c,fresh);meshes(cm,rm);meshes(cm,fm);
   if(k>=10){oldTimes.push_back(a);newTimes.push_back(b);selected+=c.selectedPixels;oldSelected+=ref.selectedPixels;
    std::cout<<"RAW,"<<path<<','<<mode<<','<<k-10<<','<<a<<','<<b<<','<<ref.selectedPixels<<','<<c.selectedPixels<<'\n';}
  }
  std::cout<<"SUMMARY,"<<path<<','<<mode<<",pairs="<<repetitions<<",old_ms="<<median(oldTimes)<<",new_ms="<<median(newTimes)<<",old_selected="<<oldSelected/repetitions<<",new_selected="<<selected/repetitions<<'\n';
 }
}
int main(int argc,char**argv){proof();edges();const char *pairs=std::getenv("REMASTER_LOCAL_BENCH_PAIRS");for(int i=1;i<argc;++i)benchmark(argv[i],pairs?std::max(1,std::atoi(pairs)):100);}
