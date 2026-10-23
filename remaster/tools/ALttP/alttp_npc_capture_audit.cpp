// Read-only OBJ artwork observations using the actual versioned frame decoder.
#include "../../runtime/frame.h"
#include <iostream>
#include <set>

static void jsonString(const char *value)
{
	std::cout<<'"';
	for(const unsigned char *p=reinterpret_cast<const unsigned char *>(value);*p;p++){
		if(*p=='"' || *p=='\\')std::cout<<'\\'<<char(*p);
		else if(*p<32){const char *hex="0123456789abcdef";std::cout<<"\\u00"<<hex[*p>>4]<<hex[*p&15];}
		else std::cout<<char(*p);
	}
	std::cout<<'"';
}

int main(int argc,char **argv)
{
	if(argc<2)return 2;
	std::cout<<"[";
	for(int arg=1;arg<argc;arg++){
		RemasterFrame frame;
		if(!S9xReadRemasterFrame(argv[arg],frame)){std::cerr<<"Cannot decode "<<argv[arg]<<'\n';return 1;}
		std::set<RemasterTileContentId> ids;
		for(const auto &instance:frame.tileInstances)if(instance.source==RemasterSourceType::Object)ids.insert(instance.tileId);
		// A representative scene omits later animation instances. Expand only
		// tracks intersecting observed OBJ artwork; do not scan all BG assets.
		const auto observed=ids;
		for(const auto &group:frame.assetGroups)if(group.name.find("capture_animation_")==0 &&
			std::any_of(group.tileIds.begin(),group.tileIds.end(),[&](const auto &id){return observed.count(id);}))
			ids.insert(group.tileIds.begin(),group.tileIds.end());
		std::cout<<(arg>1?",":"")<<"{\"capture\":";jsonString(argv[arg]);std::cout<<",\"tiles\":[";
		bool first=true;
		for(const auto &id:ids){
			const auto *metadata=S9xRemasterFrameMetadataForTile(frame,id);
			std::cout<<(first?"":",")<<"{\"tile_hash\":\""<<RemasterProfileSerialization::TileId(id)
				<<"\",\"has_height\":"<<(metadata && metadata->hasHeight?"true":"false")
				<<",\"has_normals\":"<<(metadata && metadata->hasNormals?"true":"false")<<"}";
			first=false;
		}
		std::cout<<"]}";
	}
	std::cout<<"]\n";
}
