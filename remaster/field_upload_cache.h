#ifndef REMASTER_FIELD_UPLOAD_CACHE_H
#define REMASTER_FIELD_UPLOAD_CACHE_H
#include <vector>
#include <cstdint>
#include <cstring>

// Each in-flight resource slot owns its own baseline. Compare exact bytes and
// upload contiguous changed rows; a slot can lag several frames behind the CPU.
struct RemasterFieldUploadCache
{
	std::vector<uint8_t> previous;
	size_t rowBytes = 0;
	template<class Upload> bool update(const void *data, size_t stride, size_t rows, Upload upload)
	{
		const auto *bytes = static_cast<const uint8_t *>(data);
		const size_t size = stride*rows;
		if (stride != rowBytes || previous.size() != size)
		{
			upload(0, rows); previous.assign(bytes, bytes+size); rowBytes = stride; return true;
		}
		bool changed = false;
		for (size_t y = 0; y < rows;)
		{
			if (!std::memcmp(previous.data()+y*stride, bytes+y*stride, stride)) { y++; continue; }
			const size_t first = y++;
			while (y < rows && std::memcmp(previous.data()+y*stride, bytes+y*stride, stride)) y++;
			upload(first, y-first);
			std::memcpy(previous.data()+first*stride, bytes+first*stride, (y-first)*stride);
			changed = true;
		}
		return changed;
	}
};
#endif
