#include "remaster.h"
#include <cassert>

int main()
{
	std::array<uint8_t,8192> attributes = {};
	RemasterDungeonFloorContext c; c.collisionAttributes=attributes.data();
	for (int kind : {0x1d,0x1e,0x1f,0x3d,0x3e,0x3f}) {
		attributes.fill(0); c.stairCount=0;
		for (int p=0;p<2;p++) for (int y=1;y<=2;y++) {
			for (int x=1;x<=2;x++) attributes[p*4096+(42+y)*64+46+x]=kind;
			attributes[p*4096+(42+y)*64+46]=1;
			attributes[p*4096+(42+y)*64+49]=1;
		}
		// A later writer covers one plane's side (actual room060 case).
		attributes[43*64+46]=attributes[43*64+49]=28;
		S9xRemasterDungeonStairsFromAttributes(c);
		assert(c.stairCount==1 && c.stairs[0].x==46 && c.stairs[0].y==42);
		assert(c.stairs[0].changesPlane==((kind&15)!=13));
		assert(c.stairs[0].highIsNorth==(kind==0x1d || kind==0x1e || kind==0x1f || kind==0x3f));
		S9xRemasterDungeonStairsFromAttributes(c); assert(c.stairCount==1);
		// Completed handler signature corrects stale table-derived orientation
		// without losing the table index or creating a duplicate assembly.
		c.stairs[0].highIsNorth=!c.stairs[0].highIsNorth;
		c.stairs[0].changesPlane=!c.stairs[0].changesPlane;
		c.stairs[0].tableEntry=7;
		S9xRemasterDungeonStairsFromAttributes(c);
		assert(c.stairCount==1 && c.stairs[0].tableEntry==7);
		assert(c.stairs[0].highIsNorth==(kind==0x1d || kind==0x1e || kind==0x1f || kind==0x3f));
		assert(c.stairs[0].changesPlane==((kind&15)!=13));
		c.stairCount=0; attributes[4096+44*64+48]=0;
		S9xRemasterDungeonStairsFromAttributes(c); assert(c.stairCount==0);
	}
}
