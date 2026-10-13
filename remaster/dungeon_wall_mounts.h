#ifndef REMASTER_DUNGEON_WALL_MOUNTS_H
#define REMASTER_DUNGEON_WALL_MOUNTS_H
// Terminal wall-mounted fixtures. Full ROM footprint and BOTH surviving wall
// ends prove backing ownership; collision/Link plane alone never supplies Z.
inline void S9xRemasterAttachWallMounts(const RemasterDungeonFloorContext &c,
 RemasterDungeonHeightMap &map)
{
 if(!c.verifiedAlttpRom || !c.dungeonTemplateData || !c.constructedMaps || !c.placements)return;
 for(int off:{0xa14,0xa24,0xa34,0xa44}) {
  const bool north=off<0xa34;const int w=north?4:2,h=north?2:4;
  for(int y=0;y<=64-h;y++)for(int x=0;x<=64-w;x++)for(int flip=0;flip<4;flip++) {
   bool exact=true,seed=false;int palette=-1;
   for(int yy=0;exact && yy<h;yy++)for(int xx=0;exact && xx<w;xx++) {
    int sx=flip&1?w-1-xx:xx,sy=flip&2?h-1-yy:yy;
    int n=off+2*(north?sy*w+sx:sx*h+sy);
    int expected=(c.dungeonTemplateData[n]|c.dungeonTemplateData[n+1]<<8)^((flip&1?0x4000:0)|(flip&2?0x8000:0));
    int i=(y+yy)*64+x+xx,word=c.constructedMaps[i*2]|c.constructedMaps[i*2+1]<<8;
    const auto &t=c.placements[i];int delta=((word>>10)-(expected>>10))&7;
    if((word&0xc3ff)!=(expected&0xc3ff) || (palette>=0 && palette!=delta) || !t.hasHeight ||
     (t.surface!=RemasterDungeonFloorContext::Surface::Wall && t.surface!=RemasterDungeonFloorContext::Surface::Unknown))exact=false;
    palette=delta;seed|=t.surface==RemasterDungeonFloorContext::Surface::Wall;
   }
   if(!exact || !seed || (north?(x==0 || x+w>=64):(y==0 || y+h>=64)))continue;
   // Tangent ends must each carry the same displayed cross section and solved
   // structural plane. No nearest-wall search or propagation through a gap.
   struct Part{int cell,base;std::array<uint8_t,64> local;int plane;};std::vector<Part> parts;
   bool complete=true;
   for(int band=0;band<(north?h:w);band++) {
    int a=north?(y+band)*64+x-1:(y-1)*64+x+band;
    int b=north?(y+band)*64+x+w:(y+h)*64+x+band;
    const auto &ta=c.placements[a],&tb=c.placements[b];
    if(ta.surface!=RemasterDungeonFloorContext::Surface::Wall || tb.surface!=RemasterDungeonFloorContext::Surface::Wall ||
     !ta.hasHeight || !tb.hasHeight){complete=false;break;}
    auto sample=[](const RemasterDungeonFloorContext::Placement &t,int xx,int yy){return int(t.height[(t.vflip?7-yy:yy)*8+(t.hflip?7-xx:xx)]);};
    int plane=-1,base=255;std::array<int,64> effective={};
    for(int p=0;p<2;p++) {
     int ba=map.structuralPlaneOffsets[p][a],bb=map.structuralPlaneOffsets[p][b];if(ba==255 || bb==255)continue;
     bool compatible=true;std::array<int,64> values={};
     for(int py=0;py<8;py++)for(int px=0;px<8;px++) {
      values[py*8+px]=ba+sample(ta,px,py);
      if(values[py*8+px]!=bb+sample(tb,px,py))compatible=false;
      // Backing has no tangent rise: preserve its authored normal-axis ramp.
      if(sample(ta,px,py)!=sample(ta,north?0:px,north?py:0))compatible=false;
     }
     if(!compatible){complete=false;break;}
     if(plane>=0 && values!=effective){complete=false;break;}
     plane=p;base=ba;effective=values;
    }
    if(!complete || plane<0){complete=false;break;}
    for(int k=0;k<(north?w:h);k++) {
     int i=north?(y+band)*64+x+k:(y+k)*64+x+band;
     const auto &t=c.placements[i];std::array<uint8_t,64> local={};
     for(int py=0;py<8;py++)for(int px=0;px<8;px++) {
      int v=effective[py*8+px]-base;if(v<0 || v>255)complete=false;
      local[(t.vflip?7-py:py)*8+(t.hflip?7-px:px)]=uint8_t(v);
     }
     parts.push_back({i,base,local,plane});
    }
   }
   if(!complete)continue;
   // Whole-assembly claims must agree; a transformed/template symmetry may
   // reproduce the same footprint, but cannot replace a differing solved base.
   for(const auto &part:parts)if(map.offsets[part.cell]!=255 && map.structuralPlaneOffsets[part.plane][part.cell]!=255 &&
    map.offsets[part.cell]!=part.base)complete=false;
   if(!complete){map.structuralConflicts++;continue;}
   for(const auto &part:parts) {
    map.offsets[part.cell]=part.base;map.structuralPlaneOffsets[part.plane][part.cell]=part.base;
    map.fallbackDirections[part.cell]=0;
    auto found=std::find_if(map.localHeightPreviews.begin(),map.localHeightPreviews.end(),[&](const auto &v){return v.cell==part.cell;});
    if(found==map.localHeightPreviews.end())map.localHeightPreviews.push_back({part.cell,part.local});
   }
  }
 }
}
#endif
