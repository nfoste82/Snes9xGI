// Standalone CLI regression. Build this and alttp_overworld_replay.cpp with
// c++ -std=c++17 -Wall -Wextra -pedantic, then pass the producer binary as argv[1].
// All artwork, captures and candidates are synthetic; no ROM is required.
#include "../../remaster/frame.h"
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
namespace TestDigest {
#include "../../sha256.cpp"
}

namespace {
namespace fs = std::filesystem;

void need(bool value, const std::string &message)
{
	if (!value) throw std::runtime_error(message);
}

struct TemporaryDirectory {
	fs::path path;
	TemporaryDirectory()
	{
		std::string pattern = (fs::temp_directory_path()/"overworld-replay-test-XXXXXX").string();
		char *created = mkdtemp(pattern.data());
		need(created != nullptr, "create temporary fixture directory");
		path = created;
	}
	~TemporaryDirectory() { std::error_code error; fs::remove_all(path, error); }
};

uint64_t artworkHash(const uint8_t *indices)
{
	uint64_t hash = UINT64_C(14695981039346656037);
	for (uint8_t byte : {1, 1, 4, 8, 8}) { hash ^= byte; hash *= UINT64_C(1099511628211); }
	for (int k = 0; k < 64; ++k) { hash ^= indices[k]; hash *= UINT64_C(1099511628211); }
	return hash;
}

RemasterFrame fixture()
{
	RemasterFrame frame;
	frame.width = 64; frame.height = 8;
	frame.profileRomSha256 = std::string(64, 'a');
	frame.originalRgb555.resize(512); frame.sceneRgb555.resize(512);
	frame.mainPixels.resize(512); frame.subPixels.resize(512); frame.scenePixels.resize(512);
	std::set<uint64_t> hashes;
	for (unsigned tile = 0; tile < 9; ++tile) {
		RemasterFrameAsset asset;
		for (unsigned k = 0; k < 64; ++k) asset.indices[k] = 1 + (k + tile) % 15;
		asset.tileId = {artworkHash(asset.indices), 1, 4};
		need(hashes.insert(asset.tileId.hash).second, "fixture artwork hashes must be distinct");
		frame.assets.push_back(asset);
		RemasterFrameAssetMetadata metadata;
		metadata.tileId = asset.tileId; metadata.tileRole = RemasterTileRole::Wall;
		metadata.hasHeight = metadata.hasNormals = metadata.hasOcclusion = true;
		metadata.height.fill(777); metadata.normalXyz.fill(255); metadata.occlusion.fill(200);
		frame.assetMetadata.push_back(metadata);
		RemasterFrameTileInstance instance;
		instance.tileId = asset.tileId;
		instance.source = tile == 8 ? RemasterSourceType::Object : RemasterSourceType::Background;
		instance.sourceIndex = tile == 8 ? 0 : 1;
		instance.tileNumber = 100 + tile; instance.palette = tile % 8;
		instance.hFlip = tile & 1; instance.vFlip = tile & 2;
		instance.normalYaw = 1; // Candidate normals are already in world/display space.
		instance.heightOffset = tile == 8 ? -123 : 999;
		instance.hasPlacementHeight = true; instance.placementHeight.fill(-19);
		instance.hasPaletteArtwork = true;
		for (unsigned k = 0; k < 64; ++k) instance.paletteRgb555[k] = (tile * 100 + k) & 32767;
		frame.tileInstances.push_back(instance);
	}
	for (unsigned n = 0; n < 512; ++n) {
		const unsigned tile = (n % 64) / 8, x = n % 8, y = n / 64;
		const auto &instance = frame.tileInstances[tile];
		RemasterFramePixel pixel;
		pixel.owner = 0x12340000 + n; pixel.instanceId = tile + 1;
		pixel.tilePixel = (instance.vFlip ? 7-y : y)*8 + (instance.hFlip ? 7-x : x);
		frame.mainPixels[n] = frame.subPixels[n] = frame.scenePixels[n] = pixel;
		frame.originalRgb555[n] = (n * 37) & 32767;
		frame.sceneRgb555[n] = (n * 71 + 13) & 32767;
	}
	// One actor pixel per plane leaves all eight BG occurrences visible.
	for (auto *plane : {&frame.mainPixels, &frame.subPixels, &frame.scenePixels}) {
		plane->back().instanceId = 9; plane->back().tilePixel = 17;
	}
	return frame;
}

int localHeight(unsigned tile, unsigned display) { return int(tile)*9 + int(display)*3 - 130; }
uint8_t normal(unsigned tile, unsigned display, unsigned axis)
{
	return uint8_t((tile*17 + display*3 + axis*73) % 256);
}

// Intentionally independent of the producer's Cell/signature implementation.
std::string candidate(const RemasterFrame &frame, unsigned count = 8,
	bool duplicate = false, bool incomplete = false)
{
	std::ostringstream text;
	text << "OWREPLAY1 " << frame.profileRomSha256 << ' ' << count*(duplicate ? 2 : 1) << '\n';
	for (unsigned region = 0; region < (duplicate ? 2u : 1u); ++region)
		for (unsigned tile = 0; tile < count; ++tile) {
			const auto &instance = frame.tileInstances[tile];
			const unsigned word = instance.tileNumber | (instance.palette << 10) |
				(instance.hFlip ? 0x4000 : 0) | (instance.vFlip ? 0x8000 : 0);
			const int status = incomplete && tile == 6 ? 2 : incomplete && tile == 7 ? 0 : 1;
			text << 11 + tile + region*40 << " 7 " << word << ' ' << std::hex << instance.tileId.hash
				<< std::dec << ' ' << status << ' ' << int(RemasterTileRole::Floor) << ' '
				<< -40 + int(tile)*5 + int(region)*1000;
			if (status == 1) {
				for (unsigned k = 0; k < 64; ++k) text << ' ' << localHeight(tile, k);
				for (unsigned k = 0; k < 64; ++k)
					for (unsigned axis = 0; axis < 3; ++axis) text << ' ' << unsigned(normal(tile, k, axis));
			}
			text << '\n';
		}
	return text.str();
}

std::string readText(const fs::path &path)
{
	std::ifstream input(path);
	need(bool(input), "read CLI diagnostics");
	return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct Result { int status; std::string out, error; fs::path output; };

Result run(const fs::path &producer, const fs::path &directory, const std::string &name,
	const RemasterFrame &frame, const std::string &text, std::string standing = "")
{
	const std::string input = (directory / (name + ".s9xrmf")).string();
	const std::string map = (directory / (name + ".txt")).string();
	const std::string output = (directory / (name + "-output.s9xrmf")).string();
	const fs::path out = directory / (name + ".stdout"), error = directory / (name + ".stderr");
	need(S9xWriteRemasterFrame(frame, input), name + ": write valid synthetic capture");
	const std::string att = (directory/(name+"-standing.txt")).string();
	if (!standing.empty()) {
		std::ifstream file(input, std::ios::binary);
		std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)), {});
		unsigned char digest[32]; TestDigest::sha256sum(bytes.data(), unsigned(bytes.size()), digest);
		std::string sha; const char *hex="0123456789abcdef";
		for(auto b:digest){sha+=hex[b>>4];sha+=hex[b&15];}
		if(standing.find("SHA")!=std::string::npos)standing.replace(standing.find("SHA"),3,sha);
		std::ofstream manifest(att); manifest<<standing;
	}
	{ std::ofstream file(map); file << text; need(bool(file), "write candidate"); }
	pid_t child = fork();
	need(child >= 0, "fork producer");
	if (child == 0) {
		int stdoutFile = open(out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		int stderrFile = open(error.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
		if (stdoutFile < 0 || stderrFile < 0 || dup2(stdoutFile, STDOUT_FILENO) < 0 ||
			dup2(stderrFile, STDERR_FILENO) < 0) _exit(126);
		close(stdoutFile); close(stderrFile);
		if(standing.empty())execl(producer.c_str(), producer.c_str(), input.c_str(), map.c_str(), output.c_str(), nullptr);
		else execl(producer.c_str(), producer.c_str(), input.c_str(), map.c_str(), output.c_str(), att.c_str(), nullptr);
		_exit(127);
	}
	int status = 0;
	need(waitpid(child, &status, 0) == child && WIFEXITED(status), name + ": producer crashed");
	return {WEXITSTATUS(status), readText(out), readText(error), output};
}

RemasterFrame accepted(const Result &result)
{
	need(result.status == 0, "expected acceptance: " + result.error);
	RemasterFrame frame;
	need(S9xReadRemasterFrame(result.output.string(), frame), "read emitted capture");
	std::vector<uint8_t> bytes, again;
	RemasterFrame replay;
	need(S9xSerializeRemasterFrame(frame, bytes) && S9xDeserializeRemasterFrame(bytes, replay) &&
		S9xSerializeRemasterFrame(replay, again) && bytes == again, "canonical packet roundtrip");
	return replay;
}

void rejected(const Result &result, const std::string &diagnostic)
{
	need(result.status == 1, "expected rejection, got " + std::to_string(result.status));
	need(result.error.find(diagnostic) != std::string::npos, "wrong rejection path: " + result.error);
	need(!fs::exists(result.output), "rejection must not publish a packet");
}

void verify(const RemasterFrame &input, const RemasterFrame &output, bool incomplete)
{
	need(input.originalRgb555 == output.originalRgb555 && input.sceneRgb555 == output.sceneRgb555,
		"stock and scene RGB must remain byte-for-byte unchanged");
	for (const auto *plane : {&output.mainPixels, &output.subPixels, &output.scenePixels}) {
		need(plane->back().instanceId == 9 && plane->back().tilePixel == 17, "actor pixel unchanged");
		for (unsigned n = 0; n + 1 < plane->size(); ++n) {
			const auto &pixel = (*plane)[n];
			const unsigned tile = (n % 64)/8, display = (n/64)*8 + n%8;
			const auto &instance = output.tileInstances.at(pixel.instanceId-1);
			need(instance.tileId == input.tileInstances[tile].tileId, "BG artwork identity preserved");
			const auto geometry = S9xRemasterResolveInstanceGeometry(output.assetMetadata[tile], instance);
			if (incomplete && tile >= 6) {
				need(instance.geometryStatus == (tile == 6 ? RemasterOccurrenceGeometryStatus::Conflict :
					RemasterOccurrenceGeometryStatus::Unresolved), "explicit conflict/unresolved survives roundtrip");
				need(!instance.hasPlacementHeight && !instance.hasPlacementNormals && !instance.hasPlacementOcclusion &&
					instance.heightOffset == 0 && instance.placementTileRole == RemasterTileRole::Unknown,
					"incomplete cell must clear stale placement geometry");
				need(!geometry.hasHeight && !geometry.hasNormals && !geometry.hasOcclusion,
					"conflict/unresolved must suppress hash-global geometry fallback");
				continue;
			}
			need(instance.geometryStatus == RemasterOccurrenceGeometryStatus::Resolved &&
				instance.placementTileRole == RemasterTileRole::Floor && instance.hasPlacementHeight &&
				instance.hasPlacementNormals && instance.placementNormalIgnoreTileTransform,
				"resolved occurrence geometry flags and role");
			need(instance.heightOffset == -40 + int(tile)*5, "signed placement datum preserved");
			need(instance.placementHeight[pixel.tilePixel] == localHeight(tile, display) &&
				geometry.height[pixel.tilePixel] == localHeight(tile, display), "canonical flip height mapping");
			float vector[3];
			for (unsigned axis = 0; axis < 3; ++axis) {
				need(instance.placementNormalXyz[pixel.tilePixel*3+axis] == normal(tile, display, axis),
					"canonical flip normal mapping");
				vector[axis] = geometry.normalXyz[pixel.tilePixel*3+axis]/127.5f - 1.0f;
			}
			const std::array<float, 3> before = {vector[0], vector[1], vector[2]};
			S9xRemasterTransformNormalForTileInstance(instance, vector[0], vector[1], vector[2], geometry.normalIgnoreTileTransform);
			need(before == std::array<float, 3>{vector[0], vector[1], vector[2]}, "world normals must not flip/yaw twice");
		}
	}
	// Undo only the intended BG ownership/table additions. Whole-packet equality
	// then covers the actor's complete record, all original instances, owners,
	// palette artwork, asset metadata and every other retained capture field.
	auto restored = output;
	restored.tileInstances.resize(input.tileInstances.size());
	for (auto pair : {std::make_pair(&restored.mainPixels, &input.mainPixels),
		std::make_pair(&restored.subPixels, &input.subPixels), std::make_pair(&restored.scenePixels, &input.scenePixels)}) {
		for (unsigned n = 0; n < pair.first->size(); ++n) {
			need((*pair.first)[n].owner == (*pair.second)[n].owner &&
				(*pair.first)[n].tilePixel == (*pair.second)[n].tilePixel, "pixel owners and canonical coordinates unchanged");
			(*pair.first)[n].instanceId = (*pair.second)[n].instanceId;
		}
	}
	std::vector<uint8_t> before, after;
	need(S9xSerializeRemasterFrame(input, before) && S9xSerializeRemasterFrame(restored, after) && before == after,
		"actor and original capture data unchanged");
}
} // namespace

