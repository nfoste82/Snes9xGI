// Queue-ordered shared room cache. Every CPU-written GPU input is submit-owned.
static RemasterPerRayDispatch PrepareRemasterRoomRayCache(id<MTLCommandBuffer> command,
 RemasterGpuStageCounters &stages, RemasterMetalResources &resources,
 const RemasterFrame *frame,const std::vector<vector_uint2> &sources,
 const std::vector<RemasterEmitterPatch> &patches,
 const std::vector<RemasterHiddenDirectSample> &hidden,uint32_t visibleCount,
 bool clustered,bool eligible,bool reuseDirect,bool count)
{
 RemasterPerRayDispatch r;
 if(std::getenv("S9X_REMASTER_DISABLE_PER_RAY_CACHE")) {
  remasterPerRayCache.reset();r.outcome="disabled";return r;
 }
 if(!eligible || !frame || !frame->shiftFieldsValid || !remasterPerRayChunkPipeline) {
  remasterPerRayCache.reset();r.outcome="room-provenance";return r;
 }
 const uint32_t w=uint32_t(resources.width),h=uint32_t(resources.height);
 const auto &t=frame->lightingTrace;
 auto c=remasterPerRayCache;
 const float settings[]={float(frame->representativeEmitters),float(frame->offscreenRepresentativeEmitters),
  frame->lightingCoordinateScale,frame->reflectanceBoost,float(frame->samplesPerFrame),float(frame->indirectBounceCount)};
 std::vector<uint8_t> key(reinterpret_cast<const uint8_t*>(settings),reinterpret_cast<const uint8_t*>(settings)+sizeof(settings));
 int dx=0,dy=0;
 if(c){dx=int16_t(uint16_t(frame->shiftScrollX[0]-c->scrollX));dy=int16_t(uint16_t(frame->shiftScrollY[0]-c->scrollY));}
 bool reset=!c || !c->roomSpace || c->device!=metalDevice || c->queue!=metalCommandQueue ||
  c->width!=w || c->height!=h || c->room!=t.room || c->plane!=t.plane || c->shiftMode!=frame->shiftMode ||
  c->settings!=key || std::abs(dx)>=int(w) || std::abs(dy)>=int(h);
 if(c && (dx!=int16_t(uint16_t(frame->shiftScrollX[1]-c->secondScrollX)) ||
  dy!=int16_t(uint16_t(frame->shiftScrollY[1]-c->secondScrollY))))reset=true;
 if(c && (c->failed.load() || !c->producer || c->producer.status<MTLCommandBufferStatusCommitted ||
  c->producer.status==MTLCommandBufferStatusError || c->generation>=16000000))reset=true;
 size_t cap=512u*1024*1024;
 if(const char *v=std::getenv("S9X_REMASTER_ROOM_RAY_CACHE_MB")) {
  char *end=nullptr;long n=strtol(v,&end,10);if(end!=v && !*end && n>=0 && n<=4096)cap=size_t(n)*1024*1024;
 }
 int guard=64;
 if(const char *v=std::getenv("S9X_REMASTER_ROOM_RAY_GUARD_PX"))guard=std::max(0,std::min(256,atoi(v)));
 if(c && (c->chunks.guard!=guard || c->roomLimit!=cap))reset=true;
 if(reset) {
  c=std::make_shared<RemasterPerRayCache>();c->roomSpace=true;c->device=metalDevice;c->queue=metalCommandQueue;
  c->roomLimit=cap;
  c->width=w;c->height=h;c->room=t.room;c->plane=t.plane;c->shiftMode=frame->shiftMode;c->settings=key;
  c->scrollX=frame->shiftScrollX[0];c->scrollY=frame->shiftScrollY[0];
  size_t columns=256;while(columns<sources.size()+64)columns*=2;
  c->chunks.configure(columns,cap,guard,w,h);
  dx=dy=0;
 }
 if(!c->chunks.pool){remasterPerRayCache.reset();r.outcome="cap";return r;}
 // Accumulate exact wrapped BG deltas, rather than interpreting raw wraps as jumps.
 int x=c->scrollX+dx,y=c->scrollY+dy;
 // scrollX/Y retain the unwrapped basis; low16 bits remain the PPU register value.
 c->scrollX=x;c->scrollY=y;
 c->secondScrollX=frame->shiftScrollX[1];c->secondScrollY=frame->shiftScrollY[1];
 const uint32_t generation=++c->generation;
 auto &ch=c->chunks;ch.begin(x,y,w,h,generation);
 std::vector<RemasterVisibilityBounds> changed;
 for(size_t p=0;p<ch.pixels.size();p++) {
  uint32_t at=ch.pixels[p];if(at==UINT32_MAX)continue;
  auto &chunk=ch.chunks[at/64];uint64_t bit=uint64_t(1)<<(at%64);
  PRVChunkCell now;memcpy(now.occlusion,resources.fieldUploads[0].previous.data()+p*2,2);
  memcpy(now.height,resources.fieldUploads[2].previous.data()+p*8,8);
  memcpy(now.surface,resources.fieldUploads[5].previous.data()+p*16,16);
  memcpy(&now.mesh,resources.fieldUploads[7].previous.data()+p*sizeof(now.mesh),sizeof(now.mesh));
  const auto &old=chunk.cells[at%64];bool observed=(chunk.observed&bit)!=0;
  if(!reset && (!observed || !now.equals(old))) {
   float lo=now.surface[0],hi=lo;
   auto extend=[&](const PRVChunkCell &s) {
    float a=s.surface[0],b=a;for(float z:s.mesh.corners){a=std::min(a,z);b=std::max(b,z);}
    a-=s.mesh.thickness*.5f;b+=s.mesh.thickness*.5f;if(s.mesh.solidWall>.5f)a=std::min(a,s.mesh.wallBase);
    lo=std::min(lo,a-.01f);hi=std::max(hi,b+.01f);
   };
   extend(now);if(observed)extend(old);else {lo=-INFINITY;hi=INFINITY;}
   changed.push_back({{float(int(p%w)+x)-1.001f,float(int(p/w)+y)-1.001f,lo,float(generation)},
    {float(int(p%w)+x)+2.001f,float(int(p/w)+y)+2.001f,hi,0}});
  }
  chunk.cells[at%64]=now;chunk.observed|=bit;
 }
 if(c->history.size()+changed.size()>131072) {
  c->history.clear();c->historyFloor=generation;
 }
 c->history.insert(c->history.end(),changed.begin(),changed.end());
 std::vector<uint32_t> columns;std::map<std::vector<uint32_t>,uint32_t> duplicates;
 auto roomPixel=[&](uint32_t p,std::vector<uint32_t> &k){k.push_back(uint32_t(int(p%w)+x));k.push_back(uint32_t(int(p/w)+y));};
 for(uint32_t i=0;i<sources.size();i++) {
  std::vector<uint32_t> k;
  if(i<visibleCount) {
   auto s=sources[i];k={0,s.y&3u};
   if(clustered){const auto &p=patches[(s.y&0x7fffffffu)/4];k.push_back(p.mode);k.push_back(p.count);for(uint32_t j=0;j<p.count;j++)roomPixel(p.members[j],k);}
   else roomPixel(s.x,k);
  } else {
   k={1};auto s=hidden[i-visibleCount];s.position[0]+=x;s.position[1]+=y;s.endpoint[0]+=x;s.endpoint[1]+=y;
   uint32_t bits[12];memcpy(bits,&s,sizeof(bits));k.insert(k.end(),bits,bits+12);
  }
  uint32_t ordinal=duplicates[k]++;k.push_back(ordinal);
  auto found=c->columns.find(k);
  if(found==c->columns.end() && c->columns.size()<ch.samples)found=c->columns.emplace(k,uint32_t(c->columns.size())).first;
  columns.push_back(found==c->columns.end()?UINT32_MAX:found->second);
 }
 const size_t pixels=ch.pool*64,capacity=ch.samples;
 if(!c->entries) {
  c->entries=[metalDevice newBufferWithLength:pixels*capacity*16 options:MTLResourceStorageModePrivate];
  c->receivers=[metalDevice newBufferWithLength:pixels*32 options:MTLResourceStorageModePrivate];
  c->samples=[metalDevice newBufferWithLength:capacity*64 options:MTLResourceStorageModePrivate];
  c->groupStamps=[metalDevice newBufferWithLength:ch.pool*capacity*8 options:MTLResourceStorageModePrivate];
  if(!c->entries || !c->receivers || !c->samples || !c->groupStamps){remasterPerRayCache.reset();r.outcome="allocation";return r;}
  auto clear=[command blitCommandEncoder];
  for(id<MTLBuffer> b in @[c->entries,c->receivers,c->samples,c->groupStamps])[clear fillBuffer:b range:NSMakeRange(0,b.length) value:0];
  [clear endEncoding];c->capacity=capacity;
 }
 auto shared=[&](const void *data,size_t bytes){return [metalDevice newBufferWithBytes:data length:bytes options:MTLResourceStorageModeShared];};
 uint32_t zero=0;RemasterVisibilityBounds empty={};
 std::vector<uint32_t> births;std::vector<vector_int4> info;
 for(const auto &chunk:ch.chunks){births.push_back(chunk.birth);info.push_back(vector_int4{chunk.x==INT_MIN?0:chunk.x*8,chunk.y==INT_MIN?0:chunk.y*8,int(chunk.birth),int(chunk.lastSeen==generation)});}
 r.cache=c;r.room={x,y,1,int(c->historyFloor)};r.layout={uint32_t(pixels),uint32_t(capacity)};
 r.generation={generation,generation-1};r.config={count?257u:1u,uint32_t(reset),uint32_t(sources.size()),uint32_t(c->history.size())};
 r.columns=shared(columns.empty()?&zero:columns.data(),std::max<size_t>(1,columns.size())*4);
 r.changes=shared(c->history.empty()?&empty:c->history.data(),std::max<size_t>(1,c->history.size())*sizeof(empty));
 r.pixelMap=shared(ch.pixels.data(),ch.pixels.size()*4);r.births=shared(births.data(),births.size()*4);
 id<MTLBuffer> chunkInfo=shared(info.data(),info.size()*16);
 r.dirty=c->groupStamps;r.counters=[metalDevice newBufferWithLength:256*6*4 options:MTLResourceStorageModeShared];
 if(!r.columns || !r.changes || !r.pixelMap || !r.births || !chunkInfo || !r.counters){remasterPerRayCache.reset();return RemasterPerRayDispatch{};}
 memset(r.counters.contents,0,r.counters.length);
 auto e=stages.encoder(command,"per_ray_receivers");[e setComputePipelineState:remasterPerRayReceiverPipeline];
 [e setTexture:resources.surface atIndex:0];[e setTexture:resources.occlusion atIndex:1];[e setTexture:resources.heightField atIndex:2];
 [e setBuffer:resources.mesh offset:0 atIndex:0];[e setBuffer:c->receivers offset:0 atIndex:1];
 [e setBytes:&r.config length:16 atIndex:2];[e setBytes:&r.room length:16 atIndex:3];[e setBuffer:r.pixelMap offset:0 atIndex:4];
 [e dispatchThreads:MTLSizeMake(w,h,1) threadsPerThreadgroup:MTLSizeMake(8,8,1)];[e endEncoding];
 if(!sources.empty()) {
  e=stages.encoder(command,"per_ray_samples");[e setComputePipelineState:remasterPerRaySamplePipeline];
  [e setBuffer:resources.directSamples offset:0 atIndex:0];[e setBuffer:r.columns offset:0 atIndex:1];[e setBuffer:c->samples offset:0 atIndex:2];
  [e setBytes:&r.config length:16 atIndex:3];[e setBytes:&r.room length:16 atIndex:4];
  [e dispatchThreads:MTLSizeMake(sources.size(),1,1) threadsPerThreadgroup:MTLSizeMake(64,1,1)];[e endEncoding];
  e=stages.encoder(command,"per_ray_chunk_replay");[e setComputePipelineState:remasterPerRayChunkPipeline];
  [e setBuffer:c->samples offset:0 atIndex:0];[e setBuffer:r.columns offset:0 atIndex:1];[e setBuffer:r.changes offset:0 atIndex:2];
  [e setBuffer:r.dirty offset:0 atIndex:3];[e setBuffer:chunkInfo offset:0 atIndex:4];[e setBytes:&r.config length:16 atIndex:5];
  vector_uint4 layout={uint32_t(ch.pool),uint32_t(capacity),generation,0};[e setBytes:&layout length:16 atIndex:6];
  [e dispatchThreads:MTLSizeMake(ch.pool,sources.size(),1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];[e endEncoding];
 }
 c->producer=command;remasterPerRayCache=c;
 std::weak_ptr<RemasterPerRayCache> weak=c;
 [command addCompletedHandler:^(id<MTLCommandBuffer> done){if(done.status!=MTLCommandBufferStatusCompleted)if(auto state=weak.lock())state->failed.store(true);}];
 r.bytes=c->accountedBytes=ch.bytes;r.outcome=reuseDirect?"room-direct-reuse":reset?"room-cold":"room-active";
 r.allocatedBytes=c->entries.length+c->receivers.length+c->samples.length+c->groupStamps.length+
  r.columns.length+r.changes.length+r.pixelMap.length+r.births.length+chunkInfo.length+r.counters.length+
  ch.chunks.capacity()*sizeof(PRVChunkHost)+ch.table.capacity()*sizeof(PRVChunkTableSlot)+
  ch.pixels.capacity()*4+ch.active.capacity()*4+c->history.capacity()*sizeof(RemasterVisibilityBounds);
 r.chunkHits=ch.active.size()-ch.newChunks;r.chunkReturning=ch.returning;
 for(const auto &chunk:ch.chunks)r.chunkRetained+=chunk.x!=INT_MIN;
 return r;
}
