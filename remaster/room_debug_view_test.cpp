#include "room_debug_view.h"
#include "remaster.h"
#include <cassert>

int main()
{
	RemasterFrame f;
	f.width=24;f.height=8;
	f.mainPixels.resize(192);f.mainRoomDiagnostics.resize(192);
	f.tileInstances.resize(3);
	for(auto &i:f.tileInstances){i.source=RemasterSourceType::Background;i.sourceIndex=1;}
	std::vector<uint8_t> rgba(192*4,100);
	for(int y=0;y<8;y++)for(int x=0;x<24;x++) {
		int at=y*24+x,t=x/8;
		f.mainPixels[at].instanceId=t+1;
		f.mainPixels[at].tilePixel=y*8+x%8;
		auto &p=f.mainRoomDiagnostics[at];p.visiblePlane=0;
		auto &d=p.planes[0];d.room=0x60;d.cell=t;d.background=1;
		if(t<2){d.floorId=100+t;d.floorBase=0;}
		else {d.wall=true;d.uphillDirections=1<<3;d.lowerFloorDistance=3;}
	}
	auto image=S9xRemasterDrawRoomDebugView(f,rgba.data());
	assert(image.size()==24*8*16*4);
	assert(image[0]!=image[8*4*4] || image[1]!=image[8*4*4+1] || image[2]!=image[8*4*4+2]);
	bool cyan=false,orange=false,white=false;
	for(size_t i=0;i<image.size();i+=4) {
		cyan|=image[i]==0x40 && image[i+1]==255 && image[i+2]==255;
		orange|=image[i]==255 && image[i+1]==0xb0 && image[i+2]==0x40;
		white|=image[i]==255 && image[i+1]==255 && image[i+2]==255;
	}
	assert(cyan && orange && white);
	// Failed height equalities must not erase a known structural distance.
	for(int y=0;y<8;y++)for(int x=16;x<24;x++)f.mainRoomDiagnostics[y*24+x].planes[0].heightConflict=true;
	image=S9xRemasterDrawRoomDebugView(f,rgba.data());
	bool red=false;white=false;
	for(size_t i=0;i<image.size();i+=4) {
		red|=image[i]==255 && image[i+1]==0x55 && image[i+2]==0x77;
		white|=image[i]==255 && image[i+1]==255 && image[i+2]==255;
	}
	assert(red && white);
	// A recognized wall without an axis has a visible X and unknown label.
	for(int y=0;y<8;y++)for(int x=16;x<24;x++) {
		auto &d=f.mainRoomDiagnostics[y*24+x].planes[0];
		d.heightConflict=false;d.axisUnknown=true;d.uphillDirections=0;d.lowerFloorDistance=-1;
	}
	image=S9xRemasterDrawRoomDebugView(f,rgba.data());
	orange=false;
	for(size_t i=0;i<image.size();i+=4)orange|=image[i]==255 && image[i+1]==0xb0 && image[i+2]==0x40;
	assert(orange);
	// Sprite ownership masks room labels even if diagnostic vectors are stale.
	f.tileInstances[2].source=RemasterSourceType::Object;
	image=S9xRemasterDrawRoomDebugView(f,rgba.data());
	for(int y=0;y<32;y++)for(int x=64;x<96;x++)for(int c=0;c<3;c++)
		assert(image[(y*96+x)*4+c]==100);
	// Shortcut cycle activates live diagnostic production, then stops on exit.
	auto &s=S9xRemasterState();s.requestedDebugMode=RemasterDebugMode::TileTypes;
	assert(S9xRemasterCycleDebugMode()==RemasterDebugMode::FloorWalls);
	assert(s.requestedRoomDiagnostics);
	assert(S9xRemasterCycleDebugMode()==RemasterDebugMode::Original);
	assert(!s.requestedRoomDiagnostics);
}
