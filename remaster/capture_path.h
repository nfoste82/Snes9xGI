#ifndef _REMASTER_CAPTURE_PATH_H_
#define _REMASTER_CAPTURE_PATH_H_

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

inline std::string S9xRemasterCaptureLocationPath (const std::string &path,
	bool alttp, bool indoors, unsigned room, unsigned x, unsigned y)
{
	const size_t extension = path.rfind('.');
	const std::string stem = extension == std::string::npos ? path : path.substr(0, extension);
	const std::string suffix = extension == std::string::npos ? ".s9xrmf" : path.substr(extension);
	char location[96] = {};
	if (alttp && indoors) std::snprintf(location, sizeof(location), "_room%03x", room);
	else if (alttp) std::snprintf(location, sizeof(location), "_overworld_x%04x_y%04x", x, y);
	return stem + location + suffix;
}

// Exclusive creation makes both existing captures and simultaneous writers safe.
inline bool S9xRemasterWriteUniqueCapture (const std::vector<uint8_t> &bytes,
	const std::string &path, std::string &writtenPath)
{
	const size_t extension = path.rfind('.');
	const std::string stem = extension == std::string::npos ? path : path.substr(0, extension);
	const std::string suffix = extension == std::string::npos ? "" : path.substr(extension);
	for (unsigned index = 0; ; index++)
	{
		char number[32] = {};
		if (index) std::snprintf(number, sizeof(number), "_%03u", index);
		const std::string candidate = stem + number + suffix;
		bool orphanEvidence=false;
		for (const char *sidecar : {".profile.toml",".dungeon.json",".dungeon-input.bin",
			".dungeon-templates.bin",".terrain-input.bin",".sprite-input.bin"})
			if (FILE *existing=std::fopen((candidate+sidecar).c_str(),"rb")) {
				std::fclose(existing);orphanEvidence=true;break;
			}
		if (orphanEvidence) { if(index!=~0u)continue; return false; }
		FILE *output = std::fopen(candidate.c_str(), "wbx");
		if (!output)
		{
			if (errno == EEXIST && index != ~0u) continue;
			return false;
		}
		const bool complete = std::fwrite(bytes.data(), 1, bytes.size(), output) == bytes.size();
		const bool closed = std::fclose(output) == 0;
		if (!complete || !closed) { std::remove(candidate.c_str()); return false; }
		writtenPath = candidate;
		return true;
	}
}

#endif
