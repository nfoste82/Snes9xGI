// Exact viewport visibility retention. Included only by mac-render.mm.
// CPU state is protected by renderMutex; GPU buffers are queue-ordered, not slot-local.
#ifndef SNES9X_REMASTER_PER_RAY_CACHE_H
#define SNES9X_REMASTER_PER_RAY_CACHE_H

struct RemasterVisibilityBounds { vector_float4 low, high; };
#include "prv-chunks.h"
struct RemasterPerRayCache
{
	id<MTLDevice> device = nil;
	id<MTLCommandQueue> queue = nil;
	id<MTLCommandBuffer> producer = nil;
	id<MTLBuffer> entries = nil, receivers = nil, samples = nil;
	std::array<std::vector<uint8_t>, 4> fields;
	std::map<std::vector<uint32_t>, uint32_t> columns;
	uint32_t width = 0, height = 0, room = 0xffff, plane = 0;
	int scrollX = 0, scrollY = 0;
	int secondScrollX=0,secondScrollY=0;
	size_t capacity = 0, accountedBytes = 0, roomLimit=0;
	uint32_t generation = 0;
	std::atomic<bool> failed { false };
	bool roomSpace=false;
	PRVChunksHost chunks;
	id<MTLBuffer> groupStamps=nil;
	std::vector<RemasterVisibilityBounds> history;
	uint32_t historyFloor=1;
	uint32_t shiftMode=0;
	std::vector<uint8_t> settings;
};
static std::shared_ptr<RemasterPerRayCache> remasterPerRayCache;
static_assert(sizeof(RemasterVisibilityBounds)==32,"Metal visibility bounds ABI");
static id<MTLComputePipelineState> remasterPerRayReceiverPipeline;
static id<MTLComputePipelineState> remasterPerRaySamplePipeline;
static id<MTLComputePipelineState> remasterPerRayGroupPipeline;
static id<MTLComputePipelineState> remasterPerRayChunkPipeline;

// These inputs belong to one command buffer. Never overwrite shared CPU-visible
// columns/bounds/counters while an earlier resource slot can still consume them.
struct RemasterPerRayDispatch
{
	std::shared_ptr<RemasterPerRayCache> cache;
	id<MTLBuffer> columns = nil, changes = nil, dirty = nil, counters = nil;
	id<MTLBuffer> pixelMap=nil, births=nil;
	vector_int4 room={};
	vector_uint2 layout={};
	size_t chunkHits=0,chunkRetained=0,chunkReturning=0;
	size_t allocatedBytes=0;
	vector_uint4 config = {};
	vector_uint2 generation = {};
	const char *outcome = "bypass";
	size_t bytes = 0;
	void bind(id<MTLComputeCommandEncoder> encoder, id<MTLBuffer> dummy) const
	{
		[encoder setBuffer:config.x ? cache->entries : dummy offset:0 atIndex:11];
		[encoder setBuffer:config.x ? cache->receivers : dummy offset:0 atIndex:12];
		[encoder setBuffer:config.x ? cache->samples : dummy offset:0 atIndex:13];
		[encoder setBuffer:config.x ? columns : dummy offset:0 atIndex:14];
		[encoder setBuffer:config.x ? changes : dummy offset:0 atIndex:15];
		[encoder setBuffer:config.x ? dirty : dummy offset:0 atIndex:16];
		[encoder setBytes:&config length:sizeof(config) atIndex:17];
		[encoder setBytes:&generation length:sizeof(generation) atIndex:18];
		[encoder setBuffer:config.x ? counters : dummy offset:0 atIndex:19];
		[encoder setBuffer:room.z ? pixelMap : dummy offset:0 atIndex:20];
		[encoder setBuffer:room.z ? births : dummy offset:0 atIndex:21];
		[encoder setBytes:&room length:sizeof(room) atIndex:22];
		[encoder setBytes:&layout length:sizeof(layout) atIndex:23];
	}
};

static size_t RemasterPerRayLimit()
{
	const size_t fallback = 256u * 1024u * 1024u;
	const char *text = std::getenv("S9X_REMASTER_PER_RAY_CACHE_MB");
	if (!text) return fallback;
	char *end = nullptr;
	long value = std::strtol(text, &end, 10);
	return end != text && !*end && value >= 0 && value <= 4096 ?
		size_t(value) * 1024 * 1024 : fallback;
}

static bool RemasterPerRayTimingOnly()
{
	// Never allow the diagnostic timing override to hide panel capture counts.
	return RemasterBenchmarkActive() && std::getenv("S9X_REMASTER_PER_RAY_TIMING");
}

