#include "capture_path.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <unistd.h>

int main()
{
	assert(S9xRemasterCaptureLocationPath("/captures/frame.s9xrmf", true, true, 0x61, 0, 0) ==
		"/captures/frame_room061.s9xrmf");
	assert(S9xRemasterCaptureLocationPath("frame.s9xrmf", true, false, 0, 0x1234, 0x5678) ==
		"frame_overworld_x1234_y5678.s9xrmf");
	assert(S9xRemasterCaptureLocationPath("frame.s9xrmf", false, false, 0, 0, 0) == "frame.s9xrmf");
	char directory[] = "/var/folders/hk/2wk9yqf564g4c39vrly7dgd80000gq/T/opencode/capture-path-XXXXXX";
	assert(mkdtemp(directory));
	std::string base = std::string(directory) + "/frame_room061.s9xrmf", written;
	std::vector<uint8_t> first = {1, 2, 3}, second = {4, 5};
	assert(S9xRemasterWriteUniqueCapture(first, base, written) && written == base);
	assert(S9xRemasterWriteUniqueCapture(second, base, written) && written == std::string(directory) + "/frame_room061_001.s9xrmf");
	assert(S9xRemasterWriteUniqueCapture(second, base, written) && written == std::string(directory) + "/frame_room061_002.s9xrmf");
	std::ifstream input(base, std::ios::binary);
	std::vector<uint8_t> preserved((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	assert(preserved == first);
	const auto orphan=std::string(directory)+"/orphan.s9xrmf";
	{std::ofstream evidence(orphan+".sprite-input.bin");evidence<<"do not replace";}
	assert(S9xRemasterWriteUniqueCapture(first,orphan,written));
	assert(written==std::string(directory)+"/orphan_001.s9xrmf");
	std::remove(written.c_str());std::remove((orphan+".sprite-input.bin").c_str());
	for (const char *name : {"frame_room061.s9xrmf", "frame_room061_001.s9xrmf", "frame_room061_002.s9xrmf"})
		std::remove((std::string(directory) + "/" + name).c_str());
	rmdir(directory);
	return 0;
}
