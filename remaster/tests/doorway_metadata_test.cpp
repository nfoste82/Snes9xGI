#include "../runtime/frame.h"
#include "../runtime/layer_editor.h"
#include <cassert>
#include <iostream>

int main() {
 static_assert(static_cast<unsigned>(RemasterTileRole::Door)==8,"legacy door ordinal");
 static_assert(static_cast<unsigned>(RemasterTileRole::Doorway)==10,"append doorway");
 RemasterProfile p;p.schemaVersion=34;p.gameTitle="Doorway";p.romSha256=std::string(64,'a');
 RemasterTileContentId id{123,1,4};auto &a=p.assets[id];a.tileId=id;a.tileRole=RemasterTileRole::Doorway;
 a.hasHeight=a.hasNormals=a.hasDoorframeMask=true;a.height.fill(-17);a.normalXyz.fill(128);a.doorframeMask=0x123;
 std::string text;std::vector<RemasterProfileDiagnostic> errors;RemasterProfile q;
 assert(S9xRemasterSerializeProfile(p,text,errors));assert(text.find("doorway_support = false")!=std::string::npos);
 assert(text.find("doorway_support_direction")==std::string::npos);
 for(unsigned d=1;d<=4;d++) {
  a.doorwaySupport=true;a.doorwaySupportDirection=static_cast<RemasterSupportDirection>(d);
  assert(S9xRemasterSerializeProfile(p,text,errors));assert(S9xRemasterParseProfile(text,q,errors));
  assert(q.assets.at(id).doorwaySupport && q.assets.at(id).doorwaySupportDirection==a.doorwaySupportDirection);
  assert(q.assets.at(id).height==a.height && q.assets.at(id).normalXyz==a.normalXyz && q.assets.at(id).doorframeMask==a.doorframeMask);
 }
 auto invalid=text;invalid.erase(invalid.find("doorway_support_direction"));assert(!S9xRemasterParseProfile(invalid,q,errors));
 a.doorwaySupportDirection=RemasterSupportDirection::Unknown;assert(!S9xRemasterSerializeProfile(p,text,errors));
 a.doorwaySupport=false;a.hasDoorframeMask=false;a.tileRole=RemasterTileRole::Door;
 assert(S9xRemasterSerializeProfile(p,text,errors));assert(S9xRemasterParseProfile(text,q,errors));assert(q.assets.at(id).tileRole==RemasterTileRole::Door);
 text.replace(text.find("schema_version = 34"),19,"schema_version = 33");
 assert(S9xRemasterParseProfile(text,q,errors));assert(q.assets.at(id).tileRole==RemasterTileRole::Doorway);
 RemasterFrame frame;frame.width=frame.height=1;frame.originalRgb555.resize(1);frame.mainPixels.resize(1);frame.subPixels.resize(1);
 RemasterFrameAsset asset;asset.tileId=id;frame.assets.push_back(asset);
 for(auto role:{RemasterTileRole::Doorway,RemasterTileRole::Door}) {
  a.tileRole=role;a.doorwaySupport=role==RemasterTileRole::Doorway;
  a.doorwaySupportDirection=a.doorwaySupport?RemasterSupportDirection::East:RemasterSupportDirection::Unknown;
  S9xRemasterApplyProfileToFrame(p,frame);std::vector<uint8_t> bytes;RemasterFrame decoded;
  assert(S9xSerializeRemasterFrame(frame,bytes));assert(S9xDeserializeRemasterFrame(bytes,decoded));
  assert(decoded.schemaVersion==REMASTER_FRAME_SCHEMA_VERSION && decoded.assetMetadata.at(0).tileRole==role);
  assert(decoded.assetMetadata.at(0).doorwaySupport==a.doorwaySupport && decoded.assetMetadata.at(0).doorwaySupportDirection==a.doorwaySupportDirection);
 }
 for(auto d:{RemasterSupportDirection::North,RemasterSupportDirection::South,RemasterSupportDirection::East,RemasterSupportDirection::West})
  for(bool h:{false,true})for(bool v:{false,true})assert(S9xRemasterTransformSupportDirection(S9xRemasterTransformSupportDirection(d,h,v),h,v)==d);
 a.tileRole=RemasterTileRole::Doorway;a.doorwaySupport=true;a.doorwaySupportDirection=RemasterSupportDirection::North;
 assert(S9xRemasterRotateMetadataLayer(a,RemasterEditorMetadata,true) && a.doorwaySupportDirection==RemasterSupportDirection::East);
 assert(S9xRemasterFlipMetadataLayer(a,RemasterEditorMetadata,true) && a.doorwaySupportDirection==RemasterSupportDirection::West);
 assert(S9xRemasterRotateMetadataLayer(a,RemasterEditorMetadata,false) && a.doorwaySupportDirection==RemasterSupportDirection::South);
 std::cout<<"doorway metadata migration / support strict parsing roundtrip / actual Door / capture v39 / transforms PASS\n";
}