int main(int argc, char **argv)
{
	try {
		need(argc == 2, "usage: alttp_overworld_replay_test PRODUCER_BINARY");
		const fs::path producer = fs::absolute(argv[1]);
		TemporaryDirectory temporary;
		const auto frame = fixture();
		auto unique = run(producer, temporary.path, "unique", frame, candidate(frame));
		verify(frame, accepted(unique), false);
		need(unique.out.find("\"origin\":[88,56]") != std::string::npos &&
			unique.out.find("\"unique_matching_occurrences\":8") != std::string::npos &&
			unique.out.find("\"mismatched_pixels\":0") != std::string::npos &&
			unique.out.find("\"actor_support_applied\":false") != std::string::npos, "unique-origin evidence report");
		rejected(run(producer, temporary.path, "ambiguous", frame, candidate(frame, 8, true)),
			"ambiguous/insufficient world alignment");
		rejected(run(producer, temporary.path, "seven-occurrences", frame, candidate(frame, 7)),
			"ambiguous/insufficient world alignment");
		verify(frame, accepted(run(producer, temporary.path, "incomplete", frame, candidate(frame, 8, false, true))), true);
		// A sole crosses two floor cells. Same numeric height does NOT prove
		// ownership; varying local relief must not be ignored either.
		auto standingFrame=frame;standingFrame.tileInstances[8].heightOffset=0;
		auto standingCandidate=[&](bool split, bool relief) {
			std::ostringstream s;s<<"OWREPLAY2 "<<frame.profileRomSha256<<" 8\n";
			for(int t=0;t<8;t++) {
				const auto&i=frame.tileInstances[t];int w=i.tileNumber|(i.palette<<10)|(i.hFlip?0x4000:0)|(i.vFlip?0x8000:0);
				s<<11+t<<" 7 "<<w<<' '<<std::hex<<i.tileId.hash<<std::dec<<" 1 1 32 "<<(split&&t==1?12:11)<<" 0 0";
				for(int k=0;k<64;k++)s<<' '<<(relief&&t==1?1:0);
				for(int k=0;k<192;k++)s<<" 127";s<<'\n';
			}return s.str();
		};
		std::string att="OWSTAND1 SHA 1 88 56 88 40 0 -16 11 0 32 0 1 0";
		for(int x=5;x<=11;x++)att+=" "+std::to_string(x)+" 7 "+std::to_string(88+x)+" 63";
		auto stood=accepted(run(producer,temporary.path,"standing",standingFrame,standingCandidate(false,false),att));
		need(stood.tileInstances[8].heightOffset==32 && stood.tileInstances[8].placementHeight==standingFrame.tileInstances[8].placementHeight,
			"replace actor base exactly and preserve sprite relief");
		auto expectedStanding=accepted(run(producer,temporary.path,"standing-no-attestation",standingFrame,standingCandidate(false,false)));
		expectedStanding.tileInstances[8].heightOffset=32;
		std::vector<uint8_t> stoodBytes,expectedBytes;
		need(S9xSerializeRemasterFrame(stood,stoodBytes)&&S9xSerializeRemasterFrame(expectedStanding,expectedBytes)&&stoodBytes==expectedBytes,
			"whole packet differs from unpinned replay only by exact actor base");
		rejected(run(producer,temporary.path,"different-support",standingFrame,standingCandidate(true,false),att),"standing fixture rejected");
		rejected(run(producer,temporary.path,"local-relief",standingFrame,standingCandidate(false,true),att),"standing fixture rejected");
		rejected(run(producer,temporary.path,"nonzero-original",frame,standingCandidate(false,false),att),"standing fixture rejected");
		rejected(run(producer,temporary.path,"reapply-repinned",stood,standingCandidate(false,false),att),"standing fixture rejected");
		auto badSha=att;badSha.replace(badSha.find("SHA"),3,std::string(64,'0'));
		rejected(run(producer,temporary.path,"sha-mismatch",standingFrame,standingCandidate(false,false),badSha),"standing fixture rejected");
		// Preserve eight matching occurrences while altering 25/26 of 511 BG
		// pixels. This exercises the pixel mismatch gate, not the evidence gate.
		for (unsigned mismatches : {25u, 26u}) {
			auto altered = frame;
			auto wrong = frame.tileInstances[0]; wrong.palette = 7;
			altered.tileInstances.push_back(wrong);
			for (unsigned k = 0; k < mismatches; ++k)
				altered.mainPixels[(k/8)*64+k%8].instanceId = uint32_t(altered.tileInstances.size());
			auto result = run(producer, temporary.path, "mismatch-" + std::to_string(mismatches), altered, candidate(frame));
			if (mismatches == 25) {
				accepted(result);
				need(result.out.find("\"matched_pixels\":486,\"mismatched_pixels\":25") != std::string::npos,
					"under-five-percent mismatch accepted with exact denominator");
			} else rejected(result, "insufficient map match 485/511");
		}
		std::cout << "PASS overworld replay: unique/ambiguous origins, all flips, signed relief/normals, RGB/actor preservation, 5% mismatch gate; standing base replacement, different-support/local-relief/nonzero/reapply/SHA rejection\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "FAIL overworld replay: " << error.what() << '\n';
		return 1;
	}
}
