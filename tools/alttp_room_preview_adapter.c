/*
 * Headless room renderer for a local snesrev/zelda3 checkout.
 *
 * Build this against the reference engine's object files (excluding main.o).
 * The include keeps its asset loader and configuration setup available without
 * copying or distributing the game engine in this repository.
 *
 * Usage: alttp-room-preview ROOM_ID OUTPUT.ppm [ENTRANCE_ID]
 * The reference engine must have a ROM-derived zelda3_assets.dat in cwd.
 */
#define main zelda3_gui_main
#include "src/main.c"
#undef main
#include "src/dungeon.h"
#include "src/misc.h"

/* The linked inspection export observes the reference PPU's winning BG
 * priority/color word for each pixel. The normal gallery path never enables
 * this callback. The resulting raw file is converted to RemasterFrame by the
 * separate profile-aware tool. */
static int capture_linked;
static uint16_t capture_color[224][256];
static uint16_t capture_main[224][256];
static uint16_t capture_sub[224][256];
static uint16_t capture_layers[224][3][5];

enum { kTraceCells = 64 * 64, kTraceObjects = 2048 };
typedef struct RoomObjectTrace {
  uint16_t phase, ordinal, offset, raw, is_door;
} RoomObjectTrace;
static int trace_enabled, trace_active, trace_loaded_valid;
static uint16_t trace_phase, trace_count, trace_ordinals[4];
static uint16_t trace_owner[2][kTraceCells];
static uint16_t trace_before[2][kTraceCells];
static uint16_t trace_loaded[2][kTraceCells];
static RoomObjectTrace trace_objects[kTraceObjects];

void AlttpTraceRoomReset(void) {
  trace_active = 0;
  trace_loaded_valid = 0;
  trace_phase = trace_count = 0;
  memset(trace_ordinals, 0, sizeof(trace_ordinals));
  memset(trace_owner, 0, sizeof(trace_owner));
}

void AlttpTraceRoomPhase(unsigned phase) {
  if (phase > 3) Die("Invalid room trace phase");
  trace_phase = phase;
}

void AlttpTraceRoomObjectBegin(unsigned offset, unsigned raw, unsigned door) {
  if (!trace_enabled) return;
  if (trace_count == kTraceObjects || trace_active) Die("Room trace object limit reached");
  RoomObjectTrace *record = &trace_objects[trace_count++];
  record->phase = trace_phase;
  record->ordinal = trace_ordinals[trace_phase]++;
  record->offset = offset;
  record->raw = raw;
  record->is_door = door;
  memcpy(trace_before[0], dung_bg1, sizeof(trace_before[0]));
  memcpy(trace_before[1], dung_bg2, sizeof(trace_before[1]));
  trace_active = 1;
}

void AlttpTraceRoomObjectEnd(void) {
  if (!trace_enabled) return;
  if (!trace_active) Die("Room trace ended without an object");
  for (int i = 0; i < kTraceCells; i++) {
    if (dung_bg1[i] != trace_before[0][i]) trace_owner[0][i] = trace_count;
    if (dung_bg2[i] != trace_before[1][i]) trace_owner[1][i] = trace_count;
  }
  trace_active = 0;
}

void AlttpTraceRoomLoaded(void) {
  if (!trace_enabled) return;
  memcpy(trace_loaded[0], dung_bg1, sizeof(trace_loaded[0]));
  memcpy(trace_loaded[1], dung_bg2, sizeof(trace_loaded[1]));
  trace_loaded_valid = 1;
}