static RemasterVisibilityBounds RemasterPerRayBounds(size_t pixel, uint32_t width,
	const float *surface, const RemasterSurfaceMesh::Cell &mesh)
{
	float low = surface[pixel * 4], high = low;
	for (float corner : mesh.corners) { low = std::min(low, corner); high = std::max(high, corner); }
	low -= mesh.thickness * 0.5f; high += mesh.thickness * 0.5f;
	if (mesh.solidWall > 0.5f) low = std::min(low, mesh.wallBase);
	return { {float(pixel % width)-1.001f, float(pixel / width)-1.001f, low-0.01f, 0},
		{float(pixel % width)+2.001f, float(pixel / width)+2.001f, high+0.01f, 0} };
}

static bool RemasterRoomRayEnabled() {
	// Off by default: change-history replay can stall the GPU while walking.
	const char *v=std::getenv("S9X_REMASTER_ROOM_RAY_CACHE");return v && strcmp(v,"0");
}
#include "remaster-room-ray-cache.h"

static RemasterPerRayDispatch PrepareRemasterPerRayCache(id<MTLCommandBuffer> command,
	RemasterGpuStageCounters &stages, RemasterMetalResources &resources,
	const RemasterFrame *frame, const std::vector<vector_uint2> &sources,
	const std::vector<RemasterEmitterPatch> &patches,
	const std::vector<RemasterHiddenDirectSample> &hidden, uint32_t visibleCount,
	bool clustered, bool eligible, bool reuseDirect, bool count)
{
	if(RemasterRoomRayEnabled())return PrepareRemasterRoomRayCache(command,stages,resources,frame,sources,patches,hidden,visibleCount,clustered,eligible,reuseDirect,count);
	RemasterPerRayDispatch result;
	if (reuseDirect) {
		result.outcome = "direct-reuse";
		if (remasterPerRayCache) result.bytes=remasterPerRayCache->accountedBytes;
		return result;
	}
	if (std::getenv("S9X_REMASTER_DISABLE_PER_RAY_CACHE")) {
		remasterPerRayCache.reset(); result.outcome = "disabled"; return result;
	}
	if (!eligible || !frame || !remasterPerRayReceiverPipeline ||
		!remasterPerRaySamplePipeline || !remasterPerRayGroupPipeline) {
		remasterPerRayCache.reset(); return result;
	}
	const uint32_t width = uint32_t(resources.width), height = uint32_t(resources.height);
	const auto &location = frame->lightingTrace;
	auto cache = remasterPerRayCache;
	bool reset = !cache || cache->roomSpace || cache->device != metalDevice || cache->queue != metalCommandQueue ||
		cache->width != width || cache->height != height || cache->room != location.room ||
		cache->plane != location.plane || cache->scrollX != location.scrollX || cache->scrollY != location.scrollY;
	// A producer that has not been submitted might be abandoned by drawable
	// acquisition. Its speculative field baselines must never be consumed.
	if (cache && (cache->failed.load() || !cache->producer ||
		cache->producer.status < MTLCommandBufferStatusCommitted ||
		cache->producer.status == MTLCommandBufferStatusError || cache->generation == UINT32_MAX)) reset = true;
	const bool bypass = reset;
	if (reset) cache = std::make_shared<RemasterPerRayCache>();
	cache->device = metalDevice; cache->queue = metalCommandQueue;
	cache->width = width; cache->height = height; cache->room = location.room;
	cache->plane = location.plane; cache->scrollX = location.scrollX; cache->scrollY = location.scrollY;
	const size_t pixels = size_t(width) * height;
	std::vector<uint32_t> columns;
	std::map<std::vector<uint32_t>, uint32_t> duplicates;
	for (uint32_t i = 0; i < sources.size(); ++i) {
		std::vector<uint32_t> key;
		if (i < visibleCount) {
			const auto record = sources[i]; key = {0, record.y & 3u};
			if (clustered) {
				const auto &patch = patches[(record.y & 0x7fffffffu) / 4];
				key.push_back(patch.mode); key.push_back(patch.count);
				for (uint32_t j = 0; j < patch.count; ++j) key.push_back(patch.members[j]);
			} else key.push_back(record.x);
		} else {
			key = {1};
			const auto &sample = hidden[i-visibleCount];
			// Explicit exact bits, excluding radiance and struct padding.
			uint32_t bits[12]; memcpy(bits, &sample, sizeof(bits));
			key.insert(key.end(), bits, bits + 12);
		}
		key.push_back(duplicates[key]++);
		auto found = cache->columns.find(key);
		if (found == cache->columns.end())
			found = cache->columns.emplace(key, uint32_t(cache->columns.size())).first;
		columns.push_back(found->second);
	}
	const size_t capacity = std::max<size_t>(1, cache->columns.size());
	const size_t tiles = size_t((width+7)/8) * ((height+7)/8);
	// Include persistent GPU storage, worst-case bounds/dispatch storage, and
	// CPU field snapshots. Exact allocation, no power-of-two pair headroom.
	const size_t bytes = pixels*capacity*16 + pixels*32 + capacity*64 +
		tiles*sources.size()*4 + pixels*sizeof(RemasterVisibilityBounds) +
		pixels*(2+8+16+sizeof(RemasterSurfaceMesh::Cell)) + columns.size()*4 + 256*6*4;
	if (bytes > RemasterPerRayLimit()) {
		remasterPerRayCache.reset(); result.outcome = "cap"; return result;
	}
	std::vector<RemasterVisibilityBounds> changes;
	const size_t indices[] = {0,2,5,7};
	const size_t strides[] = {2,8,16,sizeof(RemasterSurfaceMesh::Cell)};
	if (!reset) {
		for (size_t pixel = 0; pixel < pixels; ++pixel) {
			bool changed = false;
			for (size_t f = 0; f < 4; ++f)
				changed |= memcmp(resources.fieldUploads[indices[f]].previous.data()+pixel*strides[f],
					cache->fields[f].data()+pixel*strides[f], strides[f]) != 0;
			if (!changed) continue;
			RemasterSurfaceMesh::Cell before, after;
			memcpy(&before, cache->fields[3].data()+pixel*sizeof(before), sizeof(before));
			memcpy(&after, resources.fieldUploads[7].previous.data()+pixel*sizeof(after), sizeof(after));
			auto old = RemasterPerRayBounds(pixel,width,reinterpret_cast<const float *>(cache->fields[2].data()),before);
			auto now = RemasterPerRayBounds(pixel,width,reinterpret_cast<const float *>(resources.fieldUploads[5].previous.data()),after);
			now.low = simd_min(old.low,now.low); now.high = simd_max(old.high,now.high);
			changes.push_back(now);
		}
	}
	// Exact conservative union per changed 8x8 cell tile. This bounds group-test
	// cost on animated artwork without dropping any old/new cell envelope.
	if (!changes.empty()) {
		std::map<std::pair<int,int>,RemasterVisibilityBounds> tiled;
		for (const auto &bounds : changes) {
			const auto key = std::make_pair(int(std::floor((bounds.low.x+1.001f)/8)),
				int(std::floor((bounds.low.y+1.001f)/8)));
			auto found = tiled.find(key);
			if (found == tiled.end()) tiled.emplace(key,bounds);
			else { found->second.low=simd_min(found->second.low,bounds.low);
				found->second.high=simd_max(found->second.high,bounds.high); }
		}
		changes.clear(); for (const auto &tile : tiled) changes.push_back(tile.second);
	}
	for (size_t f = 0; f < 4; ++f) cache->fields[f] = resources.fieldUploads[indices[f]].previous;
	result.generation = {cache->generation+1, cache->generation};
	++cache->generation;
	// Rebuild/scroll frames retain only CPU baselines. Do not allocate or clear
	// a viewport-sized pair table on every moving-camera frame.
	if (bypass) {
		cache->producer = command; remasterPerRayCache = cache;
		result.outcome = "rebuild"; return result;
	}
	if (!cache->entries || capacity > cache->capacity) {
		id<MTLBuffer> oldEntries=cache->entries, oldSamples=cache->samples;
		cache->entries = [metalDevice newBufferWithLength:pixels*capacity*16 options:MTLResourceStorageModePrivate];
		if (!cache->receivers)
			cache->receivers = [metalDevice newBufferWithLength:pixels*32 options:MTLResourceStorageModePrivate];
		cache->samples = [metalDevice newBufferWithLength:capacity*64 options:MTLResourceStorageModePrivate];
		if (!cache->entries || !cache->receivers || !cache->samples) {
			remasterPerRayCache.reset(); result.outcome = "allocation"; return result;
		}
		auto clear = [command blitCommandEncoder];
		if (oldEntries) {
			// Adding a source must not make every unrelated torch column cold.
			[clear copyFromBuffer:oldEntries sourceOffset:0 toBuffer:cache->entries destinationOffset:0 size:oldEntries.length];
			[clear fillBuffer:cache->entries range:NSMakeRange(oldEntries.length,cache->entries.length-oldEntries.length) value:0];
			[clear copyFromBuffer:oldSamples sourceOffset:0 toBuffer:cache->samples destinationOffset:0 size:oldSamples.length];
			[clear fillBuffer:cache->samples range:NSMakeRange(oldSamples.length,cache->samples.length-oldSamples.length) value:0];
		} else {
			for (id<MTLBuffer> buffer in @[cache->entries, cache->receivers, cache->samples])
				[clear fillBuffer:buffer range:NSMakeRange(0,buffer.length) value:0];
			reset = true;
		}
		[clear endEncoding]; cache->capacity = capacity;
	}
	// A rebuild frame traces normally and does not write pairs. It records a
	// baseline; the next stable miss warms the cache, never scroll-history reuse.
	cache->producer = command; remasterPerRayCache = cache;
	std::weak_ptr<RemasterPerRayCache> weak = cache;
	[command addCompletedHandler:^(id<MTLCommandBuffer> completed) {
		if (completed.status != MTLCommandBufferStatusCompleted)
			if (auto state = weak.lock()) state->failed.store(true);
	}];
	result.bytes = bytes;
	cache->accountedBytes = bytes;
	result.cache = cache; result.outcome = reset ? "cold" : "active";
	result.config = {count ? 257u : 1u, uint32_t(reset), uint32_t(sources.size()), uint32_t(changes.size())};
	uint32_t zero = 0; RemasterVisibilityBounds empty = {};
	result.columns = [metalDevice newBufferWithBytes:columns.empty() ? &zero : columns.data()
		length:std::max<size_t>(1,columns.size())*4 options:MTLResourceStorageModeShared];
	result.changes = [metalDevice newBufferWithBytes:changes.empty() ? &empty : changes.data()
		length:std::max<size_t>(1,changes.size())*sizeof(empty) options:MTLResourceStorageModeShared];
	result.dirty = [metalDevice newBufferWithLength:std::max<size_t>(1,tiles*sources.size())*4 options:MTLResourceStorageModePrivate];
	result.counters = [metalDevice newBufferWithLength:256*6*4 options:MTLResourceStorageModeShared];
	if (!result.columns || !result.changes || !result.dirty || !result.counters) {
		remasterPerRayCache.reset();
		RemasterPerRayDispatch fallback; fallback.outcome="allocation"; return fallback;
	}
	memset(result.counters.contents,0,result.counters.length);
	auto encoder = stages.encoder(command,"per_ray_receivers");
	[encoder setComputePipelineState:remasterPerRayReceiverPipeline];
	[encoder setTexture:resources.surface atIndex:0]; [encoder setTexture:resources.occlusion atIndex:1];
	[encoder setTexture:resources.heightField atIndex:2]; [encoder setBuffer:resources.mesh offset:0 atIndex:0];
	[encoder setBuffer:cache->receivers offset:0 atIndex:1];
	[encoder setBytes:&result.config length:16 atIndex:2];
	[encoder setBytes:&result.room length:16 atIndex:3];[encoder setBuffer:result.columns offset:0 atIndex:4];
	[encoder dispatchThreads:MTLSizeMake(width,height,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];
	[encoder endEncoding];
	if (!sources.empty()) {
		encoder = stages.encoder(command,"per_ray_samples");
		[encoder setComputePipelineState:remasterPerRaySamplePipeline];
		[encoder setBuffer:resources.directSamples offset:0 atIndex:0];
		[encoder setBuffer:result.columns offset:0 atIndex:1]; [encoder setBuffer:cache->samples offset:0 atIndex:2];
		[encoder setBytes:&result.config length:16 atIndex:3];
		[encoder setBytes:&result.room length:16 atIndex:4];
		[encoder dispatchThreads:MTLSizeMake(sources.size(),1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];
		[encoder endEncoding];
	}
	if (!changes.empty() && !reset && !sources.empty()) {
		encoder = stages.encoder(command,"per_ray_groups");
		[encoder setComputePipelineState:remasterPerRayGroupPipeline];
		[encoder setBuffer:cache->receivers offset:0 atIndex:0]; [encoder setBuffer:resources.directSamples offset:0 atIndex:1];
		[encoder setBuffer:result.changes offset:0 atIndex:2]; [encoder setBuffer:result.dirty offset:0 atIndex:3];
		[encoder setBytes:&result.config length:16 atIndex:4];
		vector_uint2 dimensions = {width,height}; [encoder setBytes:&dimensions length:8 atIndex:5];
		[encoder dispatchThreads:MTLSizeMake(tiles,sources.size(),1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
		[encoder endEncoding];
	}
	return result;
}
#endif
