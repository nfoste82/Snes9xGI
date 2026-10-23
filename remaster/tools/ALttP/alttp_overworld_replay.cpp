// Apply a complete-area candidate to an existing genuinely linked Snes9x packet.
// Alignment is inferred from exact artwork/slot/palette/flip correspondences,
// never filename coordinates. This does NOT establish complete runtime state.
#include "../../runtime/frame.h"
#include <iostream>
#include <map>
#include <set>
#include <tuple>
#include <cmath>
#include <iomanip>
// Retain the standalone producer build command without another link input.
namespace ReplayDigest {
#include "../../../sha256.cpp"
}
struct Cell {
 int x=0,y=0,word=0,status=0,role=0,base=0;
 int support=-1,layer=-1,surface=-1;
 uint64_t hash=0;
 std::array<int16_t,64> height{};
 std::array<uint8_t,192> normals{};
};
using Signature=std::tuple<uint64_t,int,int,bool,bool>;
static Signature signature(const RemasterFrameTileInstance &i) {
 return {i.tileId.hash,i.tileNumber,i.palette,i.hFlip,i.vFlip};
}
static Signature signature(const Cell &c) {
 return {c.hash,c.word&1023,(c.word>>10)&7,bool(c.word&0x4000),bool(c.word&0x8000)};
}
int main(int argc,char **argv) {
 if(argc!=4&&argc!=5) {std::cerr<<"usage: replay INPUT.s9xrmf CANDIDATE.txt OUTPUT.s9xrmf [OWSTAND1.txt]\n";return 2;}
 RemasterFrame f;if(!S9xReadRemasterFrame(argv[1],f))return 1;
 std::ifstream in(argv[2]);std::string magic,rom;size_t count;
 if(!(in>>magic>>rom>>count)||(magic!="OWREPLAY1"&&magic!="OWREPLAY2")||rom!=f.profileRomSha256||count>655360)return 1;
 std::map<std::pair<int,int>,Cell> cells;
 std::map<Signature,std::vector<std::pair<int,int>>> positions;
 for(size_t k=0;k<count;k++) {
  Cell c;std::string hash;
  if(!(in>>c.x>>c.y>>c.word>>hash>>c.status>>c.role>>c.base)||c.status<0||c.status>2||c.role<0||c.role>=int(RemasterTileRole::Count)||c.base<-32767||c.base>32767)return 1;
   c.hash=std::stoull(hash,nullptr,16);
   if(magic=="OWREPLAY2"&&!(in>>c.support>>c.layer>>c.surface))return 1;
  if(c.status==1) {
   for(auto &h:c.height){int v;if(!(in>>v)||v<INT16_MIN||v>INT16_MAX)return 1;h=v;}
   for(auto &n:c.normals){int v;if(!(in>>v)||v<0||v>255)return 1;n=v;}
  }
  if(!cells.emplace(std::make_pair(c.x,c.y),c).second)return 1;
  positions[signature(c)].push_back({c.x,c.y});
 }
 in>>std::ws;if(!in.eof())return 1;
 // One vote per visible tile occurrence, not per pixel or render pass.
 std::map<std::tuple<int,int,int>,double> votes;
 std::set<std::tuple<int,int,int,Signature>> seen;
 for(unsigned y=0;y<f.height;y++)for(unsigned x=0;x<f.width;x++) {
  const auto &p=f.mainPixels[y*f.width+x];if(!p.instanceId||p.tilePixel>=64)continue;
  const auto &i=f.tileInstances[p.instanceId-1];if(i.source!=RemasterSourceType::Background)continue;
  int dx=p.tilePixel%8,dy=p.tilePixel/8;if(i.hFlip)dx=7-dx;if(i.vFlip)dy=7-dy;
  int sx=int(x)-dx,sy=int(y)-dy;auto sig=signature(i);
  if(!seen.emplace(i.sourceIndex,sx,sy,sig).second)continue;
  auto it=positions.find(sig);if(it==positions.end()||it->second.size()>100)continue;
  for(auto xy:it->second)votes[{i.sourceIndex,xy.first*8-sx,xy.second*8-sy}]+=1.0/it->second.size();
 }
 if(votes.empty())return 1;
 auto best=std::max_element(votes.begin(),votes.end(),[](const auto&a,const auto&b){return a.second<b.second;});
 // Reject repeated-artwork origins with nearly equal exact occurrence evidence.
 using Origin=std::tuple<int,int,int>;
 std::vector<std::pair<size_t,Origin>> scores;
 for(const auto &vote:votes) {
  if(vote.second<best->second*0.5)continue;
  const int layer=std::get<0>(vote.first),x0=std::get<1>(vote.first),y0=std::get<2>(vote.first);
  size_t exact=0;
  for(const auto &occ:seen) {
   if(std::get<0>(occ)!=layer)continue;
   int x=x0+std::get<1>(occ),y=y0+std::get<2>(occ);
   auto at=cells.find({x/8,y/8});
   if(x>=0&&y>=0&&x%8==0&&y%8==0&&at!=cells.end()&&signature(at->second)==std::get<3>(occ))exact++;
  }
  scores.push_back({exact,vote.first});
 }
 std::sort(scores.begin(),scores.end(),[](const auto&a,const auto&b){return a.first>b.first;});
 if(scores.empty()||scores[0].first<8||(scores.size()>1&&scores[1].first*100>=scores[0].first*98)) {
  std::cerr<<"ambiguous/insufficient world alignment\n";return 1;
 }
 const int bg=std::get<0>(scores[0].second),ox=std::get<1>(scores[0].second),oy=std::get<2>(scores[0].second);
  const auto originals=f.tileInstances;
  int actorBase=0,actorSupport=-1;size_t actorInstances=0,actorDirectSamples=0,actorMapSamples=0;
  if(argc==5) {
   auto reject=[](){std::cerr<<"standing fixture rejected: pin/footprint/support/artwork/original actor offset\n";return 1;};
   std::ifstream att(argv[4]);std::string tag,sha;int layer,x0,y0,ax,ay,sax,say,local,base,old,nslots;
   if(magic!="OWREPLAY2"||!(att>>tag>>sha>>layer>>x0>>y0>>ax>>ay>>sax>>say>>actorSupport>>local>>base>>old>>nslots)||
      tag!="OWSTAND1"||layer!=bg||x0!=ox||y0!=oy||ax!=sax+ox||ay!=say+oy||old!=0||
      actorSupport<0||base<-32767||base>32767||local<INT16_MIN||local>INT16_MAX||nslots<1||nslots>128)return reject();
   std::ifstream capture(argv[1],std::ios::binary);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(capture)),{});
   if(bytes.size()>UINT32_MAX)return reject();
   unsigned char digest[32];ReplayDigest::sha256sum(bytes.data(),unsigned(bytes.size()),digest);
   std::ostringstream hex;hex<<std::hex<<std::setfill('0');for(auto b:digest)hex<<std::setw(2)<<unsigned(b);
   if(hex.str()!=sha)return reject();
   std::set<int> slots;for(int k=0;k<nslots;k++){int s;if(!(att>>s)||s<0||s>127||!slots.insert(s).second)return reject();}
   // WRAM anchor + (8 +/-3,23), not visible sprite extrema. These are
   // explicit fixture sole samples, not a claim to reproduce movement.
   for(int d=-3;d<=3;d++) {
    int sx,sy,mx,my;if(!(att>>sx>>sy>>mx>>my)||sx!=sax+8+d||sy!=say+23||mx!=sx+ox||my!=sy+oy||
       sx<0||sy<0||sx>=int(f.width)||sy>=int(f.height)||mx<0||my<0)return reject();
    auto at=cells.find({mx/8,my/8});if(at==cells.end())return reject();const Cell &c=at->second;
    if(c.status!=1||c.role!=int(RemasterTileRole::Floor)||c.layer!=0||c.surface!=0||c.support!=actorSupport||
       c.base!=base||c.height[(my%8)*8+mx%8]!=local)return reject();
    bool exact=false,exposed=false;
    for(const auto *plane:{&f.mainPixels,&f.subPixels,&f.scenePixels}) {
     if(plane->empty())continue;const auto &p=(*plane)[sy*f.width+sx];
     if(!p.instanceId||p.tilePixel>=64)continue;const auto &i=originals[p.instanceId-1];
     if(i.source!=RemasterSourceType::Background||i.sourceIndex!=bg)continue;
     exposed=true;int dx=p.tilePixel%8,dy=p.tilePixel/8;if(i.hFlip)dx=7-dx;if(i.vFlip)dy=7-dy;
     if(signature(i)!=signature(c)||dx!=mx%8||dy!=my%8)return reject();exact=true;
    }
    // Covered support: exact selected-BG map cell captured elsewhere in this
    // tile occurrence is sufficient. A nearby occurrence/hash is not.
    if(!exposed)for(const auto &occ:seen)if(std::get<0>(occ)==bg&&std::get<1>(occ)==mx/8*8-ox&&
        std::get<2>(occ)==my/8*8-oy&&std::get<3>(occ)==signature(c))exact=true;
    if(!exact)return reject();
    if(exposed)actorDirectSamples++;else actorMapSamples++;
   }
   att>>std::ws;if(!att.eof())return reject();
   std::set<int> found;
   for(const auto &i:originals) {
    if(i.source!=RemasterSourceType::Object||!slots.count(i.sourceIndex))continue;
    if(i.heightOffset!=0||i.geometryStatus!=RemasterOccurrenceGeometryStatus::Inherit)return reject();
    found.insert(i.sourceIndex);actorInstances++;
   }
   if(found!=slots||!actorInstances)return reject();
   actorBase=base+local;
   for(auto &i:f.tileInstances)if(i.source==RemasterSourceType::Object&&slots.count(i.sourceIndex))i.heightOffset=actorBase;
  }
 std::map<std::tuple<uint32_t,int,int>,uint32_t> copies;
 size_t matched=0,mismatched=0,solved=0,unresolved=0;
 for(auto *plane:{&f.mainPixels,&f.subPixels,&f.scenePixels}) {
  for(size_t n=0;n<plane->size();n++) {
   auto &p=(*plane)[n];if(!p.instanceId||p.tilePixel>=64)continue;
   const auto &i=originals[p.instanceId-1];if(i.source!=RemasterSourceType::Background||i.sourceIndex!=bg)continue;
   int dx=p.tilePixel%8,dy=p.tilePixel/8;if(i.hFlip)dx=7-dx;if(i.vFlip)dy=7-dy;
   int wx=ox+int(n%f.width)-dx,wy=oy+int(n/f.width)-dy;
   auto at=cells.find({wx/8,wy/8});bool match=wx>=0&&wy>=0&&wx%8==0&&wy%8==0&&at!=cells.end()&&signature(at->second)==signature(i);
   if(plane==&f.mainPixels){if(match)matched++;else mismatched++;}
   auto key=std::make_tuple(p.instanceId,wx,wy);auto cached=copies.find(key);
   if(cached==copies.end()) {
    auto copy=i;copy.heightOffset=0;copy.hasPlacementHeight=false;
    copy.geometryStatus=match&&at->second.status==2 ? RemasterOccurrenceGeometryStatus::Conflict : RemasterOccurrenceGeometryStatus::Unresolved;
    copy.placementTileRole=RemasterTileRole::Unknown;
    copy.hasPlacementNormals=copy.hasPlacementOcclusion=copy.placementNormalIgnoreTileTransform=false;
    if(match&&at->second.status==1) {
     const auto &c=at->second;copy.geometryStatus=RemasterOccurrenceGeometryStatus::Resolved;
     copy.placementTileRole=RemasterTileRole(c.role);copy.heightOffset=c.base;
     copy.hasPlacementHeight=copy.hasPlacementNormals=true;copy.placementNormalIgnoreTileTransform=true;
     for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
      int canonical=(i.vFlip?7-y:y)*8+(i.hFlip?7-x:x),display=y*8+x;
      copy.placementHeight[canonical]=c.height[display];
      for(int a=0;a<3;a++)copy.placementNormalXyz[canonical*3+a]=c.normals[display*3+a];
     }
    }
    f.tileInstances.push_back(copy);cached=copies.emplace(key,uint32_t(f.tileInstances.size())).first;
   }
   p.instanceId=cached->second;
   if(plane==&f.mainPixels){if(f.tileInstances[p.instanceId-1].geometryStatus==RemasterOccurrenceGeometryStatus::Resolved)solved++;else unresolved++;}
  }
 }
 // Refuse accidental alignment; preserve a useful incomplete candidate only
 // when the overwhelming majority of visible background agrees exactly.
 if(matched==0||matched<double(matched+mismatched)*0.95){std::cerr<<"insufficient map match "<<matched<<"/"<<matched+mismatched<<"\n";return 1;}
  // Without the explicit standing fixture actors stay unchanged. This fixture
  // attests groundedness only; it does not establish general live actor state.
 f.schemaVersion=REMASTER_FRAME_SCHEMA_VERSION;
 if(!S9xWriteRemasterFrame(f,argv[3]))return 1;
  std::cout<<"{\"bg\":"<<bg<<",\"origin\":["<<ox<<","<<oy<<"],\"matched_pixels\":"<<matched<<",\"mismatched_pixels\":"<<mismatched<<",\"solved_pixels\":"<<solved<<",\"unresolved_pixels\":"<<unresolved<<",\"unique_matching_occurrences\":"<<scores[0].first<<",\"runner_up_occurrences\":"<<(scores.size()>1?scores[1].first:0)<<",\"actor_support_applied\":"<<(actorInstances?"true":"false")<<",\"actor_instances\":"<<actorInstances<<",\"actor_base\":"<<actorBase<<",\"actor_support\":"<<actorSupport<<",\"actor_direct_bg_samples\":"<<actorDirectSamples<<",\"actor_exact_map_samples\":"<<actorMapSamples<<",\"runtime_state_pinned\":false}\n";
}