void AlttpCaptureLine(Ppu *ppu, unsigned line) {
  if (!capture_linked || line >= 224) return;
  // The PPU draws the visible screen at the fixed center of its wide row.
  // extraLeftCur only controls how much *additional* artwork is drawn to its
  // left; subtracting it here shifts the captured colors relative to the BG
  // priority words and duplicates room features across stitched viewports.
  const uint32 *rgb = (const uint32 *)(ppu->renderBuffer + line * ppu->renderPitch) +
    kPpuExtraLeftRight;
  for (int layer = 0; layer < 3; layer++) {
    const BgLayer *bg = &ppu->bgLayer[layer];
    capture_layers[line][layer][0] = bg->hScroll;
    capture_layers[line][layer][1] = bg->vScroll;
    capture_layers[line][layer][2] = bg->tilemapAdr;
    capture_layers[line][layer][3] = bg->tileAdr;
    capture_layers[line][layer][4] = (bg->tilemapWider ? 1 : 0) |
      (bg->tilemapHigher ? 2 : 0) |
      ((ppu->screenEnabled[0] >> layer & 1) << 2) |
      ((ppu->screenEnabled[1] >> layer & 1) << 3);
  }
  for (int x = 0; x < 256; x++) {
    uint32 color = rgb[x];
    capture_color[line][x] = ((color >> 19) & 31) |
      ((color >> 11) & 31) << 5 | ((color >> 3) & 31) << 10;
    capture_main[line][x] = ppu->bgBuffers[0].data[x + kPpuExtraLeftRight];
    capture_sub[line][x] = ppu->preventMathMode != 3 && ppu->addSubscreen &&
      ppu->mathEnabled && ppu->screenEnabled[1] ?
      ppu->bgBuffers[1].data[x + kPpuExtraLeftRight] : 0x0500;
  }
}

static void WriteU16(FILE *file, uint16_t value) {
  fputc(value & 255, file);
  fputc(value >> 8, file);
}

static void WriteRoomProvenance(const char *raw_path, int room) {
  if (!trace_loaded_valid || trace_active) Die("Room provenance was not settled");
  char path[4096];
  if (snprintf(path, sizeof(path), "%s.prov", raw_path) >= sizeof(path))
    Die("Room provenance path is too long");
  FILE *file = fopen(path, "wb");
  if (!file) Die("Cannot open room provenance output");
  fwrite("ALTPRV1\0", 1, 8, file);
  WriteU16(file, room);
  WriteU16(file, trace_count);
  for (int i = 0; i < trace_count; i++) {
    RoomObjectTrace *record = &trace_objects[i];
    WriteU16(file, record->phase);
    WriteU16(file, record->ordinal);
    WriteU16(file, record->offset);
    WriteU16(file, record->raw);
    WriteU16(file, record->is_door);
  }
  for (int layer = 0; layer < 2; layer++)
    for (int i = 0; i < kTraceCells; i++) {
      uint16_t final_word = layer ? dung_bg2[i] : dung_bg1[i];
      WriteU16(file, final_word == trace_loaded[layer][i] ? trace_owner[layer][i] : 0xffff);
    }
  for (int i = 0; i < kTraceCells; i++) WriteU16(file, dung_bg1[i]);
  for (int i = 0; i < kTraceCells; i++) WriteU16(file, dung_bg2[i]);
  if (fclose(file)) Die("Cannot write room provenance output");
}

