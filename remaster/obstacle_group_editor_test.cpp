#include "obstacle_group_editor.h"
#include <cassert>
#include <iostream>

int main()
{
	RemasterTileContentId repeated{123,1,4}, foot{456,1,4};
	std::vector<RemasterObstacleEditorCell> cells{
		{repeated,40,16,1,false,false},{repeated,40,24,1,false,false},
		{foot,40,32,1,true,false}};
	RemasterObstacleGroup group; std::string error;
	assert(S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	assert(group.members.size()==3 && group.members[0].y==0 && group.members[2].y==2);
	assert(!group.members[0].supportContact && !group.members[1].supportContact && group.members[2].supportContact);
	assert(group.members[0].z==0 && group.members[2].hflip);
	assert(!S9xRemasterObstacleEditorBuild(cells,{group},"second",group,error));
	cells[1].background=0;
	assert(!S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	cells[1].background=1; cells[1].x++;
	assert(!S9xRemasterObstacleEditorBuild(cells,{},"column",group,error));
	// Rasterizer canonical pixels recover the same occurrence origin under flips.
	RemasterFrame frame; frame.width=frame.height=16;
	frame.mainPixels.resize(256); frame.subPixels.resize(256);
	RemasterFrameTileInstance instance; instance.tileId=repeated;
	instance.source=RemasterSourceType::Background; instance.sourceIndex=1;
	for (int flip=0;flip<4;flip++)
	{
		instance.hFlip=flip&1; instance.vFlip=flip&2; frame.tileInstances={instance};
		frame.mainPixels[6*16+5].instanceId=1;
		frame.mainPixels[6*16+5].tilePixel=(flip&2?1:6)*8+(flip&1?2:5);
		RemasterObstacleEditorCell cell;
		assert(S9xRemasterObstacleEditorCellAt(frame,5,6,cell));
		assert(cell.x==0 && cell.y==0 && cell.hflip==instance.hFlip && cell.vflip==instance.vFlip);
	}
	std::cout<<"PASS occurrence footprint authoring, repeated identities, contacts, ownership rejection, raster flips\n";
}
