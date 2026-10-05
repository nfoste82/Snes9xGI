// Ordered ownership witnesses, independent of the external capture fixture.
#include "remaster.h"
#include <cassert>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
int main()
{
	auto words=std::make_unique<std::array<uint8_t,16384>>();
	auto rom=std::make_unique<std::array<uint8_t,12876>>();
	auto graphics=std::make_unique<std::array<uint8_t,65536>>();
	auto tiles=std::make_unique<std::array<RemasterDungeonFloorContext::Placement,4096>>();
	RemasterDungeonFloorContext c;c.verifiedAlttpRom=true;c.constructedMaps=words->data();
	c.graphics=graphics->data();c.graphicsBase=c.bg1GraphicsBase=0;c.dungeonTemplateData=rom->data();
	RemasterProfile p;
	auto put=[](auto &a,int i,int v){a[i*2]=v;a[i*2+1]=v>>8;};
	// A complete nine-word surviving low-corner triangle. Other seven words
	// are overwritten; this must not assign topology or promote solver roles.
	for(int n=0;n<16;n++)put(*rom,0xc06/2+n,100+n);
	for(int yy=0;yy<3;yy++)for(int xx=0;xx<4-yy;xx++)
		put(*words,(12+yy)*64+20+xx,100+xx*4+yy);
	S9xRemasterDungeonPlacements(c,p,*tiles);
	assert((*tiles)[13*64+22].diagnosticWallTemplateOffset==0xc06);
	assert((*tiles)[13*64+22].surface==RemasterDungeonFloorContext::Surface::Unknown);
	put(*words,12*64+20,999); // one required template word missing
	S9xRemasterDungeonPlacements(c,p,*tiles);
	assert(!(*tiles)[13*64+22].diagnosticWallTemplateOffset);
	// Exact perimeter companion row proves one truncated row, but cannot
	// recursively bless a further strip or accept an overwritten trim word.
	for(int n=0;n<8;n++)put(*rom,0x2c8/2+n,200+n);
	for(int yy=0;yy<3;yy++)for(int xx=0;xx<4;xx++)
		put(*words,(30+yy)*64+10+xx,200+(yy%2)*4+xx);
	S9xRemasterDungeonPlacements(c,p,*tiles);
	assert((*tiles)[32*64+10].diagnosticWallTemplateOffset==0x2c8);
	put(*words,32*64+13,999);
	S9xRemasterDungeonPlacements(c,p,*tiles);
	assert(!(*tiles)[32*64+10].diagnosticWallTemplateOffset);
	// Carpet trim needs a minimum four-word ROM run, not neutral artwork.
	put(*rom,0x6c8/2,321);
	for(int x=40;x<44;x++)put(*words,40*64+x,321);
	put(*words,42*64+40,321);
	S9xRemasterDungeonPlacements(c,p,*tiles);
	assert((*tiles)[40*64+40].diagnosticRomFloorTrim);
	assert(!(*tiles)[42*64+40].diagnosticRomFloorTrim);
	assert((*tiles)[40*64+40].surface==RemasterDungeonFloorContext::Surface::Unknown);
}
