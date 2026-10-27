// Frozen startup frame.h selection policy (e9506107), deliberately independent
// of the refactored shared selector. Keep as an oracle, not a production helper.
inline RemasterVisibleChannelSelection heightSelectionReference(const RemasterFrame &frame,
 size_t offset,bool useScene,const std::vector<const RemasterFrameAssetMetadata *> &metadata) {
 RemasterVisibleChannelSelection result;
 auto valid=[&](const RemasterFramePixel &p){return p.instanceId&&p.instanceId<=frame.tileInstances.size()&&p.tilePixel<64;};
 auto worldBg=[&](const RemasterFramePixel &p){return valid(p)&&frame.tileInstances[p.instanceId-1].source==RemasterSourceType::Background&&
  frame.tileInstances[p.instanceId-1].sourceIndex<2&&static_cast<RemasterSourceType>(p.owner>>24)==RemasterSourceType::Background&&
  ((p.owner>>16)&255)==frame.tileInstances[p.instanceId-1].sourceIndex;};
 if(offset>=frame.mainPixels.size())return result;
 const auto &main=frame.mainPixels[offset];const RemasterFramePixel empty;
 const auto &sub=offset<frame.subPixels.size()?frame.subPixels[offset]:empty;
 const auto &scene=useScene&&offset<frame.scenePixels.size()?frame.scenePixels[offset]:main;
 const bool hud=frame.profileRomSha256=="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb"&&
  static_cast<RemasterSourceType>(main.owner>>24)==RemasterSourceType::Background&&((main.owner>>16)&255)==2;
 result.visible=valid(main)&&!hud?main:valid(sub)&&(!hud||useScene)?sub:scene;
 if(hud&&!useScene)result.visible=main;
 if(hud&&useScene&&offset<frame.observedBgPixels[0].size()&&offset<frame.observedBgPixels[1].size()&&
  offset<frame.observedBgDepth[0].size()&&offset<frame.observedBgDepth[1].size()) {
  const auto &a=frame.observedBgPixels[0][offset],&b=frame.observedBgPixels[1][offset];
  if(valid(a)||valid(b))result.visible=valid(a)&&(!valid(b)||frame.observedBgDepth[0][offset]>=frame.observedBgDepth[1][offset])?a:b;
 }
 if(useScene&&valid(result.visible)&&valid(scene)&&result.visible.owner==scene.owner&&result.visible.tilePixel==scene.tilePixel) {
  const auto &a=frame.tileInstances[result.visible.instanceId-1],&b=frame.tileInstances[scene.instanceId-1];
  if(a.tileId==b.tileId&&a.source==b.source&&a.sourceIndex==b.sourceIndex&&a.hFlip==b.hFlip&&a.vFlip==b.vFlip)result.visible=scene;
 }
 const auto &preferred=result.visible;RemasterFramePixel other;
 if(worldBg(preferred)) {
  for(const auto *candidate:{&main,&sub,&scene})if(worldBg(*candidate)&&frame.tileInstances[candidate->instanceId-1].sourceIndex!=
   frame.tileInstances[preferred.instanceId-1].sourceIndex){other=*candidate;break;}
  if(!other.instanceId)for(const auto &buffer:frame.observedBgPixels)if(offset<buffer.size()) {
   const auto &p=buffer[offset];if(worldBg(p)&&frame.tileInstances[p.instanceId-1].sourceIndex!=frame.tileInstances[preferred.instanceId-1].sourceIndex){other=p;break;}
  }
 }
 if(useScene&&valid(other)&&valid(scene)&&other.owner==scene.owner&&other.tilePixel==scene.tilePixel) {
  const auto &a=frame.tileInstances[other.instanceId-1],&b=frame.tileInstances[scene.instanceId-1];
  if(a.tileId==b.tileId&&a.source==b.source&&a.sourceIndex==b.sourceIndex&&a.hFlip==b.hFlip&&a.vFlip==b.vFlip)other=scene;
 }
 auto present=[&](const RemasterFramePixel &p){return valid(p)&&p.instanceId<=metadata.size()&&metadata[p.instanceId-1]&&metadata[p.instanceId-1]->hasHeight;};
 result.donors[0]=present(preferred)?preferred:present(other)?other:RemasterFramePixel{};
 return result;
}
