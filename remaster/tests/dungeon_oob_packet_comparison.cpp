// Bounded before/after replay comparison; inputs are immutable own-profile
// EndFrame packets, not the currently edited canonical profile.
#include "../runtime/lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
static void require(bool b,const char *why){if(!b)throw std::runtime_error(why);}
int main(int argc,char **argv) {try {
 require(argc==3,"usage: before.s9xrmf after.s9xrmf");
 RemasterFrame before,after;require(S9xReadRemasterFrame(argv[1],before) && S9xReadRemasterFrame(argv[2],after),"read packets");
 require(before.width==after.width && before.height==after.height && before.assetMetadata.size()==after.assetMetadata.size(),"dimensions/metadata inventory");
 require(before.originalRgb555==after.originalRgb555 && before.sceneRgb555==after.sceneRgb555,"artwork unchanged");
 for(size_t k=0;k<before.assetMetadata.size();k++) {
  const auto &a=before.assetMetadata[k],&b=after.assetMetadata[k];
  require(a.tileId==b.tileId && a.tileRole==b.tileRole && a.hasHeight==b.hasHeight && a.height==b.height && a.hasNormals==b.hasNormals && a.normalXyz==b.normalXyz && a.hasOcclusion==b.hasOcclusion && a.occlusion==b.occlusion && a.hasEmission==b.hasEmission && a.emissionRgba==b.emissionRgba,"all metadata arrays/presence unchanged");
 }
 RemasterLightingFieldCache a,b;a.update(before,true);b.update(after,true);
 RemasterFrameChannelResolver ac(before),bc(after);
 const auto mesh=RemasterSurfaceMesh::build(after.width,after.height,b.samples,after.lightingCoordinateScale/255);
 size_t nonOob=0,solved=0,unknown=0;
 for(size_t at=0;at<after.scenePixels.size();at++) {
  auto p=before.scenePixels[at],q=after.scenePixels[at];require(p.owner==q.owner && p.tilePixel==q.tilePixel,"stock ownership/sample unchanged");
  if(!p.instanceId){require(!q.instanceId,"empty unchanged");continue;}
  const auto &i=before.tileInstances[p.instanceId-1],&j=after.tileInstances[q.instanceId-1];
  require(i.tileId==j.tileId,"selected source identity unchanged");
  const auto &x=ac.effective[p.instanceId-1],&y=bc.effective[q.instanceId-1];
  require(x.hasNormals==y.hasNormals && x.normalXyz==y.normalXyz && x.hasOcclusion==y.hasOcclusion && x.occlusion==y.occlusion,"effective independent normals/masks unchanged");
  if(x.tileRole!=RemasterTileRole::OutOfBounds) {
   require(i.heightOffset==j.heightOffset && i.visualClaim==j.visualClaim && i.hasPlacementHeight==j.hasPlacementHeight && i.placementHeight==j.placementHeight,"wall/floor and other occurrence heights/claims unchanged");
   require(a.height[at*2]==b.height[at*2] && a.height[at*2+1]==b.height[at*2+1],"non-OOB selected CPU height unchanged");++nonOob;
  } else if(j.visualClaim.published) {
   require(b.samples[at].known && b.samples[at].sheet && !b.samples[at].solidWall && mesh[at].thickness==0,"solved OOB actual planar mesh");
   for(float corner:mesh[at].corners)require(std::abs(corner-b.surface[at*4])<1e-6,"flat minimum mesh corners");++solved;
  } else {require(!b.samples[at].known,"unanchored remains unknown");++unknown;}
 }
 std::cout<<"PASS metadata/art/normal/mask/non-OOB parity and OOB mesh: nonOob="<<nonOob<<" solved="<<solved<<" unknown="<<unknown<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
