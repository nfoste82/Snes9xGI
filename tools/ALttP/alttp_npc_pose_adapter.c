/* Offline draw-only harness for a local, ROM-backed snesrev/zelda3 engine.
 * No AI/state mutation in the emulator; each pose starts from the same RAM.
 * Usage: alttp-npc-poses TYPE GFX PALETTE DRAW DIRECTIONS STEPS WORLD FOLLOWER VARIANT
 */
#define main zelda3_gui_main
#include "src/main.c"
#undef main
#include "src/sprite_main.h"
#include "src/sprite.h"
#include "src/load_gfx.h"
#include "src/dungeon.h"
#include "src/misc.h"
#include "src/player_oam.h"

/* Existing local engine objects may contain the room inspection hooks. */
void AlttpCaptureLine(Ppu *ppu, unsigned line) { (void)ppu; (void)line; }
void AlttpTraceRoomReset(void) {}
void AlttpTraceRoomPhase(unsigned p) { (void)p; }
void AlttpTraceRoomObjectBegin(unsigned a, unsigned b, unsigned c) { (void)a; (void)b; (void)c; }
void AlttpTraceRoomObjectEnd(void) {}
void AlttpTraceRoomLoaded(void) {}

static uint8 checkpoint[sizeof(g_ram)];
static uint16 vram_checkpoint[0x8000];
static const char *draw;
static int variant;

static void Draw(int type) {
  #define DRAW(name, call) if (!strcmp(draw, name)) { call; return; }
  DRAW("smithy", Smithy_Draw(0))
  DRAW("frog", SmithyFrog_Draw(0))
  DRAW("kid", BugNetKid_Draw(0))
  DRAW("story", StoryTeller_1_Draw(0))
  DRAW("father", FluteBoyFather_Draw(0))
  DRAW("hideout", BlindHideoutGuy_Draw(0))
  DRAW("thief", Thief_Draw(0))
  DRAW("sweep", SweepingLady_Draw(0))
  DRAW("hobo", Hobo_Draw(0))
  DRAW("lumber", Lumberjacks_Draw(0))
  DRAW("flute", (void)FluteBoy_Draw(0))
  DRAW("stump", FluteAardvark_Draw(0))
  DRAW("lady", Lady_Draw(0))
  DRAW("maze", MazeGameGuy_Draw(0))
  DRAW("fortune", FortuneTeller_Draw(0))
  DRAW("witch", Witch_Draw(0))
  DRAW("middle", MiddleAgedMan_Draw(0))
  DRAW("single", SpriteDraw_SingleLarge(0))
  DRAW("tutorial", TutorialSoldier_Draw(0))
  DRAW("zora", ZoraKing_Draw(0))
  DRAW("archery", ArcheryGameGuy_Draw(0))
  DRAW("queen", FaerieQueen_Draw(0))
  DRAW("priest", Priest_Draw(0))
  DRAW("uncle", Uncle_Draw(0))
  DRAW("maiden", CrystalMaiden_Draw(0))
  DRAW("oldman", OldMountainMan_Draw(0))
  DRAW("bombshop", BombShopEntity_Draw(0))
  DRAW("kiki", (void)Kiki_Draw(0))
  DRAW("bully", Bully_Draw(0))
  DRAW("shop", Shopkeeper_Draw(0))
  DRAW("drink", DrinkingGuy_Draw(0))
  DRAW("catfish", GreatCatfish_Draw(0))
  DRAW("fairy", BigFaerie_Draw(0))
  DRAW("dig", DiggingGameGuy_Draw(0))
  DRAW("agahnim", Agahnim_Draw(0))
  DRAW("ganon", Ganon_Draw(0))
  DRAW("link", LinkOam_Main())
  DRAW("spear", PsychoTrooper_Draw(0))
  DRAW("javelin", JavelinTrooper_Draw(0))
  DRAW("archer", ArcherSoldier_Draw(0))
  DRAW("bombguard", BombTrooper_Draw(0))
  DRAW("recruit", Recruit_Draw(0))
  DRAW("large", SpriteDraw_SingleLarge(0))
  DRAW("rat", SpriteDraw_SingleLarge(0))
  DRAW("rope", SpriteDraw_SingleLarge(0))
  DRAW("mini-helmasaur", SpriteDraw_SingleLarge(0))
  DRAW("armos-knight", ArmosKnight_Draw(0))
  DRAW("armos-statue", Armos_Draw(0))
  DRAW("hardhat", HardHatBeetle_Draw(0))
  DRAW("stalfos", Stalfos_Draw(0))
  DRAW("gibdo", Gibdo_Draw(0))
  DRAW("wizzrobe", Wizzrobe_Draw(0))
  DRAW("freezor", Freezor_Draw(0))
  DRAW("pengator", Pengator_Draw(0))
  DRAW("wallmaster", WallMaster_Draw(0))
  DRAW("zol", Zol_Draw(0))
  DRAW("red-bari", RedBari_Draw(0))
  DRAW("popo", Bot_Draw(0))
  DRAW("eyegore", Eyegore_Draw(0))
  DRAW("beamos", Beamos_Draw(0))
  DRAW("mothula", Mothula_Draw(0))
  DRAW("blind", Blind_Draw(0))
  DRAW("arrghus", Arrghus_Draw(0))
  DRAW("kholdstare", Kholdstare_Draw(0))
  DRAW("vitreous", Vitreous_Draw(0))
  DRAW("lanmola", Lanmola_Draw(0))
  DRAW("moldorm-boss", GiantMoldorm_Draw(0))
  DRAW("helmasaur-king", HelmasaurKing_Draw(0))
  #undef DRAW
  switch (type) {
  case 0x32: QuarrelBros_Draw(0); break;
  case 0x34: YoungSnitchLady_Draw(0); break;
  case 0x35: InnKeeper_Draw(0); break;
  case 0x3c: TroughBoy_Draw(0); break;
  case 0x41: case 0x42: case 0x43: Guard_HandleAllAnimation(0); break;
  case 0x74: RunningMan_Draw(0); break;
  case 0x75: (void)BottleVendor_Draw(0); break;
  case 0x78: ElderWife_Draw(0); break;
  case 0x16: Elder_Draw(0); break;
  default: Die("Unmodeled NPC draw routine");
  }
}

