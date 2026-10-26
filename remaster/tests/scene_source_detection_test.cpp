// Detection-only regression for legitimate subscreen receivers absent from scene.
// This pins current failing production preconditions; it does not repair them.
#include "../runtime/lighting_field_cache.h"
#include <cassert>
#include <iostream>
int main() {
 RemasterFrame f;f.width=f.height=1;
 f.profileRomSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";
 const uint32_t backdrop=uint32_t(RemasterSourceType::Backdrop)<<24;
 const uint32_t bg1=uint32_t(RemasterSourceType::Background)<<24;
 f.mainPixels={{backdrop,0,0}};f.scenePixels=f.mainPixels;f.subPixels={{bg1,1,0}};
 f.originalRgb555={0};f.sceneRgb555={0};
 RemasterFrameTileInstance i;i.source=RemasterSourceType::Background;i.sourceIndex=0;
 i.tileId={123,1,4};i.hasPaletteArtwork=true;i.paletteRgb555.fill(123);
 f.tileInstances.push_back(i);
 RemasterFrameAssetMetadata m;m.tileId=i.tileId;m.hasHeight=true;m.height.fill(0);m.tileRole=RemasterTileRole::Floor;
 f.assetMetadata.push_back(m);
 RemasterLightingFieldCache fields;
 // BG1 has an effective authored height. Missing normals alone prevent restore.
 assert(S9xRemasterRestoreSceneSubscreenWalls(f)==0);
 fields.update(f,true,true);assert(fields.height[1]==0);
 fields.update(f,false,true);assert(fields.height[1]==1); // real sub fallback
 // An authored physical obstacle still fails role admission, despite normals.
 f.assetMetadata[0].hasNormals=true;f.assetMetadata[0].tileRole=RemasterTileRole::Obstacle;
 assert(S9xRemasterRestoreSceneSubscreenWalls(f)==0);
 fields.update(f,true,true);assert(fields.height[1]==0);
 // Accepted role restores exact ownership/palette and height, not a guessed fill.
 f.assetMetadata[0].tileRole=RemasterTileRole::Floor;
 assert(S9xRemasterRestoreSceneSubscreenWalls(f)==1);
 assert(f.scenePixels[0].instanceId==1 && f.sceneRgb555[0]==123);
 fields.update(f,true,true);assert(fields.height[1]==1);
 // Opaque main ownership must never be replaced by the hidden subscreen.
 f.scenePixels[0]={backdrop,0,0};f.mainPixels[0]={bg1,1,0};
 assert(S9xRemasterRestoreSceneSubscreenWalls(f)==0);
 std::cout<<"PASS detection: effective source versus restore admission versus height field\n";
}
