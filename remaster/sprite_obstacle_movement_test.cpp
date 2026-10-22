// Validate production benchmark Height readback against its actual final packet.
#include "lighting_field_cache.h"
#include <iostream>
#include <stdexcept>
static void need(bool v,const char *s){if(!v)throw std::runtime_error(s);}
int main(int argc,char **argv){try {
	need(argc==3,"usage: test case-05.s9xrmf live.rgba");RemasterFrame f;
	need(S9xReadRemasterFrame(argv[1],f),"packet");
	std::ifstream input(argv[2],std::ios::binary);std::vector<uint8_t> rgba{std::istreambuf_iterator<char>(input),{}};
	need(rgba.size()==f.mainPixels.size()*4,"readback dimensions");
	RemasterLightingFieldCache cache;cache.update(f,false);size_t pixels=0;
	for(size_t p=0;p<f.mainPixels.size();p++) {
		const auto &px=f.mainPixels[p];if(!px.instanceId)continue;
		const auto &a=f.tileInstances.at(px.instanceId-1);
		if(a.source!=RemasterSourceType::Object || a.placementTileRole!=RemasterTileRole::Obstacle)continue;
		need(!S9xRemasterInstanceGeometryUnresolved(a) && cache.height[p*2+1],"missing absolute height");
		need(cache.authoredNormals[p],"missing authored normal");
		// Height view remains grayscale regardless of its selected preview range;
		// unresolved sentinel purple can never pass this production GPU assertion.
		need(rgba[p*4]==rgba[p*4+1] && rgba[p*4+1]==rgba[p*4+2],"non-grayscale blocker Height pixel");pixels++;
	}
	need(pixels>0,"no visible obstacle pixels");
	std::cout<<"PASS production Height readback "<<pixels<<" resolved authored height/normal pixels\n";
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
