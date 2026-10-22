// Complete-room cache regression plus explicit authored-floor denominators.
#define main cacheCorpusMain
#include "dungeon_geometry_cache_probe.cpp"
#undef main
int main(int argc,char **argv)
{
	std::ostringstream output;auto *old=std::cout.rdbuf(output.rdbuf());
	const int status=cacheCorpusMain(argc,argv);std::cout.rdbuf(old);if(status)return status;
	const auto attributes=read(argv[2]);const auto &s=S9xRemasterState();
	size_t required=0,missing=0;
	for(int bg=0;bg<2;bg++) {
		const auto &tiles=bg?s.dungeonRetainedBg2:s.dungeonRetainedBg1;
		const auto &map=bg?s.runtimeDungeonHeightMap:s.runtimeDungeonBg1HeightMap;
		if(!tiles)continue;
		for(int p=0;p<2;p++)for(int i=0;i<4096;i++) {
			const auto &t=tiles->at(i);
			if(!t.diagnosticAuthoredFloor || !t.hasHeight || attributes[10+p*4096+i]!=0)continue;
			// Declared audit scope includes unobserved planes; these stay missing.
			required++;missing+=map.planeOffsets[p][i]==255;
		}
	}
	auto json=output.str();const auto end=json.find_last_of('}');check(end!=std::string::npos,"corpus output");
	json.insert(end,",\"authored_floor_required\":"+std::to_string(required)+",\"authored_floor_missing\":"+std::to_string(missing));
	std::cout<<json;
}
