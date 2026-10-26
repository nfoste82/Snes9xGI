// Exercise the exact cached runtime solver against a complete linked fixture.
#include "../../runtime/remaster.h"
#include <iostream>
#include <stdexcept>
// Existing portable SHA implementation, kept local to this standalone adapter.
#include "../../../sha256.cpp"

static std::vector<uint8_t> read(const char *path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) throw std::runtime_error("cannot open input");
	return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
static uint64_t digest(const std::vector<uint8_t> &bytes)
{
	uint64_t value = UINT64_C(14695981039346656037);
	for (uint8_t b : bytes) value = (value ^ b) * UINT64_C(1099511628211);
	return value;
}
int main(int argc, char **argv)
{
	try {
		std::vector<uint8_t> rom;
		if(argc>=3 && std::string(argv[argc-2])=="--rom") {rom=read(argv[argc-1]);argc-=2;}
		if (argc < 7 || (argc - 7) % 4) throw std::runtime_error("usage: snapshot attr profile linkX linkY plane [stairX stairY highNorth changesPlane]... [--rom verified-USA-ROM]");
		auto raw = read(argv[1]), attr = read(argv[2]);
		const size_t vramOffset = 14 + 224 * (30 + 256 * 6);
		if (raw.size() != vramOffset + 65536 + 512 || std::memcmp(raw.data(), "ALTPPD1\0", 8) ||
			attr.size() != 8202 || std::memcmp(attr.data(), "ALTPAT1\0", 8)) throw std::runtime_error("invalid complete snapshot/attributes");
		auto word = [&] (size_t i) { return raw[i] | (raw[i + 1] << 8); };
		RemasterDungeonFloorContext context;
		context.verifiedAlttpRom = context.indoors = true; context.roomIndex = word(8);
		if ((attr[8] | (attr[9] << 8)) != context.roomIndex) throw std::runtime_error("attribute room mismatch");
		context.linkX = std::stoi(argv[4]); context.linkY = std::stoi(argv[5]); context.linkPlane = std::stoi(argv[6]);
		context.collisionAttributes = attr.data() + 10; context.graphics = raw.data() + vramOffset;
		context.graphicsBase = word(30) * 2;
		if(!rom.empty()) {
			if(rom.size()!=1048576)throw std::runtime_error("expected unheadered supported ROM");
			unsigned char hash[32];sha256sum(rom.data(),static_cast<unsigned>(rom.size()),hash);
			const unsigned char expected[]={0x66,0x87,0x1d,0x66,0xbe,0x19,0xad,0x2c,0x34,0xc9,0x27,0xd6,0xb1,0x4c,0xd8,0xeb,0x6f,0xc3,0x18,0x19,0x65,0xb6,0xe5,0x17,0xcb,0x36,0x1f,0x73,0x16,0x00,0x9c,0xfb};
			if(std::memcmp(hash,expected,32))throw std::runtime_error("unsupported template ROM fingerprint");
			context.dungeonTemplateData=rom.data()+0x1b52;
		}
		context.bg1GraphicsBase = word(20) * 2;
		if ((word(32) & 3) != 3) throw std::runtime_error("not a complete 64x64 map");
		std::array<uint8_t, 16384> maps = {};
		for (int layer = 0; layer < 2; layer++) for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
		{
			int address = (word(18 + layer * 10) + (y / 32) * 2048 + (x / 32) * 1024 + (y % 32) * 32 + x % 32) & 32767;
			int target = (layer ? 0 : 8192) + (y * 64 + x) * 2;
			maps[target] = context.graphics[address * 2]; maps[target + 1] = context.graphics[address * 2 + 1];
		}
		context.constructedMaps = maps.data();
		for (int i = 7; i < argc; i += 4)
		{
			if (context.stairCount == 32) throw std::runtime_error("too many stairs");
			auto &stair = context.stairs[context.stairCount++];
			stair.x = std::stoi(argv[i]); stair.y = std::stoi(argv[i + 1]);
			stair.highIsNorth = std::stoi(argv[i + 2]); stair.changesPlane = std::stoi(argv[i + 3]);
		}
		S9xRemasterDungeonStairsFromAttributes(context);
		RemasterProfile profile; std::vector<RemasterProfileDiagnostic> errors;
		if (!S9xRemasterLoadProfile(argv[3], profile, errors)) throw std::runtime_error(errors.front().message);
		std::array<RemasterDungeonFloorContext::Placement, 4096> tiles = {};
		S9xRemasterDungeonPlacements(context, profile, tiles); context.placements = tiles.data();
		// Include final tile words in unresolved-structure diagnostics.
		std::array<uint16_t, 4096> tileWords = {};
		for (int i = 0; i < 4096; i++) tileWords[i] = maps[i * 2] | (maps[i * 2 + 1] << 8);
		RemasterDungeonHeightMap map;
		// Reproduce the runtime's other-BG evidence pass before solving BG2.
		auto lowerMap=std::make_unique<RemasterDungeonHeightMap>();
		std::rotate(maps.begin(),maps.begin()+8192,maps.end());
		std::swap(context.graphicsBase,context.bg1GraphicsBase);context.placementBackgroundIndex=0;
		S9xRemasterDungeonPlacements(context,profile,tiles);
		if(!S9xRemasterBuildDungeonHeightMap(context,profile.upperFloorHeight,*lowerMap))throw std::runtime_error("BG1 solve failed");
		std::rotate(maps.begin(),maps.begin()+8192,maps.end());
		std::swap(context.graphicsBase,context.bg1GraphicsBase);context.placementBackgroundIndex=1;
		context.supportRelations=&lowerMap->authoredSupportRelations;
		context.wallSupportRelations=&lowerMap->authoredWallSupportRelations;
		context.doorwaySupportRelations=&lowerMap->doorwaySupportRelations;
		S9xRemasterDungeonPlacements(context,profile,tiles);
		// Read-only failure-class ablation; never a published/runtime policy.
		if(std::getenv("ALTTP_AUDIT_NO_PERSISTENT_POTS"))for(auto &t:tiles)if(t.removableSupportPlanes){t.removableSupportPlanes=0;t.surface=RemasterDungeonFloorContext::Surface::Unknown;}
		if (!S9xRemasterBuildDungeonHeightMap(context, profile.upperFloorHeight, map)) throw std::runtime_error("unsupported room");
		std::cout << "{\"format\":\"alttp-runtime-room-heights-v1\",\"room_id\":" << context.roomIndex
			<< ",\"bg2_words_fnv1a64\":\"" << std::hex << digest(std::vector<uint8_t>(maps.begin(), maps.begin() + 8192)) << "\"" << std::dec
			<< ",\"link_xy\":[" << context.linkX << ',' << context.linkY << "],\"plane\":" << int(context.linkPlane)
			<< ",\"snapshot_fnv1a64\":\"" << std::hex << digest(raw) << "\",\"attributes_fnv1a64\":\"" << digest(attr)
			<< "\",\"profile_fnv1a64\":\"" << digest(read(argv[3])) << "\"" << std::dec
			<< ",\"template_input_present\":" << (context.dungeonTemplateData?"true":"false")
			<< ",\"structural_conflicts\":" << map.structuralConflicts << ",\"solved_wall_cells\":" << map.solvedWallCells
			<< ",\"unknown_wall_cells\":" << map.unknownWallCells
			<< ",\"authored_obstacle_cells\":" << map.authoredObstacleCells
			<< ",\"solved_authored_obstacle_cells\":" << map.solvedAuthoredObstacleCells
			<< ",\"unmatched_authored_obstacle_cells\":" << map.unmatchedAuthoredObstacleCells
			<< ",\"persistent_support_required\":["<<map.persistentSupportRequired[0]<<','<<map.persistentSupportRequired[1]<<']'
			<< ",\"persistent_support_missing\":["<<map.persistentSupportMissing[0]<<','<<map.persistentSupportMissing[1]<<']'
			<< ",\"configured_stair_fallbacks\":" << map.configuredStairFallbacks
			<< ",\"other_bg_authored_relations\":" << lowerMap->authoredSupportRelations.size()
			<< ",\"other_bg_wall_support_relations\":[";
		for(size_t n=0;n<lowerMap->authoredWallSupportRelations.size();n++) {
			if(n)std::cout<<',';const auto &r=lowerMap->authoredWallSupportRelations[n];
			std::cout<<"{\"wall\":"<<r.wall<<",\"support\":"<<r.support<<",\"rise\":"<<r.rise<<",\"word\":"<<r.word<<'}';
		}
		std::cout<<"],\"authored_obstacle_evidence\":[";
		for(size_t n=0;n<map.authoredObstacleEvidence.size();n++) {
			if(n)std::cout<<',';const auto &r=map.authoredObstacleEvidence[n];
			std::cout<<"{\"assembly\":"<<r.assembly<<",\"group\":"<<r.group<<",\"plane\":"<<r.plane
				<<",\"component\":"<<r.component<<",\"supporting_floor_base\":"<<r.floorBase<<",\"members\":"<<r.members
				<<",\"contacts\":"<<r.contacts<<",\"witnessed_contacts\":"<<r.witnessedContacts<<",\"conflict\":"<<(r.conflict?"true":"false")<<'}';
		}
		std::cout<<']'
			<< ",\"required_planes\":[" << map.requiredPlaneCells[0] << ',' << map.requiredPlaneCells[1]
			<< "],\"missing_planes\":[" << map.unknownPlaneCells[0] << ',' << map.unknownPlaneCells[1]
			<< "],\"stairs\":[";
		for (int i=0;i<context.stairCount;i++) {
			if(i) std::cout << ','; const auto &s=context.stairs[i];
			std::cout << '[' << int(s.x) << ',' << int(s.y) << ',' << s.highIsNorth << ',' << s.changesPlane << ']';
		}
		std::cout << "],\"candidate_floor_cells\":[" << map.candidateFloorCells[0] << ',' << map.candidateFloorCells[1]
			<< "],\"unobserved_floor_cells\":[" << map.unobservedFloorCells[0] << ',' << map.unobservedFloorCells[1]
			<< "],\"disconnected_plane_candidates\":" << (map.disconnectedPlaneCandidates?"true":"false")
			<< ",\"plane_relationship_unresolved\":" << (map.planeRelationshipUnresolved?"true":"false")
			<< ",\"cells_plane_x_y_base\":[";
		bool first = true;
		for (int p = 0; p < 2; p++) for (int i = 0; i < 4096; i++) if (map.planeOffsets[p][i] != 255)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << p << ',' << i % 64 << ',' << i / 64 << ',' << int(map.planeOffsets[p][i]) << ']';
		}
		std::cout << "],\"bg1_cells_plane_x_y_base\":[";first=true;
		for(int p=0;p<2;p++)for(int i=0;i<4096;i++)if(lowerMap->planeOffsets[p][i]!=255) {
			if(!first)std::cout<<',';first=false;
			std::cout<<'['<<p<<','<<i%64<<','<<i/64<<','<<lowerMap->planeOffsets[p][i]<<']';
		}
		std::cout << "],\"structural_placements\":["; first = true;
		for (int i = 0; i < 4096; i++) if (map.offsets[i] != 255 &&
			(tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall || tiles[i].surface == RemasterDungeonFloorContext::Surface::Obstacle))
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"x\":" << i % 64 << ",\"y\":" << i / 64 << ",\"floor_base\":" << int(map.offsets[i])
				<< ",\"kind\":\"" << (tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall ? "authored_wall" : "authored_obstacle") << "\",\"planes\":[";
			bool firstPlane = true;
			for (int p = 0; p < 2; p++) if ((tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall ?
				map.structuralPlaneOffsets[p][i] : tiles[i].obstacleAssembly ? map.structuralPlaneOffsets[p][i] : map.planeOffsets[p][i]) == map.offsets[i])
			{ if (!firstPlane) std::cout << ','; firstPlane = false; std::cout << p; }
			const auto floor=S9xRemasterMixedFloorBase(map,i);
			std::cout << "]";
			if(tiles[i].authoredObstacle) {
				std::cout<<",\"authored_obstacle_group\":"<<tiles[i].authoredObstacleGroup
					<<",\"authored_obstacle_assembly\":"<<tiles[i].authoredObstacleAssembly
					<<",\"member_z_tiles\":"<<tiles[i].authoredObstacleZ
					<<",\"footprint_flip\":"<<int(tiles[i].authoredObstacleFlip)
					<<",\"support_contact\":"<<(tiles[i].authoredObstacleContact?"true":"false");
				for(const auto &r:map.authoredObstacleEvidence)if(r.assembly==tiles[i].authoredObstacleAssembly)
					std::cout<<",\"supporting_floor_base\":"<<r.floorBase<<",\"support_plane\":"<<r.plane<<",\"support_component\":"<<r.component;
			}
			if(tiles[i].wallTemplateOffset)std::cout << ",\"wall_template_offset\":" << tiles[i].wallTemplateOffset;
			if(tiles[i].wallTemplateOffset>=0xa14 && tiles[i].wallTemplateOffset<=0xa44 &&
				(tiles[i].wallTemplateOffset-0xa14)%0x10==0)for(const auto &v:map.localHeightPreviews)if(v.cell==i) {
				std::cout<<",\"local_source\":\"placement_wall_mount_backing\",\"local_height\":[";
				for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(v.height[k]);}std::cout<<']';break;
			}
			if(tiles[i].diagonalHeightPreview && !tiles[i].diagonalAuthoredPreserved) {
				std::cout<<",\"local_source\":\"placement_rom_diagonal_preview\",\"local_height\":[";
				for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(tiles[i].height[k]);}std::cout<<']';
			}
			if(tiles[i].lowerDoorWallPart)std::cout << ",\"door_wall_part\":true";
			if(tiles[i].terminalCorner)std::cout << ",\"terminal_corner\":true";
			if(tiles[i].terminalCorner) {
				std::cout<<",\"corner_branch_relations\":[";bool firstBranch=true;
				for(const auto &b:map.cornerBranches)if(b.corner==i && map.structuralPlaneOffsets[b.plane][i]==map.offsets[i]) {
					if(!firstBranch)std::cout<<',';firstBranch=false;
					std::cout<<"{\"x\":"<<b.branch%64<<",\"y\":"<<b.branch/64<<",\"plane\":"<<b.plane<<",\"floor_base\":"<<b.base<<",\"support\":"<<b.support<<",\"rise\":"<<b.rise<<'}';
				}std::cout<<']';
			}
			if (floor != 255)
			{
				const auto local=S9xRemasterMixedFloorHeight(tiles[i].height,map.offsets[i],floor);
				std::cout << ",\"attached_floor_base\":" << int(floor) << ",\"local_source\":\"placement_grounded_floor_clamp\",\"local_height\":[";
				for (int k=0;k<64;k++) { if (k) std::cout << ','; std::cout << int(local[k]); }
				std::cout << ']';
			}
			std::cout << '}';
		}
		std::cout << "],\"mixed_floor_unknown\":" << map.mixedFloorUnknown << ",\"mixed_floor_conflicts\":" << map.mixedFloorConflicts
			<< ",\"mixed_floor_placements\":["; first=true;
		for (int p=0;p<2;p++) for (int i=0;i<4096;i++) if (tiles[i].groundedFloorMask && map.structuralPlaneOffsets[p][i] != 255)
		{
			if (!first) std::cout << ','; first=false;
			std::cout << "{\"plane\":" << p << ",\"x\":" << i%64 << ",\"y\":" << i/64 << ",\"attached_floor_base\":";
			if (map.mixedFloorBases[p][i] == 255) std::cout << "null"; else std::cout << int(map.mixedFloorBases[p][i]);
			std::cout << '}';
		}
		std::cout << "],\"persistent_support\":[";first=true;
		for(const auto &r:map.persistentSupport){if(!first)std::cout<<',';first=false;std::cout<<"{\"plane\":"<<r.plane<<",\"xy\":["<<r.cell%64<<','<<r.cell/64<<"],\"component\":"<<r.component<<",\"base\":"<<r.base<<'}';}
		std::cout << "],\"relative_anchor_provenance\":["; first = true;
		for (const auto &anchor : map.relativeAnchors)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"node\":" << anchor.node << ",\"relative_minimum\":" << anchor.minimum
				<< ",\"support_nodes\":" << anchor.supportNodes << ",\"convention\":\""
				<< (anchor.isolatedFlat ? "isolated_authored_flat_relative_zero" : "lowest_established_support_relative_zero") << "\"}";
		}
		std::cout << "],\"geometry_component_issues\":["; first = true;
		for (const auto &issue : map.geometryGroupIssues)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"start_node\":" << issue.start << ",\"nodes\":" << issue.nodes << ",\"equality_mismatches\":" << issue.mismatches
				<< ",\"negative_base_nodes\":" << issue.negative << ",\"overflow_nodes\":" << issue.overflow
				<< ",\"isolated_flat\":" << (issue.isolatedFlat ? "true" : "false") << '}';
		}
		std::cout << "],\"constraint_issues\":["; first = true;
		for (const auto &issue : map.geometryConflicts)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << issue.node << ',' << issue.other << ',' << issue.expected << ',' << issue.actual << ']';
		}
		std::cout << "],\"issue_placements\":["; first = true;
		for (const auto &issue : map.geometryIssuePlacements)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << '[' << issue[0] << ',' << issue[1] << ',' << issue[2] << ',' << issue[3] << ']';
		}
		std::cout << "],\"unresolved_wall_placements\":["; first = true;
		for (int i = 0; i < 4096; i++) if (tiles[i].surface == RemasterDungeonFloorContext::Surface::Wall && map.offsets[i] == 255)
		{
			if (!first) std::cout << ','; first = false;
			std::cout << "{\"xy\":[" << i % 64 << ',' << i / 64 << "],\"local_min\":" << int(*std::min_element(tiles[i].height.begin(), tiles[i].height.end()))
				<< ",\"local_max\":" << int(*std::max_element(tiles[i].height.begin(), tiles[i].height.end()))
				<< ",\"tile_word\":" << tileWords[i] << ",\"neighbors\":[";
			bool firstNeighbor = true;
			for (const auto &d : {std::array<int, 2>{0,-1}, {0,1}, {-1,0}, {1,0}})
			{
				int x = i % 64 + d[0], y = i / 64 + d[1];
				if (x < 0 || x >= 64 || y < 0 || y >= 64) continue;
				if (!firstNeighbor) std::cout << ','; firstNeighbor = false;
				int j = y * 64 + x;
				std::cout << "{\"xy\":[" << x << ',' << y << "],\"role\":" << int(tiles[j].surface)
					<< ",\"has_height\":" << (tiles[j].hasHeight ? "true" : "false")
					<< ",\"base\":" << int(map.offsets[j]) << ",\"attributes\":[" << int(context.collisionAttributes[j])
					<< ',' << int(context.collisionAttributes[4096 + j]) << "]}";
			}
			std::cout << "]}";
		}
		std::cout << "],\"directional_geometry\":[";first=true;
		// Complete placed inventory on BOTH BG layers, each gameplay plane.
		// The validator independently samples authored/override pixels and seams.
		for(int layer=1;layer>=0;layer--)
		{
			if(layer==0){context.placementBackgroundIndex=0;std::rotate(maps.begin(),maps.begin()+8192,maps.end());std::swap(context.graphicsBase,context.bg1GraphicsBase);
				S9xRemasterDungeonPlacements(context,profile,tiles);context.placements=tiles.data();
				map=*lowerMap;}
			for(int p=0;p<2;p++)for(int i=0;i<4096;i++){
				if(!first)std::cout<<',';first=false;
				const auto &t=tiles[i];int base=map.structuralPlaneOffsets[p][i];
				if(base==255)base=map.planeOffsets[p][i];
				if(base==255 && map.fallbackDirections[i] &&
					(context.collisionAttributes[p*4096+i]==1 || context.collisionAttributes[p*4096+i]==2))
					base=map.offsets[i];
				if(t.authoredObstacle) {
					base=255;for(const auto &r:map.authoredObstacleEvidence)if(r.assembly==t.authoredObstacleAssembly && r.plane==p)base=map.offsets[i];
				}
				const int floor=map.mixedFloorBases[p][i];
				std::cout<<"{\"layer\":"<<layer<<",\"plane\":"<<p<<",\"x\":"<<i%64<<",\"y\":"<<i/64
					<<",\"role\":"<<int(t.surface)<<",\"base\":"<<base<<",\"fallback_directions\":"<<int(map.fallbackDirections[i])
					<<",\"has_local\":"<<(t.hasHeight?"true":"false")<<",\"floor_datum\":"<<floor;
				if(map.producerOwnedStair[i])std::cout<<",\"owned_stair\":true";
				if(t.authoredObstacle || map.producerOwnedStair[i]) {
					std::cout<<",\"authored_obstacle\":true,\"authored_obstacle_group\":"<<t.authoredObstacleGroup
						<<",\"authored_obstacle_assembly\":"<<t.authoredObstacleAssembly<<",\"member_z_tiles\":"<<t.authoredObstacleZ
						<<",\"support_contact\":"<<(t.authoredObstacleContact?"true":"false")<<",\"footprint_flip\":"<<int(t.authoredObstacleFlip);
					// Exercise the actual terminal frame consumer on each complete-room
					// placement, even offscreen. Record canonical effective pixel arrays.
					RemasterFrame f;f.width=f.height=8;f.mainPixels.resize(64);f.subPixels.resize(64);
					RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=layer;
					instance.tileId={t.tileHash,1,4};instance.hFlip=t.hflip;instance.vFlip=t.vflip;f.tileInstances={instance};
					RemasterFrameAssetMetadata metadata;metadata.tileId=instance.tileId;metadata.hasHeight=t.hasHeight;metadata.height=t.height;
					f.assetMetadata={metadata};for(int k=0;k<64;k++) {f.mainPixels[k].instanceId=1;f.mainPixels[k].tilePixel=k;}
					auto apply=context;apply.backgroundScrollX=i%64*8;apply.backgroundScrollY=i/64*8;
					S9xRemasterApplyDungeonHeightMap(f,apply,map,nullptr,nullptr,layer);
					std::cout<<",\"runtime_effective_height\":[";
					for(int k=0;k<64;k++){if(k)std::cout<<',';const auto &a=f.tileInstances[f.mainPixels[k].instanceId-1];
						std::cout<<a.heightOffset+(a.hasPlacementHeight?a.placementHeight[k]:t.height[k]);}
					std::cout<<"],\"runtime_authored_pixels_preserved\":"<<((f.assetMetadata[0].height==t.height && (!t.hasHeight || !f.tileInstances[0].hasPlacementHeight))?"true":"false")
						<<",\"obstacle_local_source\":\""<<(t.hasHeight?"authored_asset":map.offsets[i]!=255?"placement_obstacle_vertical_fallback":"unresolved_support")<<'"';
				}
				if(t.diagonalHeightPreview && !t.diagonalAuthoredPreserved) {
					std::cout<<",\"captured_local_source\":\"placement_rom_diagonal_preview\",\"preview_row\":"<<int(t.diagonalPreviewRow)
						<<",\"preview_foot\":"<<int(t.diagonalPreviewFoot)
						<<",\"preview_intercept\":"<<t.diagonalIntercept<<",\"preview_minimum\":"<<int(t.diagonalMinimum)
						<<",\"preview_maximum\":"<<int(t.diagonalMaximum)<<",\"captured_local_height\":[";
					for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(t.height[k]);}std::cout<<']';
				}
				if(t.wallTemplateOffset>=0xa14 && t.wallTemplateOffset<=0xa44 && (t.wallTemplateOffset-0xa14)%0x10==0)
					for(const auto &v:map.localHeightPreviews)if(v.cell==i && map.structuralPlaneOffsets[p][i]!=255) {
						std::cout<<",\"captured_local_source\":\"placement_wall_mount_backing\",\"captured_local_height\":[";
						for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(v.height[k]);}std::cout<<']';break;
					}
				std::cout<<'}';
			}
		}
		std::cout << "],\"stair_attachments\":[";first=true;
		for(int layer=0;layer<2;layer++) {
			if(layer==1){std::rotate(maps.begin(),maps.begin()+8192,maps.end());std::swap(context.graphicsBase,context.bg1GraphicsBase);
				context.placementBackgroundIndex=1;S9xRemasterDungeonPlacements(context,profile,tiles);S9xRemasterBuildDungeonHeightMap(context,profile.upperFloorHeight,map);}
			for(const auto &a:map.stairAttachments) {
				if(!first)std::cout<<',';first=false;
				std::cout<<"{\"layer\":"<<layer<<",\"origin\":"<<a.origin<<",\"low\":"<<a.low<<",\"high\":"<<a.high
					<<",\"low_plane\":"<<a.lowPlane<<",\"high_plane\":"<<a.highPlane<<",\"rise\":"<<a.rise<<",\"base\":"<<a.base
					<<",\"flip\":"<<int(a.flip)<<",\"conflict\":"<<(a.conflict?"true":"false")<<'}';
			}
		}
		// Restore the BG1 starting inventory expected by the following pass.
		std::rotate(maps.begin(),maps.begin()+8192,maps.end());std::swap(context.graphicsBase,context.bg1GraphicsBase);
		context.placementBackgroundIndex=0;S9xRemasterDungeonPlacements(context,profile,tiles);map=*lowerMap;
		std::cout << "],\"doorframe_attachments\":[";first=true;
		// Complete BG1 map is selected above; also reconstruct BG2's applied
		// result for the region inventory without changing authored metadata.
		for(int layer=0;layer<2;layer++) {
			if(layer==1) {
				std::rotate(maps.begin(),maps.begin()+8192,maps.end());std::swap(context.graphicsBase,context.bg1GraphicsBase);
				context.placementBackgroundIndex=1;S9xRemasterDungeonPlacements(context,profile,tiles);
				S9xRemasterBuildDungeonHeightMap(context,profile.upperFloorHeight,map);
			}
			for(const auto &a:map.doorframeAttachments) {
				if(!first)std::cout<<',';first=false;const auto &t=tiles[a.cell];
				// Exercise the actual frame application for every attached region,
				// including offscreen art; array arithmetic alone is not runtime proof.
				RemasterFrame frame;frame.width=frame.height=8;frame.mainPixels.resize(64);frame.subPixels.resize(64);
				RemasterFrameTileInstance instance;instance.source=RemasterSourceType::Background;instance.sourceIndex=layer;
				instance.tileId={t.tileHash,1,4};instance.hFlip=t.hflip;instance.vFlip=t.vflip;frame.tileInstances.push_back(instance);
				RemasterFrameAssetMetadata metadata;metadata.tileId=instance.tileId;metadata.tileRole=RemasterTileRole::Doorway;
				metadata.hasHeight=metadata.hasDoorframeMask=true;metadata.height=t.height;metadata.doorframeMask=t.doorframeMask;
				frame.assetMetadata.push_back(metadata);
				for(int y=0;y<8;y++)for(int x=0;x<8;x++) {
					auto &pixel=frame.mainPixels[y*8+x];pixel.instanceId=1;pixel.tilePixel=(t.vflip?7-y:y)*8+(t.hflip?7-x:x);
				}
				auto applyContext=context;applyContext.backgroundScrollX=a.cell%64*8;applyContext.backgroundScrollY=a.cell/64*8;
				S9xRemasterApplyDungeonHeightMap(frame,applyContext,map,nullptr,nullptr,layer);
				for(const auto &pixel:frame.mainPixels) {
					const auto &applied=frame.tileInstances[pixel.instanceId-1];
					const int effective=applied.heightOffset+(applied.hasPlacementHeight?applied.placementHeight[pixel.tilePixel]:t.height[pixel.tilePixel]);
					int base=255;for(const auto &region:map.doorframeAttachments)if(region.cell==a.cell && region.visible) {
						const int value=(region.mask&(UINT64_C(1)<<pixel.tilePixel))?region.base:region.openingBase;
						if(value==255)continue;
						if(base!=255 && base!=value){base=255;break;}base=value;
					}
					const int expected=t.height[pixel.tilePixel]+(base!=255?base:0);
					if(effective!=expected || frame.assetMetadata[0].height!=t.height)throw std::runtime_error("doorframe runtime application disagrees with complete-room reconstruction");
				}
				std::cout<<"{\"layer\":"<<layer<<",\"plane\":"<<a.plane<<",\"x\":"<<a.cell%64<<",\"y\":"<<a.cell/64
					<<",\"frame_base\":"<<a.base<<",\"opening_base\":"<<a.openingBase<<",\"visible\":"<<(a.visible?"true":"false")<<",\"split_dispatch\":"<<(t.splitDoorPart?"true":"false")<<",\"template_offset\":"<<a.source<<",\"mask\":\""<<std::hex<<a.mask<<std::dec
					<<"\",\"local_source\":\"placement_rom_directional_doorway_support\",\"ownership_scope\":\""<<(t.doorwayColumnContact?"surviving_inner_column":"complete_frame")<<"\",\"effective_height\":[";
				for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(t.height[k])+((a.mask&(UINT64_C(1)<<k))?a.base:a.openingBase==255?0:a.openingBase);}std::cout<<"]}";
			}
		}
		std::cout << "],\"doorframe_ownership\":[";first=true;
		for(int i=0;i<4096;i++)if(tiles[i].surface==RemasterDungeonFloorContext::Surface::Door && (tiles[i].doorframeMask || tiles[i].doorwaySupport)) {
			if(!first)std::cout<<',';first=false;const auto &t=tiles[i];
			std::cout<<"{\"xy\":["<<i%64<<','<<i/64<<"],\"template\":"<<t.doorTemplateOffset<<",\"assembly\":"<<t.doorAssembly<<",\"direction\":"<<int(t.doorDirection)
				<<",\"doorway_support\":"<<(t.doorwaySupport?"true":"false")<<",\"doorway_support_direction\":\""<<S9xRemasterSupportDirectionName(t.doorwaySupportDirection)<<"\",\"neighbors\":[";
			bool nf=true;for(int j:{i-64,i+64,i-1,i+1})if(j>=0 && j<4096) {
				if(!nf)std::cout<<',';nf=false;const auto &n=tiles[j];
				std::cout<<"{\"xy\":["<<j%64<<','<<j/64<<"],\"role\":"<<int(n.surface)<<",\"template\":"<<n.wallTemplateOffset<<",\"base\":"<<map.structuralPlaneOffsets[0][j]<<",\"local\":[";
				for(int k=0;k<64;k++){if(k)std::cout<<',';std::cout<<int(n.height[k]);}std::cout<<"]}";
			}std::cout<<"]}";
		}
		std::cout << "],\"unknown_doorframe_cells\":"<<map.unknownDoorframeCells<<",\"conflicting_doorframe_cells\":"<<map.conflictingDoorframeCells<<"}\n";
	} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
