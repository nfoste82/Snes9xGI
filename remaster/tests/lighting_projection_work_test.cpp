#include "../runtime/frame.h"
#include <cassert>
#include <iostream>
int main() {
    auto m=std::make_unique<RemasterDungeonHeightMap>();m->roomIndex=7;m->hasRoomGeometry=true;
    m->offsets.fill(24);m->structuralPlaneOffsets[0].fill(24);m->structuralPlaneOffsets[1].fill(255);
    RemasterDungeonFloorContext c;c.roomIndex=7;
    RemasterFrame f;f.width=256;f.height=224;f.mainPixels.resize(256*224);f.subPixels.resize(256*224);
    RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=1;f.tileInstances.push_back(i);
    for(size_t n=0;n<f.mainPixels.size();n++){f.mainPixels[n].instanceId=1;f.mainPixels[n].tilePixel=(n/256%8)*8+n%8;}
    RemasterLightingTrace::Frame t;t.id=1;
    {RemasterLightingTrace::Scope scope(&t,RemasterLightingTrace::Apply);S9xRemasterApplyDungeonHeightMap(f,c,*m);}
    assert(f.tileInstances[0].heightOffset==24);
    // The producer trace counts exact selection work, not a timing threshold.
    std::cout<<"projection decisions="<<t.projectionDecisions<<" pixels="<<f.mainPixels.size()<<'\n';
    assert(t.projectionDecisions<=f.mainPixels.size()/4);
}