static void WriteRoomAttributes(const char *raw_path, int room) {
  char path[4096];
  if (snprintf(path, sizeof(path), "%s.attr", raw_path) >= sizeof(path))
    Die("Room attribute path is too long");
  FILE *file = fopen(path, "wb");
  if (!file) Die("Cannot open room attribute output");
  fwrite("ALTPAT1\0", 1, 8, file);
  WriteU16(file, room);
  // TileDetection_Execute selects the second BG2 attribute plane with +0x1000
  // when Link occupies the lower gameplay layer. Both are 64x64 byte maps.
  fwrite(dung_bg2_attr_table, 1, kTraceCells * 2, file);
  if (fclose(file)) Die("Cannot write room attributes");
  if (snprintf(path, sizeof(path), "%s.state.json", raw_path) >= sizeof(path))
    Die("Room state path is too long");
  file = fopen(path, "wb");
  if (!file) Die("Cannot open room state output");
  fprintf(file, "{\"format\":\"alttp-room-traversal-state-v1\",\"room_id\":%d,"
          "\"entrance\":%d,\"link_xy\":[%d,%d],\"plane\":%d,\"collision_mode\":%d,"
          "\"stair_kind\":%d,\"default_wall_attribute\":%d,\"stairs\":[",
          room, which_entrance, link_x_coord & 511, link_y_coord & 511,
          link_is_on_lower_level, dung_hdr_collision, kind_of_in_room_staircase & 255,
          attributes_for_tile[0]);
  int count = 0;
  for (int table = 0; table < 2; table++) {
    int a = table ? dung_num_stairs_1 : dung_num_inroom_upnorth_stairs;
    int b = table ? dung_num_stairs_2 : dung_num_inroom_southdown_stairs;
    int c = table ? dung_num_stairs_wet : dung_num_interpseudo_upnorth_stairs;
    int end = a > b ? a : b;
    if (c > end) end = c;
    for (int i = 0; i < end; i += 2) {
      int pos = table ? dung_stairs_table_2[i >> 1] : dung_stairs_table_1[i >> 1];
      const char *high_side = NULL;
      if (!table) {
        if (i < dung_num_inroom_upnorth_stairs) high_side = "north";
        else if (i < dung_num_inroom_southdown_stairs) high_side = "south";
        else if (i < dung_num_interpseudo_upnorth_stairs) high_side = "north";
        else if (i < dung_num_inroom_upnorth_stairs_water) high_side = "north";
        else if (i < dung_num_activated_water_ladders) high_side = "north";
      } else {
        if (i < dung_num_stairs_1) high_side = "north";
        else if (i < dung_num_stairs_2) high_side = "south";
		else if (i < dung_num_stairs_wet) high_side = "south";
      }
      fprintf(file, "%s{\"index\":%d,\"table\":%d,\"tile_xy\":[%d,%d]",
              count++ ? "," : "", i / 2, table, pos & 63, (pos >> 6) & 63);
      if (high_side) fprintf(file, ",\"high_side\":\"%s\"", high_side);
      fprintf(file, "}");
    }
  }
  fprintf(file, "]}\n");
  if (fclose(file)) Die("Cannot write room state");
}

static int SyncRoomTilemaps(void) {
  // A direct camera jump does not stream every offscreen quadrant into VRAM.
  // The room loader has already expanded the complete 64x64 maps in WRAM;
  // publish those map words to the PPU before inspection rendering.
  Ppu *ppu = g_zenv.ppu;
  if (ppu->mode != 1 || !ppu->bgLayer[0].tilemapWider ||
      !ppu->bgLayer[0].tilemapHigher || !ppu->bgLayer[1].tilemapWider ||
      !ppu->bgLayer[1].tilemapHigher)
    return 0;
  for (int y = 0; y < 64; y++)
    for (int x = 0; x < 64; x++) {
      int source = y * 64 + x;
      int quadrant = (y >> 5) * 0x800 + (x >> 5) * 0x400;
      int address = quadrant + (y & 31) * 32 + (x & 31);
      ppu->vram[(ppu->bgLayer[0].tilemapAdr + address) & 0x7fff] = dung_bg1[source];
      ppu->vram[(ppu->bgLayer[1].tilemapAdr + address) & 0x7fff] = dung_bg2[source];
    }
  return 1;
}

static void WriteLinkedRaw(const char *path, int room, int sx, int sy) {
  uint32 pixels[512 * 480];
  int room_x = (room & 15) << 9;
  int room_y = (room >> 4) << 9;
  BG1HOFS_copy = BG1HOFS_copy2 = BG2HOFS_copy = BG2HOFS_copy2 = room_x + sx;
  BG1VOFS_copy = BG1VOFS_copy2 = BG2VOFS_copy = BG2VOFS_copy2 = room_y + sy;
  ZeldaRunFrameInternal(0, 0);
  if (!SyncRoomTilemaps()) Die("Linked inspection requires 64x64 BG1/BG2 maps");
  memset(pixels, 0, sizeof(pixels));
  capture_linked = 1;
  ZeldaDrawPpuFrame((uint8 *)pixels, 512 * 4, kPpuRenderFlags_NewRenderer);
  capture_linked = 0;
  FILE *file = fopen(path, "wb");
  if (!file) Die("Cannot open linked room output");
  fwrite("ALTPPD1\0", 1, 8, file);
  WriteU16(file, room); WriteU16(file, sx); WriteU16(file, sy);
  for (int y = 0; y < 224; y++) {
    for (int layer = 0; layer < 3; layer++)
      for (int field = 0; field < 5; field++)
        WriteU16(file, capture_layers[y][layer][field]);
    for (int x = 0; x < 256; x++) {
      WriteU16(file, capture_color[y][x]);
      WriteU16(file, capture_main[y][x]);
      WriteU16(file, capture_sub[y][x]);
    }
  }
  for (int i = 0; i < 0x8000; i++) WriteU16(file, g_zenv.ppu->vram[i]);
  for (int i = 0; i < 0x100; i++) WriteU16(file, g_zenv.ppu->cgram[i]);
  if (fclose(file)) Die("Cannot write linked room output");
  WriteRoomProvenance(path, room);
  WriteRoomAttributes(path, room);
}

