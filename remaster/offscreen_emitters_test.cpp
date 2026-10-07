#include "remaster.h"
#include <cstdio>
#include <cstdlib>
#include <memory>

uint32_t *S9xRemasterCurrentOwners = nullptr;
uint32_t S9xRemasterCurrentOwner = 0;
static void require(bool ok, const char *what)
{
	if (!ok) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); }
}
int main()
{
	auto maps=std::make_unique<std::array<uint8_t,16384>>();
	auto graphics=std::make_unique<std::array<uint8_t,65536>>();
	// A nonuniform tile proves canonical decoding and both placement flips.
	(*graphics)[32]=0x80;
	uint8_t indices[64]={}; indices[0]=1;
	RemasterTileContentId id={S9xRemasterHashTile(4,indices),1,4};
	RemasterProfile profile;
	profile.romSha256=std::string(64,'a');
	auto &a=profile.assets[id]; a.tileId=id; a.hasEmission=a.hasHeight=true;
	a.emissionDepth=2; a.height[0]=32;
	a.emissionRgba[0]=255; a.emissionRgba[1]=128; a.emissionRgba[3]=25;
	RemasterDungeonFloorContext c;
	c.verifiedAlttpRom=c.indoors=true; c.roomIndex=0x61;
	c.constructedMaps=maps->data(); c.graphics=graphics->data();
	const int cell=10*64+40;
	(*maps)[cell*2]=1; (*maps)[cell*2+1]=0xc0;
	(*maps)[8192+cell*2]=1; (*maps)[8193+cell*2]=0xc0;
	auto height=std::make_unique<RemasterDungeonHeightMap>();
	height->roomIndex=c.roomIndex; height->offsets[cell]=48;
	RemasterFrame frame; frame.width=256; frame.height=224;
	frame.originalRgb555.resize(256*224); frame.mainPixels.resize(256*224); frame.subPixels.resize(256*224);
	auto run=[&]() { frame.lights.clear(); return S9xRemasterAppendOffscreenRoomLights(frame,c,profile,height.get(),height.get(),S9xRemasterHashTile); };
	require(run().lights==1,"offscreen source discovered before any visible observation; mirrored layers deduplicated");
	auto light=frame.lights[0];
	require(light.x==327.5f && light.y==87.5f,"canonical emission mask follows both flips");
	require(std::fabs(light.z-(80*16.0f/255+1))<0.0001f,"existing signed base plus authored relief and emission depth");
	require(light.intensity==25 && light.red==1 && std::fabs(light.green-128/255.0f)<0.0001f,"radiance/color calibration");
	std::vector<uint8_t> bytes; RemasterFrame decoded;
	require(S9xSerializeRemasterFrame(frame,bytes) && S9xDeserializeRemasterFrame(bytes,decoded),"offscreen source survives portable capture/replay");
	require(decoded.lights.size()==1 && decoded.lights[0].x==light.x,"capture retains outside coordinates");
	c.backgroundScrollX=128;
	require(run().lights==0,"visible source exclusively uses pixel emitter path");
	c.backgroundScrollX=400;
	require(run().lights==1 && frame.lights[0].x==-72.5f,"signed source left of viewport");
	c.submodule=1; require(run().lights==0,"transition never reuses settled room sources"); c.submodule=0;
	c.constructionPhase=1; require(run().lights==0,"unfinished construction excluded"); c.constructionPhase=0;
	c.verifiedAlttpRom=false; require(run().lights==0,"unverified ROM excluded"); c.verifiedAlttpRom=true;
	height->producerHashes[cell]=id.hash+1; require(run().lights==0,"stale placement producer excluded"); height->producerHashes[cell]=0;
	height->offsets[cell]=-48; require(run().lights==1 && frame.lights[0].z<0,"signed floor bases remain signed"); height->offsets[cell]=48;
	height->offsets[cell]=255; require(run().unknownBases==1 && frame.lights.empty(),"unknown base cannot produce a stale light"); height->offsets[cell]=48;
	a.emissionDepth=0; require(run().lights==0,"directional surface is not promoted to a sphere"); a.emissionOmnidirectional=true;
	require(run().lights==1,"omnidirectional source supports zero depth");
	(*graphics)[32]=0; require(run().lights==0,"current animation graphics replace prior identity without cache"); (*graphics)[32]=0x80;
	(*maps)[cell*2]=0; (*maps)[8192+cell*2]=0; require(run().lights==0,"extinguished/removed tile does not persist");
	// Two linked parts of a fixture: combine missing pixels, retaining emitting
	// area and linear RGB power. Runtime graphics animation selects current links.
	(*graphics)[64]=0x40;
	indices[0]=0; indices[1]=1;
	const RemasterTileContentId peer={S9xRemasterHashTile(4,indices),1,4};
	a.combineEmitters={peer};
	auto b=a; b.tileId=peer; b.combineEmitters={id};
	b.emissionRgba.fill(0); b.emissionRgba[4+2]=255; b.emissionRgba[4+3]=50;
	profile.assets[peer]=b;
	(*maps)[cell*2]=1; (*maps)[cell*2+1]=0;
	(*maps)[(cell+1)*2]=2;
	height->offsets[cell+1]=48;
	c.backgroundScrollX=0;
	frame.representativeEmitters=0;
	require(run().lights==2,"combining off preserves separate missing-pixel sources");
	frame.representativeEmitters=1;
	require(run().lights==1,"reciprocal adjacent offscreen tiles combine");
	light=frame.lights[0];
	require(std::fabs(4*3.14159265358979323846f*light.radius*light.radius-2)<0.00001f &&
		light.intensity==37.5f,"combined sphere preserves total area and power");
	require(std::fabs(std::pow(light.red,2.2f)*75-25)<0.0001f &&
		std::fabs(std::pow(light.blue,2.2f)*75-50)<0.0001f,"combined linear RGB power preserved");
	profile.assets[peer].combineEmitters.clear();
	require(run().lights==2,"one-way offscreen links do not combine");
	profile.assets[peer].combineEmitters={id};
	profile.assets[peer].emissionOmnidirectional=false; profile.assets[peer].emissionDepth=2;
	require(run().lights==2,"different emission-direction modes remain separate");
	profile.assets[peer].emissionOmnidirectional=true;
	(*maps)[(cell+1)*2]=0; (*maps)[8192+(cell+1)*2]=2;
	require(run().lights==2,"offscreen sources on different BG planes remain separate");
	(*maps)[8192+(cell+1)*2]=0; (*maps)[(cell+1)*2]=2;
	(*maps)[(cell+1)*2]=0; (*maps)[(cell+3)*2]=2; height->offsets[cell+3]=48;
	require(run().lights==2,"disconnected offscreen copies never combine remotely");
	(*maps)[(cell+3)*2]=0; (*maps)[(cell+1)*2]=2;
	const RemasterTileContentId next={id.hash+123,1,4};
	auto nextAsset=a; nextAsset.tileId=next; profile.assets[next]=nextAsset;
	profile.assets[peer].combineEmitters.push_back(next);
	frame.lights.clear();
	auto animatedHash=[&](uint8_t depth,const uint8_t *pixels) {
		const auto hash=S9xRemasterHashTile(depth,pixels); return hash==id.hash ? next.hash : hash;
	};
	require(S9xRemasterAppendOffscreenRoomLights(frame,c,profile,height.get(),height.get(),animatedHash).lights==1,
		"animated identity uses its own authored reciprocal combination links");
	profile.assets[next].combineEmitters.clear(); frame.lights.clear();
	require(S9xRemasterAppendOffscreenRoomLights(frame,c,profile,height.get(),height.get(),animatedHash).lights==2,
		"animation does not reuse previous frame links");
	RemasterHeldLampState lamp; lamp.verifiedRom=lamp.darkWithLantern=true;
	lamp.facing=6; lamp.linkScreenX=100; lamp.linkScreenY=80; lamp.floorHeight=3;
	S9xRemasterSetProfile(profile); S9xRemasterSetLiveFramesEnabled(true);
	std::vector<uint16_t> screen(256*224,0x1234);
	S9xRemasterBeginFrame(screen.size(),256,256,224);
	RemasterFrame completed;
	S9xRemasterEndFrame(screen.data(),256,256,224,&completed,nullptr,&lamp);
	require(completed.lights.size()==1 && completed.lights[0].x==120 && completed.lights[0].z==15,
		"EndFrame appends held lamp before publication");
	require(completed.originalRgb555[0]==0x1234,"original HUD/scene RGB remains stock");
	lamp.darkWithLantern=false;
	S9xRemasterBeginFrame(screen.size(),256,256,224);
	S9xRemasterEndFrame(screen.data(),256,256,224,&completed,nullptr,&lamp);
	require(completed.lights.empty(),"completed lamp state never persists after cone flag clears");
	std::puts("Offscreen room emitter checks passed");
}
