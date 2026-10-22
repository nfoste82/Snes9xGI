// Link against the local MIT reference engine, like alttp_build_route_adapter.py.
// Complete-room differential oracle; no emulation RAM modification by our builder.
#define ALTTP_ROOM_PREVIEW_MAIN room_preview_main
#include "alttp_room_preview_adapter.c"
#undef main
extern int RemasterCheckTransitionAttributes(const unsigned char *,unsigned char *);
int main(int argc,char **argv) {
  int status=room_preview_main(argc,argv);if(status)return status;
  unsigned char result[8192],before[0x20000];
  int checked=0;
  for(int room=0;room<320;room++) {
    dungeon_room_index=room;submodule_index=0;
    Dungeon_LoadRoom();
    memcpy(before,g_ram,sizeof(before));
    if(!RemasterCheckTransitionAttributes(g_ram,result)){fprintf(stderr,"builder rejected room=%03x\n",room);return 1;}
    if(memcmp(before,g_ram,sizeof(before)))Die("builder modified emulated RAM");
    Dungeon_LoadAttributeTable();
    int differences=0;
    for(int i=0;i<8192;i++)if(result[i]!=g_ram[0x12000+i]) {
      if(differences<8)fprintf(stderr,"room=%03x cell=%d expected=%02x actual=%02x\n",room,i,g_ram[0x12000+i],result[i]);
      differences++;
    }
    if(differences)return 1;
    checked++;
  }
  printf("PASS original USA builder differential rooms=%d cells=%d immutable input\n",checked,checked*8192);
  return 0;
}
