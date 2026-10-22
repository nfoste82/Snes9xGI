// Pinned capture -> actual production EndFrame, both complete BG maps and pixels.
#define main captureProbeMain
#include "dungeon_geometry_cache_capture_probe.cpp"
#undef main
int main(int argc,char **argv)
{
	if(captureProbeMain(argc,argv))return 1;
	const auto &s=S9xRemasterState();const auto &m=s.runtimeDungeonHeightMap;
	const std::set<uint64_t> hashes={UINT64_C(0x773dcf4a53f87b93),UINT64_C(0x0c2a4c549a67b13f)};
	size_t placements=0,narrow=0,pixels=0;
	for(int i=0;i<4096;i++)if(hashes.count(m.producerHashes[i])) {
		check(m.planeOffsets[0][i]==50 && m.offsets[i]==50 && !m.fallbackDirections[i],"floor relief base");placements++;
		if(i/64>=7 && i/64<=10 && (i%64==23 || i%64==40))narrow++;
	}
	check(placements==64 && narrow==8,"complete floor occurrence denominators");
	auto packet=std::make_unique<RemasterFrame>();
	check(S9xReadRemasterFrame(std::string(argv[2])+".s9xrmf",*packet),"produced packet");
	for(auto buffer:{&packet->mainPixels,&packet->subPixels,&packet->scenePixels})for(const auto &px:*buffer)if(px.instanceId) {
		const auto &i=packet->tileInstances.at(px.instanceId-1);
		if(i.source!=RemasterSourceType::Background || i.sourceIndex!=1 || !hashes.count(i.tileId.hash))continue;
		check(i.heightOffset==50 && !i.hasPlacementHeight,"actual floor pixel composition");
		const auto &a=s.activeProfile.assets.at(i.tileId);
		bool found=false;for(const auto &metadata:packet->assetMetadata)if(metadata.tileId==i.tileId) {
			check(metadata.hasHeight && metadata.height==a.height && metadata.normalXyz==a.normalXyz,"authored relief/normals changed");found=true;
		}check(found,"local source missing");pixels++;
	}
	check(pixels>0,"floor pixel denominator");
	std::cout<<"PASS floor occurrences="<<placements<<" narrow="<<narrow<<" actual_pixel_usages="<<pixels<<" base=50 authored relief/normals unchanged\n";
}