static void WriteRoomPpm(const char *path, int room) {
  uint8 output[512 * 512 * 3] = {0};
  uint32 pixels[512 * 480];
  int room_x = (room & 15) << 9;
  int room_y = (room >> 4) << 9;

  for (int sy = 0; sy < 512; sy += 224) {
    for (int sx = 0; sx < 512; sx += 256) {
      BG1HOFS_copy = BG1HOFS_copy2 = BG2HOFS_copy = BG2HOFS_copy2 = room_x + sx;
      BG1VOFS_copy = BG1VOFS_copy2 = BG2VOFS_copy = BG2VOFS_copy2 = room_y + sy;
      ZeldaRunFrameInternal(0, 0);
      SyncRoomTilemaps();
      memset(pixels, 0, sizeof(pixels));
      ZeldaDrawPpuFrame((uint8 *)pixels, 512 * 4, 0);
      int rows = sy + 224 <= 512 ? 224 : 512 - sy;
      for (int y = 0; y < rows; y++) {
        for (int x = 0; x < 256; x++) {
          uint32 color = pixels[y * 512 + x];
          uint8 *dest = output + ((sy + y) * 512 + sx + x) * 3;
          dest[0] = color >> 16;
          dest[1] = color >> 8;
          dest[2] = color;
        }
      }
    }
  }

  FILE *file = fopen(path, "wb");
  if (!file) Die("Cannot open room preview output");
  fprintf(file, "P6\n512 512\n255\n");
  if (fwrite(output, 1, sizeof(output), file) != sizeof(output))
    Die("Cannot write room preview");
  fclose(file);
}

static int ChooseEntrance(int room) {
  int count = kEntranceData_rooms_SIZE / sizeof(uint16);
  int best = 0, best_distance = 10000;
  for (int i = 0; i < count; i++) {
    int other = kEntranceData_rooms[i];
    if (other >= 320) continue;
    int distance = abs((room & 15) - (other & 15)) +
                   abs((room >> 4) - (other >> 4)) * 2;
    if (distance < best_distance) {
      best = i;
      best_distance = distance;
    }
  }
  return best;
}

