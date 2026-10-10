// Replays emitter discovery from captured current room inputs and existing
// published bases. Does not run or approve a room-height solve.
#include "remaster.h"
#include "hidden_emitter_samples.h"
#include "lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=0;
static void require(bool ok,const char *s){if(!ok)throw std::runtime_error(s);}
static std::vector<uint8_t> read(const std::string &p){std::ifstream f(p,std::ios::binary);require(bool(f),"missing sidecar");return {std::istreambuf_iterator<char>(f),{}};}
int main(int argc,char **argv){try {
	require(argc>=2,"usage: hud_emitter_capture_test capture...");
	for(int arg=1;arg<argc;arg++) {
		const std::string path=argv[arg];
		auto frame=std::make_unique<RemasterFrame>();require(S9xReadRemasterFrame(path,*frame),"packet");
		const auto original=*frame;
		auto input=read(path+".dungeon-input.bin"),json=read(path+".dungeon.json");
		require(input.size()==90120 && !std::memcmp(input.data(),"ALTPDI1\0",8),"current complete inputs");
		std::string text(json.begin(),json.end());
		auto number=[&](const char *key,int index=0){auto at=text.find(std::string("\"")+key+"\":");require(at!=std::string::npos,"missing state");at=text.find(':',at)+1;if(text[at]=='[')at++;for(int n=0;n<index;n++)at=text.find(',',at)+1;return std::stoi(text.substr(at));};
		RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
		require(S9xRemasterLoadProfile(path+".profile.toml",profile,errors),"pinned profile");
		RemasterDungeonFloorContext c;c.verifiedAlttpRom=c.indoors=true;
		if(text.find("\"module\":")!=std::string::npos)c.module=number("module");
		c.submodule=number("submodule");
		if(text.find("\"construction_phase\":")!=std::string::npos)c.constructionPhase=number("construction_phase");
		c.roomIndex=number("room");c.collisionMode=number("collision_mode");
		c.backgroundScrollX=number("scroll");c.backgroundScrollY=number("scroll",1);
		c.graphicsBase=number("graphics_base");c.bg1GraphicsBase=number("bg1_graphics_base");
		c.constructedMaps=input.data()+8;c.graphics=input.data()+24584;
		auto map=std::make_unique<RemasterDungeonHeightMap>();map->roomIndex=c.roomIndex;
		auto at=text.find("\"visible_bases\":[");require(at!=std::string::npos,"published placement bases");at=text.find('[',at)+1;
		for(int cell=0;cell<4096;cell++){map->offsets[cell]=std::stoi(text.substr(at));at=text.find(',',at)+1;}
		RemasterLightingFieldCache before,after;before.update(*frame,false);
		frame->lights.clear();frame->hiddenEmitters.clear();
		const auto d=S9xRemasterAppendOffscreenRoomLights(*frame,c,profile,map.get(),nullptr,S9xRemasterHashTile);
		after.update(*frame,false);
		require(frame->originalRgb555==original.originalRgb555 && frame->sceneRgb555==original.sceneRgb555 &&
			frame->mainPixels.size()==original.mainPixels.size(),"stock artwork preserved");
		for(size_t n=0;n<frame->mainPixels.size();n++)for(auto buffers:{std::make_pair(static_cast<const std::vector<RemasterFramePixel> *>(&frame->mainPixels),&original.mainPixels),std::make_pair(static_cast<const std::vector<RemasterFramePixel> *>(&frame->subPixels),&original.subPixels)}) {
			const auto &a=(*buffers.first)[n],&b=(*buffers.second)[n];
			require(a.owner==b.owner && a.instanceId==b.instanceId && a.tilePixel==b.tilePixel,"stock composition preserved");
		}
		require(before.emission==after.emission && before.surface==after.surface && before.occlusion==after.occlusion &&
			before.participation==after.participation,"visible fields unchanged");
		double power=0;size_t top=0;
		for(const auto &e:frame->hiddenEmitters) {
			require(S9xRemasterHeldLampHudPixel(*frame,size_t(e.y)*frame->width+size_t(e.x)),"hidden sources only at HUD ink");
			top+=e.x<48 && e.y<64;
			power+=e.emission[3];
		}
		std::vector<uint8_t> bytes;RemasterFrame decoded;
		require(S9xSerializeRemasterFrame(*frame,bytes) && S9xDeserializeRemasterFrame(bytes,decoded) &&
			decoded.hiddenEmitters.size()==frame->hiddenEmitters.size(),"source-only replay");
		if(path.find("_052.")!=std::string::npos)require(top>0,"reported covered top fixture recovered");
		std::cout<<path<<" hidden="<<d.hiddenPixels<<" top_fixture="<<top<<" summed_intensity="<<power
			<<" samples="<<S9xRemasterHiddenDirectSamples(*frame).size()<<" stock/fields unchanged PASS\n";
		if(const char *directory=std::getenv("S9X_HUD_WRITE_REPLAY"))require(S9xWriteRemasterFrame(*frame,
			std::string(directory)+"/hud-"+path.substr(path.find_last_of('/')+1)),"write");
	}
	return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
