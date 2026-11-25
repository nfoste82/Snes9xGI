// Private play-test chunk metadata. Snapshots are comparison data, never trace geometry.
#include <set>
struct PRVChunkCell {
    uint8_t occlusion[2]; float height[2],surface[4]; RemasterSurfaceMesh::Cell mesh;
    bool equals(const PRVChunkCell &old)const {
        return !memcmp(occlusion,old.occlusion,2) && !memcmp(height,old.height,8) &&
            !memcmp(surface,old.surface,16) && !memcmp(&mesh,&old.mesh,sizeof(mesh));
    }
};
struct PRVChunkHost {
    int x=INT_MIN,y=INT_MIN;
    uint32_t birth=0,lastSeen=0;
    uint64_t observed=0;
    std::array<PRVChunkCell,64> cells;
};
struct PRVChunkTableSlot {int x=INT_MIN,y=INT_MIN;uint32_t slot=UINT32_MAX;};
struct PRVChunksHost {
    std::vector<PRVChunkHost> chunks;
    std::vector<PRVChunkTableSlot> table;
    std::vector<uint32_t> pixels,active;
    std::set<std::pair<int,int>> seen;
    size_t samples=0,pool=0,bytes=0;
    int tableX=0,tableY=0,guard=64;
    uint64_t reentries=0,returning=0,newChunks=0,evicted=0,missing=0;
    static int div8(int v){return v>=0?v/8:-((-v+7)/8);}
    static int mod(int v,int n){int r=v%n;return r<0?r+n:r;}
    size_t index(int x,int y)const{return size_t(mod(y,tableY))*tableX+mod(x,tableX);}
    int distance(const PRVChunkHost &c,int x,int y,int w,int h)const {
        return std::max({0,x-(c.x*8+7),c.x*8-(x+w-1),y-(c.y*8+7),c.y*8-(y+h-1)});
    }
    void erase(size_t s) {
        auto &c=chunks[s];auto &t=table[index(c.x,c.y)];
        if(t.slot==s)t={};c=PRVChunkHost{};evicted++;
    }
    void configure(size_t sampleCount,size_t cap,int band,int w,int h) {
        guard=band;tableX=(w+2*band+14)/8+2;tableY=(h+2*band+14)/8+2;
        samples=sampleCount;
        // Includes pair, endpoint, snapshot and group storage; headroom bounds
        // journal, immutable in-flight maps/bounds (three slots), sources and host maps.
        size_t perChunk=64*(samples*16+32+sizeof(PRVChunkCell))+samples*8+128;
        const size_t reserve=std::min<size_t>(cap,32*1024*1024);
        pool=cap>reserve?(cap-reserve)/perChunk:0;
        pool=std::min(pool,size_t((w+2*band+14)/8)*size_t((h+2*band+14)/8));
        chunks=std::vector<PRVChunkHost>(pool);table.assign(size_t(tableX)*tableY,{});
        bytes=pool*perChunk+reserve;pixels.resize(size_t(w)*h);active.clear();seen.clear();evicted=0;
    }
    void begin(int x,int y,int w,int h,uint32_t frame) {
        if(seen.size()>32768)seen.clear();
        reentries=returning=newChunks=missing=0;
        active.clear();std::fill(pixels.begin(),pixels.end(),UINT32_MAX);
        for(size_t s=0;s<pool;s++)if(chunks[s].x!=INT_MIN && distance(chunks[s],x,y,w,h)>guard)erase(s);
        for(int cy=div8(y);cy<=div8(y+h-1);cy++)for(int cx=div8(x);cx<=div8(x+w-1);cx++) {
            auto &t=table[index(cx,cy)];size_t s=SIZE_MAX;
            if(t.x==cx && t.y==cy && t.slot<pool)s=t.slot;
            if(s==SIZE_MAX) {
                bool old=seen.count({cx,cy});if(old)returning++;
                for(size_t k=0;k<pool;k++)if(chunks[k].x==INT_MIN){s=k;break;}
                if(s==SIZE_MAX) {
                    int far=0;for(size_t k=0;k<pool;k++){int d=distance(chunks[k],x,y,w,h);if(d>far || (d==far && d>0 && s!=SIZE_MAX && chunks[k].lastSeen<chunks[s].lastSeen)){far=d;s=k;}}
                    if(s!=SIZE_MAX)erase(s);
                }
                if(s==SIZE_MAX){missing++;continue;}
                if(t.slot<pool && (t.x!=cx || t.y!=cy)){missing++;continue;}
                chunks[s].x=cx;chunks[s].y=cy;chunks[s].birth=frame;
                t={cx,cy,uint32_t(s)};newChunks++;seen.insert({cx,cy});
            } else if(chunks[s].lastSeen+1<frame){reentries++;returning++;}
            auto &c=chunks[s];c.lastSeen=frame;active.push_back(uint32_t(s));
            for(int py=std::max(y,cy*8);py<std::min(y+h,cy*8+8);py++)for(int px=std::max(x,cx*8);px<std::min(x+w,cx*8+8);px++)
                pixels[size_t(py-y)*w+px-x]=uint32_t(s*64+(py-cy*8)*8+px-cx*8);
        }
    }
};
