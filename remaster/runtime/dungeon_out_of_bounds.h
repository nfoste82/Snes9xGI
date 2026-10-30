#ifndef S9X_REMASTER_DUNGEON_OUT_OF_BOUNDS_H
#define S9X_REMASTER_DUNGEON_OUT_OF_BOUNDS_H
#define REMASTER_OOB_CREST_RANGE_DIAGNOSTICS 1

// One-way terminal visual extension. Never contributes an equation or anchor to
// the wall solve, physical support, collision, or walkability.
namespace S9xRemasterVisibleFace {
struct PlateauCell {
 RemasterVisualClaimStatus status=RemasterVisualClaimStatus::NotEvaluated;
 int base=0,component=-1;
 // Accepted terminal callback count (not unique wall count). Unequal crests
 // remain inspectable independently of unsupported/conflicting contacts.
 int crestMin=0,crestMax=0,crestCount=0;
};
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
struct PlateauEvent {
 // kind: 0 join, 1 terminal, 2 poison, 3 component result.
 int kind,component,a,b,d,status,height,owner,plane;
 int crestMin=0,crestMax=0,crestCount=0;
};
inline thread_local std::vector<PlateauEvent> *plateauEvents=nullptr;
inline void tracePlateau(PlateauEvent e){if(plateauEvents)plateauEvents->push_back(e);}
#endif
// C++14-compatible dispatch: the app core uses an older language mode than the
// standalone regressions. Scalar adapters describe a single-value edge.
template<class Crest>
inline auto plateauCrest(Crest &crest,int bg,int cell,int d,int &h,int &o,int &p,int sb,int sc,int &hi,int)
 -> decltype(crest(bg,cell,d,h,o,p,sb,sc,hi)) {return crest(bg,cell,d,h,o,p,sb,sc,hi);}
template<class Crest>
inline RemasterVisualClaimStatus plateauCrest(Crest &crest,int bg,int cell,int d,int &h,int &o,int &p,int sb,int sc,int &hi,long) {
 auto status=crest(bg,cell,d,h,o,p,sb,sc);hi=h;return status;
}
template<class Crest>
inline auto plateauDatum(Crest &crest,int bg,int cell,int d,int &h,int &o,int &p,int sb,int sc,int &hi,bool exact,int &datum,int)
 -> decltype(crest(bg,cell,d,h,o,p,sb,sc,hi,exact,datum)) {return crest(bg,cell,d,h,o,p,sb,sc,hi,exact,datum);}
template<class Crest>
inline RemasterVisualClaimStatus plateauDatum(Crest &crest,int bg,int cell,int d,int &h,int &o,int &p,int sb,int sc,int &hi,bool exact,int &datum,long) {
 (void)datum;
 auto status=plateauCrest(crest,bg,cell,d,h,o,p,sb,sc,hi,0);
 // A scalar/range callback contains no paired local samples. Never promote it
 // to a nonflat contact equation.
 return exact && status==RemasterVisualClaimStatus::Solved?RemasterVisualClaimStatus::Unsupported:status;
}
template<class Crest,class Join>
inline std::vector<PlateauCell> outOfBoundsPlateaus(
 const RemasterDungeonFloorContext::Placement *const tiles[2],Crest crest,Join join) {
 using Surface=RemasterDungeonFloorContext::Surface;
 std::vector<PlateauCell> out(8192);
 static const int dx[]={0,0,-1,1},dy[]={-1,1,0,0};
 auto neighbor=[](int i,int x,int y){x+=i%64;y+=i/64;return x<0||x>=64||y<0||y>=64?-1:y*64+x;};
 for(int bg=0;bg<2;bg++)for(int start=0;start<4096;start++) {
  if(tiles[bg][start].surface!=Surface::OutOfBounds || out[bg*4096+start].component>=0)continue;
  const int component=bg*4096+start;std::vector<int> pending{component};
  out[component].component=component;
    bool invalid=false,conflict=false,anchored=false,nonflat=false,seamConflict=false;int height=0,maximum=0,count=0,owner=-1,plane=-1;
   std::map<int,int> relative;relative[component]=0;
   auto sample=[&](int index,int x,int y) {const auto &t=tiles[index/4096][index%4096];
    return t.hasHeight?int(t.height[(t.vflip?7-y:y)*8+(t.hflip?7-x:x)]):0;};
   for(size_t at=0;at<pending.size();at++) {
    int layer=pending[at]/4096,i=pending[at]%4096;const auto &t=tiles[layer][i];
    invalid|=t.stairSourceAmbiguous;
    nonflat|=t.hasHeight && !std::all_of(t.height.begin(),t.height.end(),[&](int h){return h==t.height[0];});
   for(int d=0;d<4;d++) {
    int j=neighbor(i,dx[d],dy[d]);if(j<0)continue;
    for(int other=0;other<2;other++) {
      if(tiles[other][j].surface==Surface::OutOfBounds) {
       const bool admitted=join(layer,i,other,j,d);
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
       tracePlateau({0,component,layer*4096+i,other*4096+j,d,int(admitted),0,-1,-1});
#endif
         if(admitted) {
         int delta=0;bool first=true;
         if(S9xRemasterDungeonResearchEnabled())
         for(int k=0;k<8;k++) {
         int x=dx[d]?(dx[d]>0?7:0):k,y=dy[d]?(dy[d]>0?7:0):k;
         int value=sample(layer*4096+i,x,y)-sample(other*4096+j,dx[d]?7-x:x,dy[d]?7-y:y);
         if(!first && delta!=value)seamConflict=true;delta=value;first=false;
        }
        int next=other*4096+j,value=relative[layer*4096+i]+delta;
        auto &v=out[next];if(v.component<0){v.component=component;pending.push_back(next);relative[next]=value;}
        else if(v.component==component && relative[next]!=value)seamConflict=true;
        }
       }
     }
    }
   }
   if(nonflat && !S9xRemasterDungeonResearchEnabled()) {
    // Opaque adjacency is not validated modeled contact. Preserve local
    // channels but publish neither a numeric datum nor a guessed conflict.
    for(int index:pending)out[index].status=RemasterVisualClaimStatus::Unsupported;
    continue;
   }
   if(nonflat)conflict|=seamConflict;
   int signedBase=0;
   for(int index:pending) {
    int layer=index/4096,i=index%4096;
    for(int d=0;d<4;d++) {
     int j=neighbor(i,dx[d],dy[d]);if(j<0)continue;
     for(int other=0;other<2;other++) {
       int h=0,o=-1,p=-1,hi=0;
      // Production terminals retain the full validated within-edge range.
      // Scalar callers represent a single-value edge (legacy/test adapters).
       int datum=0;
       auto status=plateauDatum(crest,other,j,d^1,h,o,p,layer,i,hi,nonflat,datum,0);
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
      tracePlateau({1,component,layer*4096+i,other*4096+j,d,int(status),h,o,p});
      if(status==RemasterVisualClaimStatus::Conflict || status==RemasterVisualClaimStatus::Unsupported || status==RemasterVisualClaimStatus::Unanchored ||
          (status==RemasterVisualClaimStatus::Solved && anchored && plane!=p))
       tracePlateau({2,component,layer*4096+i,other*4096+j,d,int(status),h,o,p});
#endif
     if(status==RemasterVisualClaimStatus::Conflict)conflict=true;
     if(status==RemasterVisualClaimStatus::Unsupported || status==RemasterVisualClaimStatus::Unanchored)invalid=true;
     if(status!=RemasterVisualClaimStatus::Solved)continue;
      // Floor identity is provenance of the independently accepted wall, not
      // ownership of this unreachable sheet. Plane disagreement is still fatal.
       if(p<0 || p>1 || o<0 || (anchored && plane!=p))conflict=true;
       if(nonflat) {
        const int vote=datum-relative[index];
        if(anchored && signedBase!=vote)conflict=true;
        signedBase=vote;
       }
      height=anchored?std::min(height,h):h;
       maximum=anchored?std::max(maximum,hi):hi;
      owner=anchored?std::min(owner,o):o;plane=p;anchored=true;++count;
    }
   }
  }
   for(int index:pending) {
    int layer=index/4096,i=index%4096;
    auto &v=out[index];v.status=conflict?RemasterVisualClaimStatus::Conflict:
     invalid?RemasterVisualClaimStatus::Unsupported:anchored?RemasterVisualClaimStatus::Solved:RemasterVisualClaimStatus::Unanchored;
      v.base=nonflat?signedBase+relative[index]:height-(tiles[layer][i].hasHeight?tiles[layer][i].height[0]:0);
     v.crestMin=height;v.crestMax=maximum;v.crestCount=count;
#ifdef REMASTER_WALL_BAND_DIAGNOSTICS
     tracePlateau({3,component,index,-1,-1,int(v.status),v.base,owner,plane,height,maximum,count});
#endif
  }
 }
 return out;
}
// Without explicit edge evidence no connectivity is certified.
template<class Crest>
inline std::vector<PlateauCell> outOfBoundsPlateaus(
 const RemasterDungeonFloorContext::Placement *const tiles[2],Crest crest) {
 return outOfBoundsPlateaus(tiles,crest,[](int,int,int,int,int){return false;});
}
}
#endif
