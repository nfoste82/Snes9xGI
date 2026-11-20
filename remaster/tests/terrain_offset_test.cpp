#include "../runtime/lighting_field_cache.h"
#include "../runtime/placement_offset_editor.h"
#include "../runtime/profile_merge.h"
#include <iostream>
#include <cstdlib>
static void check(bool v,const char*m){if(!v){std::cerr<<m<<'\n';std::exit(1);}}
static bool near(float a,float b){return std::abs(a-b)<1e-5f;}
int main(){
	RemasterProfile p;p.schemaVersion=38;p.gameTitle="Terrain test";p.romSha256="66871d66be19ad2c34c927d6b14cd8eb6fc3181965b6e517cb361f7316009cfb";p.authoredOverworldOffsets=true;
	RemasterContextLocation at;at.area=320;at.background=1;at.x=37;at.y=213;
	RemasterTerrainOffset t;t.hasHeight=true;for(unsigned k=0;k<64;k++)t.height[k]=float(k%8)*.25f;t.normal={{-.6f,0,.8f}};
	t.hasPixelNormals=true;for(unsigned k=0;k<64;k++){t.pixelNormals[k*3]=k%8<4?-.6f:0;t.pixelNormals[k*3+1]=0;t.pixelNormals[k*3+2]=k%8<4?.8f:1;}
	p.overworldTerrain[at]=t;p.overworldOffsets[at]=10;
	auto second=at;second.y++;p.overworldTerrain[second]=t;
	auto zero=at;zero.x++;p.overworldOffsets[zero]=0;p.overworldTerrain[zero]={};
	std::string text,again;std::vector<RemasterProfileDiagnostic>d;RemasterProfile q;
	if(!S9xRemasterSerializeProfile(p,text,d)){for(auto &e:d)std::cerr<<e.line<<": "<<e.message<<'\n';check(false,"schema38 serialize");}if(!S9xRemasterParseProfile(text,q,d)){for(auto &e:d)std::cerr<<e.line<<": "<<e.message<<'\n';check(false,"schema38 parse");}
	check(q.overworldTerrain.size()==2&&q.overworldOffsets.size()==2&&q.overworldOffsets.at(zero)==0,"identity terrain omitted, explicit scalar zero preserved");
	check(S9xRemasterSerializeProfile(q,again,d)&&text==again,"stable roundtrip");
	for(auto replacement:{std::string("[0, 0, 0]"),std::string("[0, 0, -1]"),std::string("[nan, 0, 1]")}){
		auto bad=text;auto pos=bad.find("normal_offsets = ");auto end=bad.find('\n',pos);bad.replace(pos,end-pos,"normal_offset = "+replacement);RemasterProfile invalid;check(!S9xRemasterParseProfile(bad,invalid,d),"invalid support normal rejected");
	}
	float x=0,y=0,z=1;S9xRemasterRotateTerrainNormal(t.normal,x,y,z);check(near(x,-.6)&&near(y,0)&&near(z,.8),"+Z maps to support");
	x=.3;y=.4;z=std::sqrt(.75f);float oldz=z;S9xRemasterRotateTerrainNormal(t.normal,x,y,z);check(near(x*x+y*y+z*z,1)&&near(x*t.normal[0]+z*t.normal[2],oldz),"detail angular relationship preserved");
	RemasterFrame f;f.width=f.height=8;f.area.id=320;f.profileRomSha256=p.romSha256;f.originalRgb555.resize(64);f.mainPixels.resize(64);f.subPixels.resize(64);f.tileInstances.resize(1);f.contextLocations[1]=at;
	auto &i=f.tileInstances[0];i.source=RemasterSourceType::Background;i.sourceIndex=1;i.savedPlacementOffsetStatus=1;i.tileId.hashVersion=1;i.tileId.bitDepth=4;i.tileId.hash=123;
	RemasterAssetMetadata m;m.tileId=i.tileId;m.hasHeight=m.hasNormals=true;m.height.fill(2);for(unsigned k=0;k<64;k++){m.normalXyz[k*3]=128;m.normalXyz[k*3+1]=128;m.normalXyz[k*3+2]=255;}p.assets[m.tileId]=m;
	for(unsigned flip=0;flip<4;flip++){
		i.hFlip=flip&1;i.vFlip=flip&2;for(unsigned k=0;k<64;k++){auto &px=f.mainPixels[k];px.owner=0x02010000;px.instanceId=1;px.tilePixel=S9xRemasterTerrainPixel(k,i.hFlip,i.vFlip);}
		S9xRemasterApplyProfileToFrame(p,f);S9xRemasterPreviewPlacementOffsets(p,f);
		RemasterLightingFieldCache cache;cache.update(f,false);
		for(unsigned k=0;k<64;k++){check(near(cache.height[k*2],(12+t.height[k])/255.f),"world sample independent of artwork flips");if(k%8<4)check(cache.surface[k*4+1]<-.59&&cache.surface[k*4+3]>.79,"per-pixel rotated runtime normal");else check(std::abs(cache.surface[k*4+1])<.005&&cache.surface[k*4+3]>.99,"per-pixel identity runtime normal");}
		p.overworldTerrain[at].height[0]+=.125f;S9xRemasterPreviewPlacementOffsets(p,f);cache.update(f,false);RemasterLightingFieldCache cold;cold.update(f,false,true);check(cache.height==cold.height&&cache.surface==cold.surface,"incremental terrain invalidation");p.overworldTerrain[at]=t;
	}
	std::vector<uint8_t> bytes;check(S9xSerializeRemasterFrame(f,bytes),"frame42 write");RemasterFrame loaded;check(S9xDeserializeRemasterFrame(bytes,loaded),"frame42 read");check(loaded.tileInstances[0].terrainOffset==f.tileInstances[0].terrainOffset,"recorded terrain preserved");
	std::string editError;
	check(S9xRemasterEditPlacementPixelHeight(p,f,{1},9,40.125f,false,editError),"fractional pixel set");
	check(near(p.overworldTerrain[at].height[9],30.125f)&&p.overworldOffsets[at]==10,"pixel set subtracts scalar base");
	check(p.overworldTerrain[at].pixelNormals==t.pixelNormals&&p.assets[m.tileId].height==m.height,"pixel editing preserves normals/local relief");
	check(S9xRemasterEditPlacementPixelHeight(p,f,{1},9,-1,true,editError)&&near(p.overworldTerrain[at].height[9],29.125f),"pixel relative edit");
	check(!S9xRemasterEditPlacementPixelHeight(p,f,{1},64,0,false,editError),"pixel bounds");
	p.overworldTerrain[at]=t;
	auto copied=S9xRemasterCopyPlacementLayer(p,at);
	RemasterPlacementLayer defaults;
	check(S9xRemasterPastePlacementLayer(p,f,{1},defaults,true,editError),"clear normal channel");
	check(!p.overworldTerrain.at(at).hasPixelNormals&&p.overworldTerrain.at(at).normal==std::array<float,3>{{0,0,1}}&&p.overworldTerrain.at(at).height==t.height&&p.overworldOffsets.at(at)==10,"normal clear preserves both heights");
	check(S9xRemasterPastePlacementLayer(p,f,{1},copied,true,editError)&&p.overworldTerrain.at(at)==t,"paste pixel normals exact");
	check(S9xRemasterPastePlacementLayer(p,f,{1},defaults,false,editError),"clear whole height layer");
	check(!p.overworldOffsets.count(at)&&!p.overworldTerrain.at(at).hasHeight&&p.overworldTerrain.at(at).pixelNormals==t.pixelNormals,"height clear preserves normals");
	check(S9xRemasterPastePlacementLayer(p,f,{1},copied,false,editError)&&p.overworldOffsets.at(at)==10&&p.overworldTerrain.at(at)==t,"paste scalar and pixel height exact");
	check(!S9xRemasterPastePlacementLayer(p,f,{1,999},defaults,false,editError)&&p.overworldOffsets.at(at)==10,"unsupported batch atomic");
	check(S9xRemasterPastePlacementLayer(p,f,{1},defaults,true,editError)&&S9xRemasterPastePlacementLayer(p,f,{1},defaults,false,editError)&&!p.overworldTerrain.count(at)&&!p.overworldOffsets.count(at),"identity paste removes sparse records");
	p.overworldTerrain[at]=t;p.overworldOffsets[at]=10;
	p.authoredOverworldOffsets=false;S9xRemasterPreviewPlacementOffsets(p,f);check(f.tileInstances[0].terrainOffset.identity()&&f.tileInstances[0].heightOffset==0,"disabled clears deformation and scalar");
	p.authoredOverworldOffsets=true;f.contextLocations[1].area=321;f.area.id=321;S9xRemasterPreviewPlacementOffsets(p,f);check(f.tileInstances[0].terrainOffset.identity(),"Dark independent identity");
	auto records=RemasterProfileMerge::Split(text);check(std::any_of(records.begin(),records.end(),[](const auto&e){return e.first.compare(0,2,"C:")==0;}),"coordinate merge record");
	std::cout<<"PASS sparse terrain schema, normals/detail rotation, flips, runtime field, cache, frame evidence and isolation\n";
}
