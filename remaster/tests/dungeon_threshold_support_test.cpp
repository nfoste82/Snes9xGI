#include "../runtime/remaster.h"
#include <cassert>
#include <iostream>

int main() {
 using S=RemasterDungeonFloorContext::Surface;
 auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
 auto map=std::make_unique<RemasterDungeonHeightMap>();
 std::vector<uint8_t> a(8192,1);
 RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;c.module=7;c.roomIndex=9;
 c.placements=tiles->data();c.collisionAttributes=a.data();c.linkX=160;c.linkY=152;
 for(int p=0;p<2;p++)for(int bg=0;bg<2;bg++)for(int flip=0;flip<4;flip++) {
  *tiles={};a.assign(8192,1);c.linkPlane=p;c.placementBackgroundIndex=bg;
  for(int y:{20,27})for(int yy=y;yy<y+4;yy++)for(int x=20;x<24;x++) {
   auto &t=(*tiles)[yy*64+x];t.surface=S::Floor;t.hasHeight=true;a[p*4096+yy*64+x]=0;
  }
  for(int y=24;y<27;y++)for(int x=20;x<24;x++) {
   auto &t=(*tiles)[y*64+x];t.surface=S::Wall;t.hasHeight=true;
   for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)t.height[yy*8+xx]=7-yy;
  }
  // Two-wide oriented terminal traversable threshold, not ordinary 00 support.
  for(int y=18;y<20;y++)for(int x=21;x<23;x++) {
   auto &t=(*tiles)[y*64+x];t.surface=S::Floor;t.hasHeight=true;t.hflip=flip&1;t.vflip=flip&2;
   t.height[3]=2;a[p*4096+y*64+x]=0x8e;
  }
  const auto original=(*tiles)[18*64+21].height;
  RemasterDungeonMovementCache cache;c.movementCache=&cache;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   for(int y=18;y<20;y++)for(int x=21;x<23;x++)assert(map->actorThresholdBases[p][y*64+x]==24);
   assert(map->actorThresholdBases[1-p][18*64+21]==255);
  assert(map->occupiedFloorOwners[p][18*64+21]<0); // no movement expansion
  assert((*tiles)[18*64+21].height==original);
  auto required=map->requiredPlaneCells;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map)); // cached traversal claims
   assert(map->actorThresholdBases[p][18*64+21]==24 && map->requiredPlaneCells==required);
   {auto actor=c;actor.linkX=21*8-8;actor.linkY=18*8-20;
    assert(S9xRemasterLinkFloorHeight(actor,*map)==24);}
   const auto actorBases=map->actorThresholdBases;
   const auto actorOwners=map->actorThresholdOwners;
   c.movementCache=nullptr;assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->actorThresholdBases==actorBases && map->actorThresholdOwners==actorOwners);
   c.movementCache=&cache;
  cache.valid=false;assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
  // Independently transform the entire physical fixture, including ramp samples
  // and collision direction; this exercises all four terminal approach sides.
  auto savedTiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>(*tiles);
  const auto savedAttributes=a;
  for(int transform=1;transform<8;transform++) {
   *tiles={};a.assign(8192,1);
   auto point=[&](int x,int y,int size){if(transform&1)std::swap(x,y);if(transform&2)x=size-1-x;if(transform&4)y=size-1-y;return y*size+x;};
   for(int i=0;i<4096;i++) {
    const int dest=point(i%64,i/64,64);auto t=(*savedTiles)[i];
    // Bake original display flips before transforming; no changed oracle.
    auto h=t.height;for(int y=0;y<8;y++)for(int x=0;x<8;x++)t.height[point(x,y,8)]=h[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)];
    t.hflip=t.vflip=false;(*tiles)[dest]=t;
    for(int plane=0;plane<2;plane++){auto v=savedAttributes[plane*4096+i];if((transform&1) && v==0x8e)v=0x8f;a[plane*4096+dest]=v;}
   }
   c.movementCache=nullptr;
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
    assert(map->actorThresholdBases[p][point(21,18,64)]==24);
    assert(map->actorThresholdBases[1-p][point(21,18,64)]==255);
  }
  *tiles=*savedTiles;a=savedAttributes;c.movementCache=&cache;
  for(int negative=0;negative<4;negative++) {
   auto saved=(*tiles)[18*64+21];
   if(negative==0)a[p*4096+18*64+21]=0x1c; // other plane's threshold is not interchangeable
   if(negative==1)(*tiles)[18*64+21].hasHeight=false;
   if(negative==2)(*tiles)[18*64+21].structuralFloorPlaneMask=1<<(1-p);
   if(negative==3)for(int y=18;y<20;y++)for(int x=21;x<23;x++)a[p*4096+y*64+x]=0x8f; // wrong axis
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
    if(negative==0 || negative==3)assert(map->actorThresholdBases[p][18*64+21]==255);
    else assert(map->actorThresholdBases[p][18*64+21]==24); // actor support independent of missing/overwritten BG channel
   (*tiles)[18*64+21]=saved;
   for(int y=18;y<20;y++)for(int x=21;x<23;x++)a[p*4096+y*64+x]=0x8e;
  }
  // Same XY on a separately observed plane has an independent zero datum.
  c.stairCount=1;c.stairs[0].x=255;c.stairs[0].changesPlane=true;
  for(int y=18;y<24;y++)for(int x=20;x<24;x++)a[(1-p)*4096+y*64+x]=y<20?0x8e:0;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->actorThresholdBases[p][18*64+21]==24 && map->actorThresholdBases[1-p][18*64+21]==0);
  c.stairCount=0;
  a=savedAttributes;
  // A real authored contradictory wall seam poisons its support component.
  for(int y=24;y<27;y++)for(int yy=0;yy<8;yy++)for(int xx=0;xx<8;xx++)(*tiles)[y*64+20].height[yy*8+xx]=2*(7-yy);
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->structuralConflicts>0 && map->actorThresholdBases[p][18*64+21]==255);
  *tiles=*savedTiles;a=savedAttributes;
  // Block the entire attachment mouth: a closed/overwritten passage cannot
  // inherit from the ordinary floor beyond it, nor from a previous cached solve.
  for(int x=20;x<24;x++)a[p*4096+20*64+x]=1;
  assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->actorThresholdBases[p][18*64+21]==255);
   {auto actor=c;actor.linkX=21*8-8;actor.linkY=18*8-20;
    assert(S9xRemasterLinkFloorHeight(actor,*map)==255);}
  c.verifiedAlttpRom=false;
  assert(!S9xRemasterBuildDungeonHeightMap(c,50,*map));c.verifiedAlttpRom=true;
  c.collisionMode=1;assert(!S9xRemasterBuildDungeonHeightMap(c,50,*map));c.collisionMode=0;
  }
  // Two separate ordinary corridors reach overlapping terminal bodies in a
  // shared threshold band. No cache edits: claims come from actual traversal.
  for(int p=0;p<2;p++)for(int bg=0;bg<2;bg++)for(int transform=0;transform<8;transform++) {
   *tiles={};a.assign(8192,1);c.linkPlane=p;c.placementBackgroundIndex=bg;c.movementCache=nullptr;
   auto point=[&](int x,int y){if(transform&1)std::swap(x,y);if(transform&2)x=63-x;if(transform&4)y=63-y;return y*64+x;};
   const char *collision[]={"TT...TT#..",".#.TTTTT..",".....T....",".TT......T","T..TT#.T..","...T.T..T#","T.T.T.TT.T",".TT....#.#",".#.....TT.","#.#..TT.T."};
   for(int y=0;y<10;y++)for(int x=0;x<10;x++) {
    const int i=point(x+16,y+16);const char v=collision[y][x];
    a[p*4096+i]=v=='.'?0:v=='#'?1:(transform&1)?0x8f:0x8e;
    auto &t=(*tiles)[i];t.surface=S::Floor;t.hasHeight=true;
   }
   // Initial body is blocked; the solver's ordinary complete-body seeds
   // independently discover both corridors (also under whole-room transforms).
   c.linkX=0;c.linkY=0;
   RemasterDungeonMovementCache cache;c.movementCache=&cache;
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   size_t competing=0;
   for(int i=0;i<4096;i++)if(map->actorThresholdOwners[p][i]==-2){
    assert(map->actorThresholdBases[p][i]==255);competing++;
    auto actor=c;actor.linkX=(i%64)*8-8;actor.linkY=(i/64)*8-20;
    assert(S9xRemasterLinkFloorHeight(actor,*map)==255); // never fall back to art's solved zero
   }
   assert(competing>0);
   const auto owners=map->actorThresholdOwners;
   const auto support=map->actorThresholdBases;
   assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));assert(map->actorThresholdOwners==owners && map->actorThresholdBases==support);
   c.movementCache=nullptr;assert(S9xRemasterBuildDungeonHeightMap(c,50,*map));
   assert(map->actorThresholdOwners==owners && map->actorThresholdBases==support);
  }
 std::cout<<"PASS terminal threshold support, planes/BGs/flips, relief, cache, missing/blocked/excluded/wrong-axis negatives\n";
}