static void WriteTile(unsigned address, unsigned palette) {
  printf("\"indices\":[");
  uint8 indices[64];
  for (int y = 0; y < 8; y++) {
    uint16 lo = g_zenv.vram[(address + y) & 0x7fff];
    uint16 hi = g_zenv.vram[(address + y + 8) & 0x7fff];
    for (int x = 0; x < 8; x++) {
      int b = 7 - x, p = y * 8 + x;
      indices[p] = (lo >> b & 1) | (lo >> (b + 8) & 1) << 1 |
                   (hi >> b & 1) << 2 | (hi >> (b + 8) & 1) << 3;
      printf("%s%d", p ? "," : "", indices[p]);
    }
  }
  printf("],\"colors\":[");
  for (int p = 0; p < 64; p++) {
    unsigned color = main_palette_buffer[128 + palette * 16 + indices[p]];
    printf("%s%u", p ? "," : "", color);
  }
  printf("]");
}

int main(int argc, char **argv) {
  if (argc != 10) return 2;
  int type = strtol(argv[1], NULL, 0), gfx = strtol(argv[2], NULL, 0);
  int pal = strtol(argv[3], NULL, 0);
  draw = argv[4];
  int directions = atoi(argv[5]), steps = atoi(argv[6]);
  int world = atoi(argv[7]), follower = atoi(argv[8]);
  variant = atoi(argv[9]);
  if (directions < 1 || directions > 4 || steps < 1 || steps > 32 || follower < 0 || follower > 13) return 2;
  if (type < 0 || type >= 243 || gfx < 0 || gfx >= 144 || pal < 0 || pal >= 256) return 2;
  ParseConfigFile(NULL);
  LoadAssets();
  ZeldaInitialize();
  if (SDL_Init(0)) Die("Cannot initialize SDL");
  g_audio_mutex = SDL_CreateMutex();
  ZeldaRunFrameInternal(0, 1);
  which_entrance = 0;
  sram_progress_indicator = 3;
  Init_LoadDefaultTileAttr();
  Module_PreDungeon();
  for (int f = 0; f < 90; f++) ZeldaRunFrameInternal(0, 1);
  sprite_graphics_index = gfx;
  InitializeTilesets();
  if (pal < 20) Overworld_LoadPalettes(0, pal);
  savegame_is_darkworld = world ? 0x40 : 0;
  is_in_dark_world = world;
  if (pal >= 20 || !strcmp(draw, "ganon")) {
    /* Room 000's actual dungeon palette, rather than OW palette zero. */
    player_is_indoors = 1;
    dungeon_room_index = 0;
    const DungPalInfo *palette = GetDungPalInfo(pal >= 20 ? pal - 20 : GetRoomHeaderPtr(0)[1]);
    palette_main_indoors = palette->pal0;
    palette_sp0l = palette->pal1;
    palette_sp5l = palette->pal2;
    palette_sp6l = palette->pal3;
    Dungeon_LoadPalettes();
  }
  if (!strcmp(draw, "link")) {
    link_armor = link_item_gloves = 0;
    link_sword_type = link_shield_type = 0;
    Palette_Load_LinkArmorAndGloves();
  }
  if (follower) { follower_indicator = follower; LoadFollowerGraphics(); }
  memcpy(checkpoint, g_ram, sizeof(g_ram));
  memcpy(vram_checkpoint, g_zenv.vram, sizeof(vram_checkpoint));
  int guard = !strcmp(draw, "default") && type >= 0x41 && type <= 0x43;
  const int guard_frames[16] = {11,12,13,12, 4,5,6,5, 0,1,2,3, 7,8,9,10};
  printf("{\"format\":\"alttp-npc-poses-raw-v1\",\"type_id\":%d,\"gfx_index\":%d,\"palette_index\":%d,\"poses\":[", type, gfx, pal);
  int first_pose = 1;
  for (int dir = 0; dir < directions; dir++) for (int step = 0; step < steps; step++) {
    memcpy(g_ram, checkpoint, sizeof(g_ram));
    memcpy(g_zenv.vram, vram_checkpoint, sizeof(vram_checkpoint));
    memset(oam_buf, 0, 512);
    memset(bytewise_extended_oam, 0, 128);
    for (int i = 0; i < 128; i++) oam_buf[i].y = 0xf0;
    Oam_ResetRegionBases();
    oam_cur_ptr = 0x800;
    oam_ext_cur_ptr = 0xa20;
    sprite_type[0] = type;
    SpritePrep_LoadProperties(0);
    sprite_state[0] = 9;
    sprite_flags3[0] &= ~0x10; /* Ground shadow is not body geometry. */
    sprite_obj_prio[0] = 0x20;
    sprite_D[0] = sprite_head_dir[0] = dir;
    sprite_graphics[0] = guard ? guard_frames[dir * 4 + step] : step;
    sprite_subtype2[0] = variant;
    if (!strcmp(draw, "armos-knight")) {
      sprite_graphics[0] = step & 1;
      if (step >= 2) sprite_oam_flags[0] = 7; /* Last knight's red crusher palette. */
    }
    if (!strcmp(draw, "rat")) {
      static const uint8 frames[16] = {0,0,3,3,1,2,4,5,1,2,4,5,0,0,3,3};
      static const uint8 flags[16] = {0,0x40,0,0x40,0,0,0,0,0x40,0x40,0x40,0x40,0x80,0xc0,0x80,0xc0};
      static const uint8 moving[8] = {8,9,4,5,0,1,12,13};
      static const uint8 idle[8] = {10,11,6,7,2,3,14,15};
      int j = (step < 2 ? moving : idle)[dir * 2 + (step & 1)];
      sprite_graphics[0] = frames[j]; sprite_oam_flags[0] = (sprite_oam_flags[0] & 0x3f) | flags[j];
    }
    if (!strcmp(draw, "rope")) {
      static const uint8 frames[8] = {0,0,2,3,2,3,1,1}, flags[8] = {0,0x40,0,0,0x40,0x40,0,0x40};
      static const uint8 indices[8] = {4,5,2,3,0,1,6,7};
      int j = indices[dir * 2 + step];
      sprite_graphics[0] = frames[j]; sprite_oam_flags[0] = (sprite_oam_flags[0] & 0x3f) | flags[j];
    }
    if (!strcmp(draw, "mini-helmasaur")) {
      static const uint8 frames[8] = {3,4,3,4,2,2,5,5}, flags[8] = {0x40,0x40,0,0,0,0x40,0x40,0};
      int j = dir * 2 + step;
      sprite_graphics[0] = frames[j]; sprite_oam_flags[0] = (sprite_oam_flags[0] & ~0x40) | flags[j];
    }
    if (!strcmp(draw, "wizzrobe") || !strcmp(draw, "stalfos")) sprite_graphics[0] = dir * 2 + step;
    if (!strcmp(draw, "gibdo")) {
      const uint8 frames[8] = {2,3,9,11,0,1,4,5}; sprite_graphics[0] = frames[dir * 2 + step];
    }
    if (!strcmp(draw, "pengator")) sprite_graphics[0] = dir * 5 + step;
    if (!strcmp(draw, "beamos")) sprite_D[0] = step * 2;
    if (!strcmp(draw, "zol")) { sprite_ai_state[0] = 1; byte_7E0FC6 = 0; }
    if (!strcmp(draw, "kholdstare")) { sprite_A[0] = step; sprite_graphics[0] = step & 3; }
    if (!strcmp(draw, "mothula")) sprite_pause[0] = 1; /* Body only, no broad ground shadow. */
    if (!strcmp(draw, "helmasaur-king")) {
      sprite_C[0] = step / 3; /* Intact, chipped, broken and absent mask. */
      sprite_delay_aux4[0] = 1; /* Draw without bomb collision side effects. */
      for (int i = 0; i < 4; i++) overlord_x_lo[i] = (step + i) % 3;
      overlord_x_lo[4] = (step % 8) * 4;
      WORD(overlord_gen1[5]) = (step % 8) * 32;
      WORD(overlord_gen2[1]) = 0;
      overlord_gen1[7] = 48;
      overlord_gen2[3] = 0;
    }
    if (!strcmp(draw, "thief")) sprite_graphics[0] = dir * 3 + step;
    if (!strcmp(draw, "zora")) sprite_ai_state[0] = 2;
    if (!strcmp(draw, "ganon")) {
      sprite_ai_state[0] = 19; /* Draw visible body, without running boss AI. */
      sprite_D[0] = dir;
      sprite_head_dir[0] = 0;
      sprite_G[0] = 0; /* Body only; trident animation has independent state. */
      submodule_index = 0;
    }
    if (!strcmp(draw, "catfish")) sprite_graphics[0] = step + 1;
    if (!strcmp(draw, "tutorial")) { const int frames[4] = {2,1,0,3}; sprite_graphics[0] = frames[dir]; }
    if (!strcmp(draw, "spear") || !strcmp(draw, "javelin") || !strcmp(draw, "bombguard")) {
      const int frames[4] = {12,0,18,8}; sprite_graphics[0] = frames[dir] + step;
    }
    if (!strcmp(draw, "archer")) sprite_graphics[0] = guard_frames[dir * 4 + step];
    if (!strcmp(draw, "recruit")) sprite_graphics[0] = dir * 2 + step;
    if (!strcmp(draw, "bully")) sprite_D[0] = dir;
    if (!strcmp(draw, "single")) sprite_oam_flags[0] ^= step ? 0x40 : 0;
    if (!strcmp(draw, "kiki")) sprite_oam_flags[0] |= 1; /* Kiki replaces follower page with normal OBJ page. */
    if (!strcmp(draw, "maiden") && type == 0xab) sprite_oam_flags[0] = 0xb;
    if (!strcmp(draw, "shop") && type == 0xbb) sprite_oam_flags[0] = 7;
    if (!strcmp(draw, "thief") && (type == 0x29 || type == 0xbb)) sprite_oam_flags[0] = 15;
    if (!strcmp(draw, "maiden") || !strcmp(draw, "oldman") || !strcmp(draw, "kiki")) {
      /* Follower draws select animated tiles in OBJ page zero. */
      dma_var6 = dma_var7 = 0;
    }
    sprite_z[0] = 0;
    sprite_x_lo[0] = sprite_y_lo[0] = 96;
    sprite_x_hi[0] = sprite_y_hi[0] = 0;
    cur_sprite_x = cur_sprite_y = 96;
    BG2HOFS_copy2 = BG2VOFS_copy2 = 0;
    link_x_coord = link_y_coord = 200;
    if (!strcmp(draw, "lanmola") || !strcmp(draw, "moldorm-boss")) {
      /* Bounded straight travel history, spaced as the reference's lookback
       * buffers expect; use real segmented draw assembly, never a tile atlas. */
      static const int8 dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
      int count = !strcmp(draw, "lanmola") ? 64 : 128;
      sprite_subtype2[0] = step * 2;
      for (int age = 0; age < count; age++) {
        int j = (sprite_subtype2[0] - age) & (count - 1);
        int x = 96 - dx[dir] * age, y = 96 - dy[dir] * age;
        moldorm_x_lo[j] = x; moldorm_y_lo[j] = y;
        g_ram[0x1fc80 + j] = g_ram[0x1fd80 + j] = 0; /* sprite_main.c's private history high-byte aliases */
        beamos_x_hi[j] = 0;
        beamos_y_hi[j] = (int[]){0,8,4,12}[dir];
      }
      sprite_D[0] = (int[]){0,8,4,12}[dir];
      sprite_x_vel[0] = dx[dir] * 16; sprite_y_vel[0] = dy[dir] * 16;
      sprite_z_vel[0] = 0;
      sprite_delay_aux2[0] = 1; /* Moldorm tail draw without damage checks. */
      sprite_delay_aux4[0] = 0;
      if (!strcmp(draw, "lanmola")) { garnish_y_lo[0] = 7; sprite_ai_state[0] = 2; }
    }
    if (!strcmp(draw, "link")) {
      main_module_index = 7; submodule_index = 0;
      link_x_coord = link_y_coord = 96;
      link_z_coord = 0;
      link_direction_facing = dir * 2;
      link_animation_steps = step;
      link_player_handler_state = link_auxiliary_state = link_state_bits = 0;
      link_visibility_status = 0;
      link_is_bunny_mirror = link_force_hold_sword_up = link_electrocute_on_touch = 0;
      link_pose_for_item = link_grabbing_wall = bitmask_of_dragstate = 0;
      link_item_in_hand = button_b_frames = link_picking_throw_state = 0;
      link_is_in_deep_water = draw_water_ripples_or_grass = player_near_pit_state = 0;
      palette_swap_flag = sort_sprites_setting = link_is_on_lower_level = 0;
    }
    Draw(type);
    if (!strcmp(draw, "link")) {
      /* Same six body/small-part DMA transfers as NMI_DoUpdates. */
      NMI_PrepareSprites();
      memcpy(&g_zenv.vram[0x4100], &kLinkGraphics[dma_source_addr_0 - 0x8000], 0x40);
      memcpy(&g_zenv.vram[0x4120], &kLinkGraphics[dma_source_addr_1 - 0x8000], 0x40);
      memcpy(&g_zenv.vram[0x4140], &kLinkGraphics[dma_source_addr_2 - 0x8000], 0x20);
      memcpy(&g_zenv.vram[0x4000], &kLinkGraphics[dma_source_addr_3 - 0x8000], 0x40);
      memcpy(&g_zenv.vram[0x4020], &kLinkGraphics[dma_source_addr_4 - 0x8000], 0x40);
      memcpy(&g_zenv.vram[0x4040], &kLinkGraphics[dma_source_addr_5 - 0x8000], 0x20);
    }
    if (!strcmp(draw, "uncle")) {
      /* Uncle_Draw sets the same sword/shield DMA selectors as Link. */
      NMI_PrepareSprites();
      memcpy(&g_zenv.vram[0x4050], &g_ram[dma_source_addr_6], 0x40);
      memcpy(&g_zenv.vram[0x4070], &g_ram[dma_source_addr_7], 0x40);
      memcpy(&g_zenv.vram[0x4150], &g_ram[dma_source_addr_11], 0x40);
      memcpy(&g_zenv.vram[0x4170], &g_ram[dma_source_addr_12], 0x40);
    }
    if (follower) {
      /* Apply the draw-selected follower DMA, without running gameplay/AI. */
      unsigned a = 0xb940 + dma_var6 * 2, b = 0xb940 + dma_var7 * 2;
      memcpy(&g_zenv.vram[0x4200], &g_ram[a], 0x40);
      memcpy(&g_zenv.vram[0x4220], &g_ram[b], 0x40);
      memcpy(&g_zenv.vram[0x4300], &g_ram[a + 0x200], 0x40);
      memcpy(&g_zenv.vram[0x4320], &g_ram[b + 0x200], 0x40);
    }
    printf("%s{\"direction\":%d,\"step\":%d,\"graphics\":%d,\"parts\":[", first_pose ? "" : ",", dir, step, sprite_graphics[0]);
    first_pose = 0;
    int first_part = 1;
    for (int slot = 0; slot < 128; slot++) {
      OamEnt *oam = &oam_buf[slot];
      if (oam->y == 0xf0) continue;
      if (!strcmp(draw, "armos-knight") && oam->charnum == 0xe4 && (oam->flags & 0x3f) == 0x25) continue;
      if ((!strcmp(draw, "arrghus") || !strcmp(draw, "wallmaster")) && slot >= 125) continue;
      if (!strcmp(draw, "lanmola") && oam->charnum == 0x6c && oam->flags == 0x34) continue;
      if (!strcmp(draw, "ganon") && slot >= 125) continue; /* Large boss shadow. */
      int size = (bytewise_extended_oam[slot] & 2) ? 16 : 8;
      /* Draw routines add their shadow even when flags3 has it disabled. */
      if ((oam->charnum == 0x6c || oam->charnum == 0x38) &&
          (oam->flags & ~0x30) == 8) continue;
      for (int ty = 0; ty < size / 8; ty++) for (int tx = 0; tx < size / 8; tx++) {
        int hf = !!(oam->flags & 0x40), vf = !!(oam->flags & 0x80);
        unsigned tile = oam->charnum + tx + 16 * ty;
        unsigned address = ((oam->flags & 1) ? 0x5000 : 0x4000) + (tile & 255) * 16;
        printf("%s{\"x\":%d,\"y\":%d,\"slot\":%d,\"address\":%u,\"hflip\":%s,\"vflip\":%s,", first_part ? "" : ",",
               oam->x - 96 + (hf ? size / 8 - 1 - tx : tx) * 8,
               oam->y - 96 + (vf ? size / 8 - 1 - ty : ty) * 8, slot, address,
               hf ? "true" : "false", vf ? "true" : "false");
        first_part = 0;
        WriteTile(address, oam->flags >> 1 & 7);
        printf("}");
      }
    }
    printf("]}");
  }
  printf("]}\n");
  return 0;
}
