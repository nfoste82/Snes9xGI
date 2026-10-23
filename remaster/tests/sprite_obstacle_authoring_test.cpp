#include "../runtime/sprite_obstacle_replay.h"
#include "../runtime/lighting_field_cache.h"
#include "../runtime/obstacle_group_editor.h"
#include <cassert>
#include <iostream>
uint32_t *S9xRemasterCurrentOwners=nullptr;
uint32_t S9xRemasterCurrentOwner=REMASTER_OWNER_UNSUPPORTED;
int main(int argc,char **argv)
{
	assert(argc==3);
	RemasterProfile profile;std::vector<RemasterProfileDiagnostic> errors;
	assert(S9xRemasterLoadProfile(argv[2],profile,errors));
	RemasterFrame frame;assert(S9xReadRemasterFrame(argv[1],frame));
	if(const char *path=std::getenv("S9X_REMASTER_DEBUG_PROBE_LIVE_RGBA")) {
		RemasterLightingFieldCache cache;cache.update(frame,false);
		std::ifstream file(path,std::ios::binary);std::vector<uint8_t> rgba{std::istreambuf_iterator<char>(file),{}};
		assert(rgba.size()==frame.mainPixels.size()*4);size_t checked=0;
		std::set<RemasterTileContentId> members;
		for(const auto &g:profile.obstacleGroups)if(g.spriteAssembly)for(const auto &m:g.members)members.insert(m.tileId);
		const bool height=std::getenv("S9X_REMASTER_DEBUG_PROBE_LIVE_HEIGHT");
		for(size_t p=0;p<frame.mainPixels.size();p++) {
			const auto &px=frame.mainPixels[p];if(!px.instanceId)continue;
			const auto &a=frame.tileInstances[px.instanceId-1];
			if(a.source!=RemasterSourceType::Object || !members.count(a.tileId))continue;
			if(height) { assert(!cache.height[p*2+1]);assert(std::abs(int(rgba[p*4])-56)<=1 && rgba[p*4+1]==0 && std::abs(int(rgba[p*4+2])-71)<=1); }
			else {assert(cache.authoredNormals[p]);for(int c=0;c<3;c++)assert(std::abs(int(rgba[p*4+c])-int(std::lround((cache.surface[p*4+c+1]*0.5f+0.5f)*255)))<=1);}
			checked++;
		}
		assert(checked>0);std::cout<<"LIVE EndFrame -> production Metal "<<(height?"unresolved height":"authored normals")<<" verified "<<checked<<" pixels\n";return 0;
	}
	{size_t height=0,normals=0,unresolved=0;std::set<RemasterTileContentId> ids;
		for(const auto &a:frame.tileInstances)if(a.source==RemasterSourceType::Object && a.sourceIndex>=116 && a.sourceIndex<=121) {
			ids.insert(a.tileId);unresolved+=S9xRemasterInstanceGeometryUnresolved(a);
		}
		for(const auto &id:ids)if(const auto *m=S9xRemasterFrameMetadataForTile(frame,id)){height+=m->hasHeight;normals+=m->hasNormals;}
		std::cout<<"Capture metadata unique identities="<<ids.size()<<" height="<<height<<" normals="<<normals<<" unresolved records="<<unresolved<<'\n';
	}
	if(std::none_of(profile.obstacleGroups.begin(),profile.obstacleGroups.end(),[](const RemasterObstacleGroup &g){return g.spriteAssembly;})) {
		// Read-only historical fixture: current disk profile may predate the
		// unsaved assembly. Reconstruct the observed group ONLY in test memory.
		using Part=std::tuple<int,int,RemasterTileContentId,bool,bool>;std::set<Part> parts;
		for(size_t p=0;p<frame.mainPixels.size();p++) {
			const auto &px=frame.mainPixels[p];if(!px.instanceId || px.tilePixel>=64)continue;
			const auto &a=frame.tileInstances[px.instanceId-1];if(a.source!=RemasterSourceType::Object || a.sourceIndex<116 || a.sourceIndex>121)continue;
			parts.emplace(int(p%frame.width)-(a.hFlip?7-px.tilePixel%8:px.tilePixel%8),int(p/frame.width)-(a.vFlip?7-px.tilePixel/8:px.tilePixel/8),a.tileId,a.hFlip,a.vFlip);
		}
		assert(parts.size()==24);int bottom=-1000;for(const auto &p:parts)bottom=std::max(bottom,std::get<1>(p));
		const int ox=std::get<0>(*parts.begin()),oy=std::get<1>(*parts.begin());RemasterObstacleGroup g;g.name="test_only_observed";g.spriteAssembly=true;
		for(const auto &p:parts)g.members.push_back({std::get<2>(p),(std::get<0>(p)-ox)/8,(std::get<1>(p)-oy)/8,0,std::get<3>(p),std::get<4>(p),std::get<1>(p)==bottom});
		profile.obstacleGroups.push_back(g);std::cout<<"No disk sprite group; test-only observed 24-member fixture, no profile writes\n";
	}
	if(std::getenv("ALTTP_SPRITE_TEST_LAYERS")) {
		std::set<RemasterTileContentId> ids;for(const auto &g:profile.obstacleGroups)if(g.spriteAssembly)for(const auto &m:g.members)ids.insert(m.tileId);
		size_t n=0;for(const auto &id:ids){auto &a=profile.assets[id];a.tileId=id;a.hasHeight=bool(n++%2);a.height.fill(23);a.hasNormals=true;a.normalXyz.fill(128);}
		std::cout<<"Explicit TEST-ONLY partial local layers; no geometry approval or profile writes\n";
	}
	const bool restored=S9xRemasterRestoreSpriteObstacleSupport(argv[1],frame,profile);
	for(const auto &group:profile.obstacleGroups)if(group.spriteAssembly) {
		RemasterObstacleGroup completed;
		if(S9xRemasterObstacleEditorCompleteSpriteFootprint(frame,group,completed)) {
			assert(group.members.size()==16 && completed.members.size()==24);
			for(size_t i=0;i<group.members.size();i++) {
				const auto &a=group.members[i],&b=completed.members[i];
				assert(a.tileId==b.tileId && a.x==b.x && a.y==b.y && a.z==b.z && a.hflip==b.hflip && a.vflip==b.vflip);
			}
			assert(std::count_if(completed.members.begin(),completed.members.end(),[](const auto &m){return m.supportContact;})==6);
			std::cout<<"Review-only completion: 16 -> 24 occurrences, six boundary contacts; existing Z/coordinates/flips preserved\n";
			if(std::getenv("S9X_REMASTER_DEBUG_PROBE_COMPLETE")) {
				profile.obstacleGroups[&group-profile.obstacleGroups.data()]=completed;
				std::cout<<"Explicit TEST-ONLY completion in memory; disk profile unchanged\n";
			}
		}
	}
	S9xRemasterApplyProfileToFrame(profile,frame);
	RemasterSpriteObstacleDiagnostics d;
	S9xRemasterApplySpriteObstacles(frame,profile,nullptr,nullptr,&d);
	std::cout<<"Observed tiles="<<d.observedTiles<<" matched="<<d.matchedAssemblies<<" claimed records="<<d.claimedInstances<<" unclaimed="<<d.unclaimedInstances<<" incomplete candidates="<<d.incompleteCandidates<<'\n';
	for(const auto &a:d.assemblies)std::cout<<a.group<<" members="<<a.members<<" contacts="<<a.contacts<<" status="<<int(a.status)<<'\n';
	for(bool scene:{false,true}) {
		if(scene && frame.scenePixels.empty())continue;
		RemasterLightingFieldCache cache;cache.update(frame,scene);size_t pixels=0,normals=0,participating=0;const auto &pp=scene?frame.scenePixels:frame.mainPixels;
		for(size_t p=0;p<pp.size();p++)if(pp[p].instanceId){const auto &a=frame.tileInstances[pp[p].instanceId-1];if(a.source==RemasterSourceType::Object && a.sourceIndex>=116 && a.sourceIndex<=121){pixels++;normals+=cache.authoredNormals[p]!=0;participating+=cache.participation[p*2]!=0;}}
		std::cout<<(scene?"scene":"main")<<" blocker pixels="<<pixels<<" normals="<<normals<<" participating="<<participating<<'\n';
		if(!scene)if(const char *prefix=std::getenv("S9X_REMASTER_DEBUG_PROBE_OUTPUT")) {
			auto read=[&](const char *suffix){std::ifstream f(std::string(prefix)+suffix,std::ios::binary);return std::vector<uint8_t>{std::istreambuf_iterator<char>(f),{}};};
			const auto normal=read(".normals.rgba"),height=read(".height.rgba");
			assert(normal.size()==pp.size()*4 && height.size()==pp.size()*4);size_t checked=0;
			for(size_t p=0;p<pp.size();p++)if(pp[p].instanceId) {
				const auto &a=frame.tileInstances[pp[p].instanceId-1];
				if(a.source!=RemasterSourceType::Object || a.sourceIndex<116 || a.sourceIndex>121)continue;
				for(int c=0;c<3;c++)assert(std::abs(int(normal[p*4+c])-int(std::lround((cache.surface[p*4+c+1]*0.5f+0.5f)*255)))<=1);
				if(S9xRemasterInstanceGeometryUnresolved(a)){assert(std::abs(int(height[p*4])-56)<=1 && height[p*4+1]==0 && std::abs(int(height[p*4+2])-71)<=1);}
				else if(cache.height[p*2+1]) {
					const int expected=int(std::lround(std::clamp(cache.height[p*2]*255/64*frame.heightPreviewMultiplier/8,0.0f,1.0f)*255));
					for(int c=0;c<3;c++)assert(std::abs(int(height[p*4+c])-expected)<=1);
				}
				checked++;
			}
			std::cout<<"PRODUCTION METAL readback verified "<<checked<<" blocker normals/height pixels\n";
		}
	}
	assert(d.matchedAssemblies==1);
	if(!restored || !d.resolvedAssemblies) {
		assert(!d.resolvedAssemblies);
		RemasterLightingFieldCache cache;cache.update(frame,false);size_t normals=0,localHeights=0;
		for(size_t p=0;p<frame.mainPixels.size();p++) {
			const auto id=frame.mainPixels[p].instanceId;if(!id)continue;
			const auto &a=frame.tileInstances[id-1];if(a.source!=RemasterSourceType::Object || a.sourceIndex<116 || a.sourceIndex>121)continue;
			assert(!cache.height[p*2+1]);normals+=cache.authoredNormals[p]!=0;
			const auto *m=S9xRemasterFrameMetadataForTile(frame,a.tileId);if(m && m->hasHeight)localHeights++;
		}
		std::cout<<"Support restored="<<restored<<"; unresolved assembly: authored normals visible="<<normals<<" pixels; local height metadata="<<localHeights
			<<" pixels; absolute height remains unresolved (correct rejection)\n";return 0;
	}
	assert(d.resolvedAssemblies==1);
	const auto assembly=d.assemblies[0];
	std::cout<<"Selected support base="<<assembly.base<<" plane="<<assembly.plane<<" actor="<<assembly.producerActor<<" configured rise="<<unsigned(profile.upperFloorHeight)<<'\n';
	assert(assembly.base!=255 && assembly.plane==0 && assembly.producerActor==0);
	RemasterLightingFieldCache cache;cache.update(frame,false);
	size_t height=0,normals=0,missing=0,claimed=0;
	for(size_t p=0;p<frame.mainPixels.size();p++) {
		const auto &pixel=frame.mainPixels[p];if(!pixel.instanceId)continue;
		const auto &a=frame.tileInstances[pixel.instanceId-1];
		if(a.source!=RemasterSourceType::Object || a.placementTileRole!=RemasterTileRole::Obstacle)continue;
		claimed++;const auto *m=S9xRemasterFrameMetadataForTile(frame,a.tileId);assert(m);
		const auto g=S9xRemasterResolveFrameGeometry(*m,frame,pixel.instanceId-1);
		if(g.hasHeight) {height++;assert(cache.height[p*2+1]);assert(std::abs(cache.height[p*2]-(assembly.base+g.height[pixel.tilePixel])/255.0f)<0.00001f);}
		else {missing++;assert(!cache.height[p*2+1]);}
		if(g.hasNormals){normals++;assert(cache.authoredNormals[p]);}
	}
	assert(height && normals);
	std::vector<uint8_t> encoded;assert(S9xSerializeRemasterFrame(frame,encoded));
	RemasterFrame decoded;assert(S9xDeserializeRemasterFrame(encoded,decoded));
	assert(!decoded.spriteObstacleSupport);
	assert(S9xRemasterRestoreSpriteObstacleSupport(argv[1],decoded,profile));
	S9xRemasterApplyProfileToFrame(profile,decoded);
	S9xRemasterApplySpriteObstacles(decoded,profile,nullptr,nullptr,&d);
	assert(d.resolvedAssemblies==1 && d.assemblies[0].base==assembly.base);
	// No sidecars, no invented support, even after a formerly solved capture.
	assert(!S9xRemasterRestoreSpriteObstacleSupport(std::string(argv[1])+".absent",decoded,profile));
	S9xRemasterApplySpriteObstacles(decoded,profile,nullptr,nullptr,&d);
	assert(!d.resolvedAssemblies);
	std::cout<<"claimed pixels="<<claimed<<" height="<<height<<" normals="<<normals<<" missing local="<<missing
		<<" missing local records="<<assembly.missingLocalInstances<<"; replay roundtrip and absent-sidecar rejection passed\n";
}
