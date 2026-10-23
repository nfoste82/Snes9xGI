// Canonical version-1 decoded tile identity, shared by renderer and evidence readers.
#ifndef _REMASTER_TILE_HASH_H_
#define _REMASTER_TILE_HASH_H_
#include <cstdint>
#include <cstddef>
inline uint64_t S9xRemasterHashTile(uint8_t bitDepth,const uint8_t *indices)
{
	const uint8_t header[]={1,1,bitDepth,8,8};
	uint64_t hash=UINT64_C(14695981039346656037);
	for(uint8_t value:header)hash=(hash^value)*UINT64_C(1099511628211);
	for(size_t i=0;i<64;i++)hash=(hash^indices[i])*UINT64_C(1099511628211);
	return hash;
}
#endif
