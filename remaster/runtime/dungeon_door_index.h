#ifndef REMASTER_DUNGEON_DOOR_INDEX_H
#define REMASTER_DUNGEON_DOOR_INDEX_H

#include <array>
#include <cstdint>

// Invocation-owned necessary-word index, not an ownership cache. Priority bit13
// alone is immaterial to door recognition; palette, name and both flips remain.
// Exact bucket membership is collision-free and cells are in original y/x order.
// All arrays live in the caller's heap-owned object (small emulation stack).
struct RemasterDungeonDoorWordIndex {
	static uint16_t key(uint16_t word) {return (word&0x1fff)|((word&0xc000)>>1);}
	struct Plane {
		std::array<uint16_t,32769> offsets{};
		std::array<uint16_t,4096> cells{};
		explicit Plane(const uint8_t *map) {
			for(int i=0;i<4096;i++)++offsets[key(map[2*i]|map[2*i+1]<<8)+1];
			for(int i=1;i<=32768;i++)offsets[i]+=offsets[i-1];
			// Fill backwards using the end offsets as cursors; restore offsets by
			// shifting the resulting starts. No third 64KiB stack/scratch array.
			for(int i=4095;i>=0;i--)cells[--offsets[key(map[2*i]|map[2*i+1]<<8)+1]]=i;
			for(int i=0;i<32768;i++)offsets[i]=offsets[i+1];
			offsets[32768]=4096;
		}
	};
	Plane primary,peer;
	explicit RemasterDungeonDoorWordIndex(const uint8_t *maps):primary(maps),peer(maps+8192) {}
	struct Origins {
		const uint16_t *cells;
		int size,width,height,anchorX,anchorY;
		int count() const {return cells?size:(65-width)*(65-height);}
		int operator[](int n) const {
			if(!cells)return (n/(65-width))*64+n%(65-width);
			const int cell=cells[n],x=cell%64-anchorX,y=cell/64-anchorY;
			return x>=0 && x<=64-width && y>=0 && y<=64-height?y*64+x:-1;
		}
	};
	Origins origins(int mapOffset,uint16_t word,int w,int h,int anchorX,int anchorY) const {
		const auto &p=mapOffset?peer:primary;const int k=key(word);
		const int begin=p.offsets[k],count=p.offsets[k+1]-begin;
		// Highly repeated/adversarial words get the original bounded grid scan.
		// Never enlarge a candidate bucket into an unbounded cross product.
		return {count>(65-w)*(65-h)/2?nullptr:p.cells.data()+begin,count,w,h,anchorX,anchorY};
	}
};
#endif
