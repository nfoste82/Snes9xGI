#ifndef _REMASTER_OVERWORLD_CAPTURE_H_
#define _REMASTER_OVERWORLD_CAPTURE_H_

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// Evidence only, never a geometry cache. All offsets/sizes are BYTES; SNES
// words are little endian. The manifest is written last and attests file I/O,
// not map/graphics parity or legal movement. Contains local ROM-derived data:
// do not redistribute captures. No native C++ structs are dumped.
struct RemasterOverworldCapture
{
	struct Section { std::string name; size_t offset, size; uint64_t hash; };
	std::vector<uint8_t> bytes={'A','L','T','P','O','W','1',0};
	std::vector<Section> sections;
	std::string metadata;

	static uint64_t Hash(const uint8_t *data,size_t size)
	{
		uint64_t h=UINT64_C(14695981039346656037);
		for(size_t i=0;i<size;i++)h=(h^data[i])*UINT64_C(1099511628211);
		return h;
	}
	void Add(const char *name,const uint8_t *data,size_t size)
	{
		sections.push_back({name,bytes.size(),size,Hash(data,size)});
		if(size)bytes.insert(bytes.end(),data,data+size);
	}
	bool Write(const std::string &path) const
	{
		// The parent packet was exclusively created; do not clobber orphaned
		// sidecars either. On failure no complete manifest is left behind.
		const std::string bin=path+".overworld-input.bin",json=path+".overworld.json";
		FILE *out=std::fopen(bin.c_str(),"wbx");if(!out)return false;
		const bool wrote=std::fwrite(bytes.data(),1,bytes.size(),out)==bytes.size();
		const bool closed=std::fclose(out)==0;
		if(!wrote || !closed){std::remove(bin.c_str());return false;}
		std::ostringstream s;
		s << "{\"format\":\"alttp-live-overworld-evidence-v1\",\"complete_inputs\":true,"
		  << "\"base_rom_map_matches_live\":null,\"geometry_approved\":false,"
		  << "\"raster_replay_complete\":false,\"boundary\":\"first completed capture frame, after FLUSH_REDRAW\","
		  << "\"byte_order\":\"little\",\"input_bytes\":" << bytes.size()
		  << ",\"input_fnv1a64\":\"" << std::hex << Hash(bytes.data(),bytes.size()) << std::dec << "\","
		  << metadata << ",\"sections\":[";
		for(size_t i=0;i<sections.size();i++) {
			const auto &v=sections[i];if(i)s<<',';
			s << "{\"name\":\""<<v.name<<"\",\"offset\":"<<v.offset<<",\"size\":"<<v.size
			  <<",\"fnv1a64\":\""<<std::hex<<v.hash<<std::dec<<"\"}";
		}
		s << "]}\n";const auto text=s.str();
		out=std::fopen(json.c_str(),"wbx");if(!out){std::remove(bin.c_str());return false;}
		const bool jsonWrote=std::fwrite(text.data(),1,text.size(),out)==text.size();
		const bool jsonClosed=std::fclose(out)==0;
		if(!jsonWrote || !jsonClosed){std::remove(json.c_str());std::remove(bin.c_str());return false;}
		return true;
	}
};

// Source: snesrev/zelda3 45a149d8b4bb8098f53d1009f76e76e7d8b79bbf,
// src/variables.h and overworld.c: DecompressAndDrawAllQuadrants,
// OverworldCopyMap16ToBuffer ($82:FD87), Overworld_DrawMap16 ($9B:C980).
// WRAM $7E:2000..3FFF is 64x64 u16 Map16, stride128 bytes (not Map8
// collision). $4000 is overlay Map16 scratch; $7F:2000 is Map8 upload
// scratch, NOT a completed overworld collision array. Keep all WRAM because
// event writers, pending DMA, overlays and graphics selectors change it.
inline std::string S9xRemasterOverworldWramMetadata(const uint8_t *ram)
{
	auto word=[&](size_t p){return unsigned(ram[p])|(unsigned(ram[p+1])<<8);};
	std::ostringstream s;
	s << "\"address_source\":\"snesrev/zelda3@45a149d8b4bb8098f53d1009f76e76e7d8b79bbf:src/variables.h,src/overworld.c\","
	  << "\"wram_fields\":{";
	struct Field {const char *name;unsigned offset,width;};
	const Field fields[]={
		{"module",0x10,1},{"submodule",0x11,1},{"indoors",0x1b,1},
		{"link_y",0x20,2},{"link_x",0x22,2},{"link_z",0x24,2},
		{"map16_load_src_off",0x84,2},{"map16_load_dst_off",0x86,2},{"map16_load_var2",0x88,2},
		{"overworld_screen_index",0x8a,2},{"subsubmodule",0xb0,1},
		{"BG1HOFS_copy2",0xe0,2},{"BG2HOFS_copy2",0xe2,2},
		{"BG1VOFS_copy2",0xe6,2},{"BG2VOFS_copy2",0xe8,2},{"link_plane",0xee,1},
		{"overworld_map_state",0x200,1},{"overworld_area_index",0x40a,2},
		{"overworld_screen_transition",0x418,1},{"current_area_of_player",0x700,2},
		{"overworld_offset_base_y",0x708,2},{"overworld_offset_mask_y",0x70a,2},
		{"overworld_offset_base_x",0x70c,2},{"overworld_offset_mask_x",0x70e,2},
		{"overworld_area_is_big",0x712,2},{"main_tile_theme_index",0xaa1,1},
		{"aux_tile_theme_index",0xaa2,1},{"sprite_graphics_index",0xaa3,1},
		{"is_in_dark_world",0xfff,1},{"sram_progress_indicator",0xf3c5,1},
		{"savegame_is_darkworld",0xf3ca,1}};
	bool first=true;for(const auto &f:fields){if(!first)s<<',';first=false;
		s<<'"'<<f.name<<"\":{\"offset\":"<<f.offset<<",\"width\":"<<f.width
		 <<",\"value\":"<<(f.width==2?word(f.offset):unsigned(ram[f.offset]))<<'}';}
	s << "},\"map16_layout\":{\"wram_offset\":8192,\"bytes\":8192,\"width\":64,\"height\":64,\"row_stride\":128,"
	  << "\"members\":\"ROM Map16 definitions: TL,TR,BL,BR u16\"},"
	  << "\"rom_definitions\":{\"map16_snes\":\"8f8000\",\"map16_file\":491520,\"count\":3752,"
	  << "\"map8_attrs_snes\":\"8e9459\",\"map8_attrs_file\":463961,\"count_attrs\":512,\"attribute_index_mask\":511}";
	return s.str();
}
#endif