int main(int argc, char **argv) {
  if (argc == 2 && strcmp(argv[1], "--entrances") == 0) {
    ParseConfigFile(NULL);
    LoadAssets();
    int count = kEntranceData_rooms_SIZE / sizeof(uint16);
    for (int i = 0; i < count; i++)
      printf("entrance=%d room=%03x theme=%d palace=%d floor=%d scroll=%d,%d starting_bg=%d\n",
             i, kEntranceData_rooms[i], kEntranceData_blockset[i],
             kEntranceData_palace[i], kEntranceData_floor[i],
             kEntranceData_scrollX[i], kEntranceData_scrollY[i],
             kEntranceData_startingBg[i]);
    return 0;
  }
  int linked = (argc >= 7 && argc <= 9) && strcmp(argv[1], "--linked") == 0;
  trace_enabled = linked;
  if (!linked && argc != 3 && argc != 4) {
    fprintf(stderr, "Usage: %s ROOM_ID OUTPUT.ppm [ENTRANCE_ID]\n"
       "       %s --linked ROOM_ID OUTPUT.raw ENTRANCE_ID VIEWPORT_X VIEWPORT_Y [ADVANCE_FRAMES [ROUTE]]\n", argv[0], argv[0]);
    return 2;
  }
  int room = (int)strtol(argv[linked ? 2 : 1], NULL, 0);
  if (room < 0 || room >= 320) Die("Room ID must be 0..319");

  ParseConfigFile(NULL);
  LoadAssets();
  // Preview the room's artwork without its lights-out color math. The ROM
  // header remains untouched; only this process's extracted asset copy changes.
  uint8 *room_header = kDungeonRoomHeaders + kDungeonRoomHeadersOffs[room];
  int preview_forced_lit = room_header[0] & 1;
  room_header[0] &= ~1;
  ZeldaInitialize();
  if (SDL_Init(0) != 0) Die("Cannot initialize SDL");
  g_audio_mutex = SDL_CreateMutex();
  if (!g_audio_mutex) Die("Cannot create audio mutex");
  ZeldaRunFrameInternal(0, 1);

  int entrance = linked ? (int)strtol(argv[4], NULL, 0) :
    (argc == 4 ? (int)strtol(argv[3], NULL, 0) : ChooseEntrance(room));
  if (linked && entrance == -1) entrance = ChooseEntrance(room);
  if (entrance < 0 || (size_t)entrance >= kEntranceData_rooms_SIZE / sizeof(uint16))
    Die("Entrance ID is out of bounds");
  int source_room = kEntranceData_rooms[entrance];
  int theme = kEntranceData_blockset[entrance];
  kEntranceData_rooms[entrance] = room;
  which_entrance = entrance;
  sram_progress_indicator = 3;
  // Direct pre-dungeon entry bypasses Module05_LoadFile, which normally
  // initializes the default collision LUT. Without it most walls become 00.
  Init_LoadDefaultTileAttr();
  Module_PreDungeon();
  for (int frame = 0; frame < 90; frame++) ZeldaRunFrameInternal(0, 1);
  if (dungeon_room_index != room) {
    fprintf(stderr, "room=%03x current=%03x main=%d sub=%d\n",
            room, dungeon_room_index, main_module_index, submodule_index);
    Die("Reference engine did not settle in the requested room");
  }
  if (linked) {
    int sx = (int)strtol(argv[5], NULL, 0), sy = (int)strtol(argv[6], NULL, 0);
    int advance = argc >= 8 ? (int)strtol(argv[7], NULL, 0) : 0;
    if (sx < 0 || sx > 256 || sy < 0 || sy > 288) Die("Viewport is outside room");
    if (advance < 0 || advance > 240) Die("Frame advance is outside 0..240");
    for (int frame = 0; frame < advance; frame++) ZeldaRunFrameInternal(0, 1);
    if (argc == 9) {
      char path[4096];
      if (snprintf(path, sizeof(path), "%s.route.jsonl", argv[3]) >= sizeof(path)) Die("Route path too long");
      FILE *route = fopen(path, "wb");
      if (!route) Die("Cannot open route trace");
      const char *step = argv[8];
      int frame = 0;
      fprintf(route, "{\"frame\":0,\"x\":%d,\"y\":%d,\"plane\":%d,\"submodule\":%d}\n",
              link_x_coord & 511, link_y_coord & 511, link_is_on_lower_level, submodule_index);
      while (*step) {
        char dir = *step++;
        uint16 input = dir == 'N' ? 0x10 : dir == 'S' ? 0x20 : dir == 'W' ? 0x40 : dir == 'E' ? 0x80 : 0;
        if (!input && dir != '_') Die("Route direction must be N/S/W/E/_");
        char *end;
        long frames = strtol(step, &end, 10);
        if (end == step || frames < 1 || frames > 2000) Die("Invalid route frame count");
        step = end;
        if (*step == ',') step++;
        for (int i = 0; i < frames; i++) {
          ZeldaRunFrameInternal(input, 1);
          fprintf(route, "{\"frame\":%d,\"x\":%d,\"y\":%d,\"plane\":%d,\"submodule\":%d,\"room\":%d}\n",
                  ++frame, link_x_coord & 511, link_y_coord & 511,
                  link_is_on_lower_level, submodule_index, dungeon_room_index);
          if (dungeon_room_index != room) Die("Route left requested room");
        }
      }
      if (fclose(route)) Die("Cannot write route trace");
    }
    WriteLinkedRaw(argv[3], room, sx, sy);
  } else {
    WriteRoomPpm(argv[2], room);
  }
  printf("room=%03x entrance=%d source_room=%03x theme=%d main=%d sub=%d forced_lit=%d\n",
          room, entrance, source_room, theme, main_module_index, submodule_index,
          preview_forced_lit);
  return 0;
}
