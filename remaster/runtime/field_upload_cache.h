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
	// Compare canonical previous bytes through an offset; never translate the
	// retained baseline. Callback gets one exact changed horizontal span.
	template<class Upload> bool updateShift(const void *data,size_t stride,size_t rows,size_t pixelBytes,int dx,int dy,Upload upload) {
		const auto *bytes=static_cast<const uint8_t *>(data);const size_t width=stride/pixelBytes;
		bool changed=dx || dy;
		for(size_t y=0;y<rows;++y) {
			auto equal=[&](size_t px) {const int sx=int(px)+dx,sy=int(y)+dy;
				return rowBytes==stride && previous.size()==stride*rows && sx>=0 && sy>=0 && sx<int(width) && sy<int(rows) &&
					!std::memcmp(bytes+y*stride+px*pixelBytes,previous.data()+size_t(sy)*stride+size_t(sx)*pixelBytes,pixelBytes);};
			size_t first=0,last=width;
			// Fast whole overlap-row comparison handles pure vertical scrolling.
			const int sy=int(y)+dy;
			if(dx==0 && sy>=0 && sy<int(rows) && rowBytes==stride && previous.size()==stride*rows &&
				!std::memcmp(bytes+y*stride,previous.data()+size_t(sy)*stride,stride))continue;
			while(first<last && equal(first))++first;
			while(last>first && equal(last-1))--last;
			if(last>first) {upload(first,y,last-first);changed=true;}
		}
		previous.assign(bytes,bytes+stride*rows);rowBytes=stride;return changed;
	}
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
