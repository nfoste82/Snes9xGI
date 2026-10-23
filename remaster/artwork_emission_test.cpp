#include "lighting_field_cache.h"
#include "layer_editor.h"
#include <cassert>
#include <iostream>

int main() {
  RemasterProfile profile;
  profile.romSha256=std::string(64,'a');
  profile.gameTitle="Emission regression";
  RemasterTileContentId id{123,1,4};
  auto &a=profile.assets[id]; a.tileId=id; a.hasEmission=true; a.emissionColorMask=0;
  for(unsigned p=0;p<64;p++) a.emissionRgba[p*4+3]=p;
  std::string text; std::vector<RemasterProfileDiagnostic> diagnostics;
  if (!S9xRemasterSerializeProfile(profile,text,diagnostics)) {
    for(const auto &d:diagnostics) std::cerr << d.message << '\n'; return 1;
  }
  assert(text.find("emission_intensity")!=std::string::npos);
  RemasterProfile parsed;
  assert(S9xRemasterParseProfile(text,parsed,diagnostics));
  assert(parsed.assets.at(id).emissionColorMask==0);
  assert(parsed.assets.at(id).emissionRgba==a.emissionRgba);
  a.emissionColorMask=1; a.emissionRgba[0]=17;
  assert(S9xRemasterSerializeProfile(profile,text,diagnostics));
  assert(S9xRemasterParseProfile(text,parsed,diagnostics));
  assert(parsed.assets.at(id).emissionColorMask==1);
  RemasterFrame frame; frame.width=2;frame.height=1;
  frame.originalRgb555={0,0}; frame.mainPixels.resize(2);frame.subPixels.resize(2);
  frame.tileInstances.resize(2); frame.assetMetadata.resize(1);
  auto &m=frame.assetMetadata[0]; m.tileId=id;m.hasEmission=true;m.emissionColorMask=0;
  m.emissionRgba[3]=25;
  for(unsigned i=0;i<2;i++) {
    frame.mainPixels[i].instanceId=i+1;
    frame.mainPixels[i].tilePixel=0;
    frame.tileInstances[i].tileId=id;frame.tileInstances[i].hasPaletteArtwork=true;
    frame.tileInstances[i].paletteRgb555[0]=i ? 31 : 31<<10;
  }
  RemasterLightingFieldCache cache; cache.update(frame,false);
  assert(cache.emission[0]==255 && cache.emission[2]==0 && cache.emission[3]==25);
  assert(cache.emission[4]==0 && cache.emission[6]==255 && cache.emission[7]==25);
  frame.tileInstances[0].paletteRgb555[0]=31<<5;
  cache.update(frame,false);
  assert(cache.emission[0]==0 && cache.emission[1]==255);
  m.emissionColorMask=1;m.emissionRgba[0]=17;m.emissionRgba[1]=29;m.emissionRgba[2]=31;
  cache.update(frame,false);
  assert(cache.emission[0]==17 && cache.emission[4]==17);
  std::vector<uint8_t> bytes; assert(S9xSerializeRemasterFrame(frame,bytes));
  RemasterFrame decoded; assert(S9xDeserializeRemasterFrame(bytes,decoded));
  assert(decoded.assetMetadata[0].emissionColorMask==1);
  // Legacy frame metadata has no mask and retains explicitly authored RGB.
  // Locate the mask just after this fixture's metadata block via a second
  // serialization with a distinctive mask, rather than fixed byte offsets.
  const uint64_t marker=UINT64_C(0x0123456789abcdef);
  m.emissionColorMask=marker;
  assert(S9xSerializeRemasterFrame(frame,bytes));
  std::vector<uint8_t> needle; RemasterFrameSerialization::U64(needle,marker);
  auto at=std::search(bytes.begin(),bytes.end(),needle.begin(),needle.end());
  assert(at!=bytes.end()); bytes.erase(at,at+8);bytes[8]=37;
  assert(S9xDeserializeRemasterFrame(bytes,decoded));
  assert(decoded.assetMetadata[0].emissionColorMask==UINT64_MAX);
  a.emissionColorMask=1;
  S9xRemasterFlipMetadataLayer(a,RemasterEditorEmission,true);
  assert(a.emissionColorMask==128);
  std::cout << "Artwork palette emission, overrides, intensity/profile/frame round trips passed\n";
}
