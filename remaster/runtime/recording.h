// Completed-frame diagnostic recordings. No emulator globals or borrowed inputs.
#ifndef _REMASTER_RECORDING_H_
#define _REMASTER_RECORDING_H_
#include "frame.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <stdexcept>

namespace RemasterRecording {
using Bytes = std::vector<uint8_t>;
using namespace RemasterFrameSerialization;
constexpr uint64_t MaxFile = UINT64_C(2)*1024*1024*1024;
constexpr size_t MaxBlob = 32*1024*1024, MaxDictionary = 64*1024*1024;
constexpr uint32_t MaxFrames = 36000, MaxBlocks = 262144;
constexpr size_t Chunk = 16384;
constexpr size_t MaxQueuedBytes = 128*1024*1024;

// Packet is copied on the producer, before its pointers/context can change.
struct Packet {
	RemasterFrame frame;
	Bytes evidence;
	std::shared_ptr<const RemasterProfile> profile;
	uint64_t sequence = 0, session = 0, epoch = 0;
	uint32_t emulatedFrame = 0;
	bool revisionOnly = false;
	RemasterAreaContext revisionArea;
};
inline size_t Weight(const Packet &p) {
	const auto &f=p.frame;size_t n=sizeof(Packet)+p.evidence.size();
	n+=f.originalRgb555.size()*2+f.sceneRgb555.size()*2;
	for(auto *v:{&f.mainPixels,&f.subPixels,&f.scenePixels})n+=v->size()*sizeof(RemasterFramePixel);
	for(auto &v:f.observedBgPixels)n+=v.size()*sizeof(RemasterFramePixel);
	for(auto &v:f.observedBgDepth)n+=v.size();
	for(auto *v:{&f.mainRoomDiagnostics,&f.subRoomDiagnostics,&f.sceneRoomDiagnostics})n+=v->size()*sizeof(RemasterRoomPixelDiagnostic);
	n+=f.tileInstances.size()*sizeof(RemasterFrameTileInstance)+f.assetMetadata.size()*sizeof(RemasterFrameAssetMetadata);
	n+=f.hiddenEmitters.size()*sizeof(RemasterFrameHiddenEmitter)+f.contextGeometry.size()*sizeof(RemasterTileContext);
	n+=f.assets.size()*sizeof(RemasterFrameAsset)+(f.artworkColors.size()+f.paletteArtworkColors.size())*sizeof(RemasterFrameArtworkColors);
	return n;
}
inline bool WithinBounds(const Packet &p) {
	const auto &f=p.frame;
	return f.width && f.height && f.width<=512 && f.height<=478 &&
		f.tileInstances.size()<=65536 && f.assetMetadata.size()<=16384 &&
		f.contextGeometry.size()<=f.tileInstances.size() && Weight(p)<=MaxBlob;
}
struct Entry {
	uint64_t offset = 0, sequence = 0, profile = 0, session = 0, epoch = 0;
	uint32_t emulatedFrame = 0;
};
inline uint64_t Hash(const Bytes &b) {
	uint64_t h=UINT64_C(14695981039346656037);
	for(auto v:b)h=(h^v)*UINT64_C(1099511628211);
	return h;
}
// PackBits-style runs; bounded expansion and no external codec dependency.
inline Bytes Pack(const Bytes &in) {
	Bytes out;
	for(size_t i=0;i<in.size();) {
		size_t n=1;while(n<128 && i+n<in.size() && in[i+n]==in[i])++n;
		if(n>=3){out.push_back(uint8_t(127+n));out.push_back(in[i]);i+=n;continue;}
		const size_t start=i;i+=n;
		while(i<in.size() && i-start<128) {
			if(i+2<in.size() && in[i]==in[i+1] && in[i]==in[i+2])break;
			++i;
		}
		out.push_back(uint8_t(i-start-1));out.insert(out.end(),in.begin()+start,in.begin()+i);
	}
	return out;
}
inline bool Unpack(const Bytes &in,size_t size,Bytes &out) {
	if(size>Chunk)return false;
	out.clear();out.reserve(size);
	for(size_t i=0;i<in.size();) {
		unsigned c=in[i++],n=c<128?c+1:c-127;
		if(n>size-out.size())return false;
		if(c<128){if(n>in.size()-i)return false;out.insert(out.end(),in.begin()+i,in.begin()+i+n);i+=n;}
		else {if(i==in.size())return false;out.insert(out.end(),n,in[i++]);}
	}
	return out.size()==size;
}
inline void Blob(Bytes &out,const Bytes &b) { U32(out,uint32_t(b.size()));out.insert(out.end(),b.begin(),b.end()); }
inline bool Take(Reader &r,Bytes &b,size_t limit=MaxBlob) {
	uint32_t n;if(!r.ReadU32(n)||n>limit||n>r.bytes.size()-r.offset)return false;
	b.assign(r.bytes.begin()+r.offset,r.bytes.begin()+r.offset+n);r.offset+=n;return true;
}
inline bool DecodeFrame(const Bytes &b,RemasterFrame &f) {
	// Current-version embedded packets only. Check container counts BEFORE the
	// legacy single-capture decoder allocates potentially amplified containers.
	if(b.size()<24 || std::memcmp(b.data(),"S9XRMF\0\1",8))return false;
	Reader r(b);r.offset=8;uint32_t version,w,h;std::string rom;
	if(!r.ReadU32(version)||version!=REMASTER_FRAME_SCHEMA_VERSION||!r.ReadU32(w)||!r.ReadU32(h)||!w||!h||w>512||h>478||!r.ReadString(rom)||rom.size()>64||b.size()-r.offset<34+28)return false;
	r.offset+=34;uint32_t count[7];for(auto &v:count)if(!r.ReadU32(v))return false;
	if(count[0]>16384||count[1]>16384||count[2]>16384||count[3]>16384||count[4]>4096||count[5]>65536||count[6]>65536)return false;
	return S9xDeserializeRemasterFrame(b,f);
}
inline RemasterFrame TableFrame(const RemasterFrame &f) {
	RemasterFrame t;t.width=t.height=1;t.originalRgb555.resize(1);t.mainPixels.resize(1);t.subPixels.resize(1);
	t.profileRomSha256=f.profileRomSha256;return t;
}

// Preserve runtime-only plane/ownership diagnostics explicitly, not native structs.
inline void Diagnostic(Bytes &b,const RemasterRoomDiagnostic &d) {
	for(int32_t v:{d.floorId,d.floorBase,d.lowerFloorId,d.lowerFloorBase,int32_t(d.lowerFloorDistance)})U32(b,uint32_t(v));
	U16(b,d.room);U16(b,d.cell);
	for(uint8_t v:{d.plane,d.background,d.uphillDirections,uint8_t(d.wall),uint8_t(d.heightConflict),uint8_t(d.axisUnknown),uint8_t(d.planeObserved),uint8_t(d.semanticFloorSupport),uint8_t(d.wallSource)})U8(b,v);
	U16(b,d.wallTemplateOffset);
}
inline bool Diagnostic(Reader &r,RemasterRoomDiagnostic &d) {
	uint32_t v[5];for(auto &x:v)if(!r.ReadU32(x))return false;
	d.floorId=int32_t(v[0]);d.floorBase=int32_t(v[1]);d.lowerFloorId=int32_t(v[2]);d.lowerFloorBase=int32_t(v[3]);d.lowerFloorDistance=int16_t(v[4]);
	uint8_t flags[5],source;
	if(!r.ReadU16(d.room)||!r.ReadU16(d.cell)||!r.ReadU8(d.plane)||!r.ReadU8(d.background)||!r.ReadU8(d.uphillDirections))return false;
	for(auto &x:flags)if(!r.ReadU8(x)||x>1)return false;
	d.wall=flags[0];d.heightConflict=flags[1];d.axisUnknown=flags[2];d.planeObserved=flags[3];d.semanticFloorSupport=flags[4];
	if(!r.ReadU8(source)||source>2)return false;
	d.wallSource=RemasterRoomDiagnostic::WallSource(source);
	return r.ReadU16(d.wallTemplateOffset);
}

// RTE1: fixed ordered scalar context, optional raw input planes, and the actual
// applied/retained map outputs. No writer-time RAM/session access or actor gate.
inline Bytes Evidence(const RemasterDungeonFloorContext *c,
	const RemasterDungeonHeightMap *applied,const RemasterDungeonHeightMap *bg1,
	const RemasterDungeonHeightMap *previous,const RemasterDungeonHeightMap *previousBg1,
	bool publishable,uint64_t key,uint64_t outgoingSession,uint16_t outgoingRoom,
	const std::array<int,4> *scrollX,const std::array<int,4> *scrollY) {
	Bytes b={'R','T','E','1'};U8(b,c!=nullptr);U8(b,publishable);U64(b,key);U64(b,outgoingSession);U16(b,outgoingRoom);
	for(const auto *scroll:{scrollX,scrollY}){U8(b,scroll!=nullptr);if(scroll)for(auto v:*scroll)U32(b,uint32_t(v));}
	if(c) {
		U64(b,c->romIdentity);
		for(uint16_t v:{c->roomIndex,c->previousRoomIndex,c->backgroundScrollX,c->backgroundScrollY,c->linkX,c->linkY,uint16_t(c->linkZ),c->graphicsBase,c->bg1GraphicsBase,c->destinationOriginX,c->destinationOriginY,c->actorScrollX,c->actorScrollY,c->retainedOutgoingRoom})U16(b,v);
		for(uint8_t v:{uint8_t(c->verifiedAlttpRom),uint8_t(c->indoors),c->module,c->savedModule,c->submodule,c->constructionPhase,c->collisionMode,c->roomTransitionFlags,c->attributeConstructionState,c->straightStairPlane,c->scrollDirection,c->linkPlane,uint8_t(c->hasDestinationOrigin),uint8_t(c->groundedEvidence),uint8_t(c->hasCompletedTransitionAttributes)})U8(b,v);
		for(auto v:c->ppuMapBase)U16(b,v);for(auto v:c->ppuMapSize)U8(b,v);
		const auto &e=c->entrySupport;U32(b,uint32_t(e.cell));U32(b,uint32_t(e.base));U16(b,e.predecessorRoom);U64(b,e.predecessorState);U64(b,e.transitionEvidence);U64(b,e.destinationState);
		U8(b,c->stairCount);for(size_t i=0;i<c->stairCount && i<c->stairs.size();i++){const auto &s=c->stairs[i];for(uint8_t v:{s.x,s.y,uint8_t(s.highIsNorth),uint8_t(s.changesPlane)})U8(b,v);}
		auto raw=[&](const uint8_t *p,size_t n){U32(b,p?uint32_t(n):0);if(p)b.insert(b.end(),p,p+n);};
		raw(c->constructedMaps,16384);raw(c->collisionAttributes,8192);raw(c->graphics,65536);raw(c->dungeonTemplateData,12876);raw(c->retainedOutgoingMaps,16384);
		U8(b,c->hasTerrainActors);if(c->hasTerrainActors){for(const auto &a:c->terrainActors){U16(b,a.x);U16(b,a.y);for(uint8_t v:{a.state,a.kind,a.z,a.plane})U8(b,v);}for(const auto &o:c->terrainOam){U16(b,o.x);for(uint8_t v:{o.y,o.name,o.flags,o.size})U8(b,v);}}
		U8(b,c->hasObstacleActors);if(c->hasObstacleActors)for(const auto &a:c->obstacleActors){U16(b,a.x);U16(b,a.y);for(uint8_t v:{a.type,a.state,a.z,a.plane,a.xSubpixel,a.xVelocity,a.yVelocity,uint8_t(a.hasMotion)})U8(b,v);}
	}
	for(const auto *m:{applied,bg1,previous,previousBg1}) {
		U8(b,m!=nullptr);if(!m)continue;U16(b,m->roomIndex);U8(b,m->hasPlaneOffsets);U8(b,m->hasRoomGeometry);
		for(auto v:m->offsets)U32(b,uint32_t(v));
		for(const auto *planes:{&m->planeOffsets,&m->occupiedFloorOwners,&m->structuralPlaneOffsets,&m->mixedFloorBases})for(const auto &plane:*planes)for(auto v:plane)U32(b,uint32_t(v));
		for(auto v:m->producerHashes)U64(b,v);for(auto v:m->producerFlips)U8(b,v);for(auto v:m->fallbackDirections)U8(b,v);
		for(const auto *flags:{&m->producerZeroFloor,&m->producerObstacle,&m->producerOwnedWall,&m->producerOwnedStair,&m->producerAuthoredObstacle,&m->producerGeneratedObstacle})for(auto v:*flags)U8(b,v);
		for(const auto *counts:{&m->requiredPlaneCells,&m->unknownPlaneCells,&m->solvedPlaneCells,&m->persistentSupportRequired,&m->persistentSupportMissing})for(auto v:*counts)U16(b,v);
		for(auto v:{m->structuralConflicts,m->solvedWallCells,m->unknownWallCells,m->componentCount,m->constraintCount,m->conflictingComponentCount,m->mixedFloorUnknown,m->mixedFloorConflicts})U16(b,v);
		U8(b,bool(m->roomDiagnostics));if(m->roomDiagnostics)for(const auto &d:m->roomDiagnostics->cells)Diagnostic(b,d);
	}
	return b;
}

class Writer {
	std::ofstream file;
	uint64_t written=8;
	uint32_t blocks=0, frames=0;
	size_t dictionaryBytes=0;
	std::map<uint64_t,std::vector<std::pair<uint32_t,Bytes>>> dictionary;
	bool Record(uint8_t type,const Bytes &b) {
		if(b.size()>MaxBlob || written+5+b.size()>MaxFile)return false;
		Bytes header;U8(header,type);U32(header,uint32_t(b.size()));
		file.write(reinterpret_cast<const char *>(header.data()),header.size());
		file.write(reinterpret_cast<const char *>(b.data()),b.size());written+=5+b.size();return bool(file);
	}
public:
	bool Open(const std::string &path) {
		// Reserve exclusively: neither another recorder nor an existing file is replaced.
		FILE *reserved=std::fopen(path.c_str(),"wbx");if(!reserved)return false;
		if(std::fclose(reserved)!=0)return false;
		file.open(path,std::ios::binary);file.write("S9XRMR\0\1",8);file.flush();return bool(file);
	}
	bool Reference(Bytes &out,const Bytes &b,unsigned intern=1) {
		if(b.size()>MaxBlob)return false;
		U32(out,uint32_t(b.size()));U32(out,uint32_t((b.size()+Chunk-1)/Chunk));
		for(size_t i=0;i<b.size();i+=Chunk) {
			Bytes chunk(b.begin()+i,b.begin()+std::min(b.size(),i+Chunk));const auto hash=Hash(chunk);
			auto found=dictionary.find(hash);uint32_t id=0;
			if(found!=dictionary.end())for(const auto &candidate:found->second)if(candidate.second==chunk){id=candidate.first;break;}
			if(!id && (!intern || (intern==2 && dictionaryBytes+chunk.size()>MaxDictionary/2))) {
				auto packed=Pack(chunk);U32(out,0);U32(out,uint32_t(chunk.size()));Blob(out,packed);continue;
			}
			if(!id) {
				if(blocks>=MaxBlocks || dictionaryBytes+chunk.size()>MaxDictionary)return false;
				id=++blocks;Bytes record;U32(record,id);U32(record,uint32_t(chunk.size()));U64(record,hash);
				auto packed=Pack(chunk);record.insert(record.end(),packed.begin(),packed.end());
				if(!Record(1,record))return false;
				dictionaryBytes+=chunk.size();dictionary[hash].emplace_back(id,std::move(chunk));
			}
			U32(out,id);
		}
		return true;
	}
	bool Append(const Packet &p) {
		if((!p.revisionOnly && frames>=MaxFrames) || !p.profile)return false;
		if(!p.revisionOnly && !WithinBounds(p))return false;
		Bytes record;U64(record,p.sequence);U32(record,p.emulatedFrame);U64(record,p.session);U64(record,p.epoch);
		std::string text;std::vector<RemasterProfileDiagnostic> diagnostics;
		// Serialized on worker, never UI/producer. Cached immutable profile pointers
		// are shared across frames; dictionary retains only changed chunks/revisions.
		if(p.profile!=lastProfile) {
			if(!S9xRemasterSerializeProfile(*p.profile,text,diagnostics,false))return false;
			profileText.assign(text.begin(),text.end());lastProfile=p.profile;
		}
		if(profileText.size()>MaxBlob)return false;
		if(p.revisionOnly) {record.clear();U64(record,Hash(profileText));U16(record,p.revisionArea.id);U8(record,p.revisionArea.romDark);bool ok=Reference(record,profileText) && Record(4,record);file.flush();return ok && bool(file);}
		U64(record,Hash(profileText));if(!Reference(record,profileText)||!Reference(record,p.evidence,2))return false;
		RemasterFrame dynamic=p.frame;
		// Individually intern authored identity/revision records. Never duplicate
		// authored arrays just because a different subset/order is visible next frame.
		auto table=[&](const auto &items,auto member) {
			U32(record,uint32_t(items.size()));
			for(const auto &item:items){auto f=TableFrame(p.frame);(f.*member).push_back(item);Bytes b;
				if(!S9xSerializeRemasterFrame(f,b)||!Reference(record,b))return false;}
			return true;
		};
		if(!table(p.frame.assets,&RemasterFrame::assets)||!table(p.frame.assetMetadata,&RemasterFrame::assetMetadata)||
			!table(p.frame.assetGroups,&RemasterFrame::assetGroups)||!table(p.frame.materials,&RemasterFrame::materials))return false;
		dynamic.assets.clear();dynamic.assetMetadata.clear();dynamic.assetGroups.clear();dynamic.materials.clear();
		Bytes b;if(!S9xSerializeRemasterFrame(dynamic,b)||!Reference(record,b,2))return false;
		// Context geometry is otherwise runtime-only; freeze the actual selected layers.
		U32(record,uint32_t(p.frame.contextGeometry.size()));
		for(const auto &entry:p.frame.contextGeometry) {
			U32(record,entry.first);auto f=TableFrame(p.frame);RemasterFrameAssetMetadata m;
			m.tileId=p.frame.tileInstances.at(entry.first-1).tileId;
			const auto &c=entry.second;m.height=c.height;m.hasHeight=c.hasHeight;m.heightSampling=c.heightSampling;
			m.normalXyz=c.normalXyz;m.hasNormals=c.hasNormals;m.normalIgnoreTileTransform=c.normalIgnoreTileTransform;
			m.directLightingOppositeFacing=c.directLightingOppositeFacing;f.assetMetadata.push_back(m);
			if(!S9xSerializeRemasterFrame(f,b)||!Reference(record,b))return false;
		}
		for(const auto *pixels:{&p.frame.mainRoomDiagnostics,&p.frame.subRoomDiagnostics,&p.frame.sceneRoomDiagnostics}) {
			b.clear();U32(b,uint32_t(pixels->size()));
			for(const auto &pixel:*pixels){U8(b,pixel.visiblePlane);for(const auto &d:pixel.planes)Diagnostic(b,d);}
			if(!Reference(record,b,2))return false;
		}
		if(!Record(2,record))return false;
		++frames;file.flush();return bool(file);
	}
	bool Finish() { Bytes b;U32(b,frames);bool ok=Record(3,b);file.flush();ok &= bool(file);file.close();return ok && bool(file); }
	uint64_t Size() const{return written;}
private:
	std::shared_ptr<const RemasterProfile> lastProfile;
	Bytes profileText;
};

// Lazy indexed reader: only one decoded frame, bounded records and dictionaries.
class ReaderFile {
	std::ifstream file;
	struct Block { uint64_t offset;uint32_t length,raw;uint64_t hash; };
	std::vector<Block> blocks;
	uint64_t fileSize=0;
	size_t expandedBytes=0;
	bool Load(uint64_t offset,uint32_t length,Bytes &b) {
		if(length>MaxBlob || offset>fileSize || length>fileSize-offset)return false;
		b.resize(length);file.clear();file.seekg(offset);file.read(reinterpret_cast<char *>(b.data()),length);return bool(file);
	}
	bool Reference(Reader &r,Bytes &b) {
		uint32_t size,count;if(!r.ReadU32(size)||size>MaxBlob||!r.ReadU32(count)||count!=(size+Chunk-1)/Chunk || count>(r.bytes.size()-r.offset)/4)return false;
		if(size>MaxQueuedBytes-expandedBytes)return false;expandedBytes+=size;
		b.clear();b.reserve(size);
		for(uint32_t i=0;i<count;i++) {
			uint32_t id;if(!r.ReadU32(id)||id>blocks.size())return false;
			if(!id) {
				uint32_t raw;Bytes packed,chunk;
				if(!r.ReadU32(raw)||!Take(r,packed,Chunk*2)||!Unpack(packed,raw,chunk)||chunk.size()!=std::min(size_t(size)-b.size(),Chunk))return false;
				b.insert(b.end(),chunk.begin(),chunk.end());continue;
			}
			const auto &block=blocks[id-1];Bytes packed,chunk;
			if(!Load(block.offset,block.length,packed)||!Unpack(packed,block.raw,chunk)||Hash(chunk)!=block.hash||chunk.size()!=std::min(size_t(size)-b.size(),Chunk))return false;
			b.insert(b.end(),chunk.begin(),chunk.end());
		}
		return b.size()==size;
	}
public:
	std::vector<Entry> entries;
	bool complete=false;
	std::string warning;
	uint32_t revisionEvents=0;
	bool Open(const std::string &path,std::string &error) try {
		file.open(path,std::ios::binary|std::ios::ate);if(!file){error="Cannot open recording";return false;}
		fileSize=uint64_t(file.tellg());if(fileSize<8||fileSize>MaxFile){error="Recording size exceeds bounds";return false;}
		Bytes b;if(!Load(0,8,b)||std::memcmp(b.data(),"S9XRMR\0\1",8)){error="Unsupported recording version";return false;}
		uint64_t offset=8;size_t dictionaryBytes=0;
		while(offset<fileSize) {
			if(fileSize-offset<5){warning="Truncated final record; recovered complete prefix";break;}
			if(!Load(offset,5,b))return false;Reader h(b);uint8_t type;uint32_t size;h.ReadU8(type);h.ReadU32(size);
			if(size>MaxBlob){error="Oversized recording record";return false;}
			if(size>fileSize-offset-5){warning="Truncated final record; recovered complete prefix";break;}
			if(!Load(offset+5,size,b))return false;Reader r(b);
			if(type==1) {
				uint32_t id,raw;uint64_t hash;
				if(!r.ReadU32(id)||id!=blocks.size()+1||id>MaxBlocks||!r.ReadU32(raw)||!raw||raw>Chunk||!r.ReadU64(hash)||dictionaryBytes+raw>MaxDictionary){error="Invalid recording dictionary";return false;}
				Bytes packed(b.begin()+r.offset,b.end()),unpacked;
				if(!Unpack(packed,raw,unpacked)||Hash(unpacked)!=hash){error="Corrupt recording dictionary";return false;}
				dictionaryBytes+=raw;blocks.push_back({offset+5+r.offset,uint32_t(b.size()-r.offset),raw,hash});
			} else if(type==2) {
				Entry e;e.offset=offset;
				if(entries.size()>=MaxFrames||!r.ReadU64(e.sequence)||!r.ReadU32(e.emulatedFrame)||!r.ReadU64(e.session)||!r.ReadU64(e.epoch)||!r.ReadU64(e.profile)||e.sequence!=entries.size()){error="Invalid recording frame index";return false;}
				entries.push_back(e);
			} else if(type==4) {
				expandedBytes=0;
				uint64_t hash;uint16_t area;uint8_t dark;Bytes text;
				if(++revisionEvents>MaxFrames || !r.ReadU64(hash)||!r.ReadU16(area)||(area>321 && area!=65535)||!r.ReadU8(dark)||dark>1||!Reference(r,text)||Hash(text)!=hash||r.offset!=b.size()){error="Invalid profile/settings revision event";return false;}
			} else if(type==3) {
				uint32_t count;if(!r.ReadU32(count)||count!=entries.size()||r.offset!=b.size()||offset+5+size!=fileSize){error="Invalid completion record";return false;}
				complete=true;
			} else {error="Unknown recording record";return false;}
			offset+=5+size;
		}
		if(!complete && warning.empty())warning="Unfinished recording; recovered complete prefix";
		return true;
	} catch(const std::exception &e){error=std::string("Recording index error: ")+e.what();return false;}
	bool Read(size_t index,RemasterFrame &frame,Bytes &evidence,std::string &profileText,std::string &error) try {
		expandedBytes=0;
		if(index>=entries.size()){error="Frame index out of range";return false;}
		Bytes header,b,blob;if(!Load(entries[index].offset,5,header))return false;Reader h(header);uint8_t type;uint32_t size;h.ReadU8(type);h.ReadU32(size);
		if(!Load(entries[index].offset+5,size,b))return false;Reader r(b);r.offset=36;
		if(!Reference(r,blob)||Hash(blob)!=entries[index].profile){error="Invalid profile revision";return false;}
		profileText.assign(blob.begin(),blob.end());if(!Reference(r,evidence))return false;
		RemasterFrame result;
		auto table=[&](auto member) {
			uint32_t count;if(!r.ReadU32(count)||count>16384)return false;
			for(uint32_t i=0;i<count;i++){RemasterFrame t;if(!Reference(r,blob)||!DecodeFrame(blob,t)||(t.*member).size()!=1)return false;(result.*member).push_back(std::move((t.*member)[0]));}
			return true;
		};
		if(!table(&RemasterFrame::assets)||!table(&RemasterFrame::assetMetadata)||!table(&RemasterFrame::assetGroups)||!table(&RemasterFrame::materials))return false;
		RemasterFrame dynamic;if(!Reference(r,blob)||!DecodeFrame(blob,dynamic))return false;
		dynamic.assets=std::move(result.assets);dynamic.assetMetadata=std::move(result.assetMetadata);dynamic.assetGroups=std::move(result.assetGroups);dynamic.materials=std::move(result.materials);
		uint32_t count;if(!r.ReadU32(count)||count>dynamic.tileInstances.size())return false;
		for(uint32_t i=0;i<count;i++) {
			uint32_t id;RemasterFrame t;if(!r.ReadU32(id)||!id||id>dynamic.tileInstances.size()||!Reference(r,blob)||!DecodeFrame(blob,t)||t.assetMetadata.size()!=1||dynamic.contextGeometry.count(id))return false;
			const auto &m=t.assetMetadata[0];RemasterTileContext c;c.height=m.height;c.hasHeight=m.hasHeight;c.heightSampling=m.heightSampling;c.normalXyz=m.normalXyz;c.hasNormals=m.hasNormals;c.normalIgnoreTileTransform=m.normalIgnoreTileTransform;c.directLightingOppositeFacing=m.directLightingOppositeFacing;dynamic.contextGeometry[id]=c;
		}
		for(auto *pixels:{&dynamic.mainRoomDiagnostics,&dynamic.subRoomDiagnostics,&dynamic.sceneRoomDiagnostics}) {
			if(!Reference(r,blob))return false;Reader d(blob);uint32_t n;
			if(!d.ReadU32(n)||(n && n!=size_t(dynamic.width)*dynamic.height)||n>(blob.size()-d.offset)/71)return false;
			pixels->resize(n);
			for(auto &pixel:*pixels){if(!d.ReadU8(pixel.visiblePlane))return false;for(auto &cell:pixel.planes)if(!Diagnostic(d,cell))return false;}
			if(d.offset!=blob.size())return false;
		}
		if(r.offset!=b.size())return false;
		frame=std::move(dynamic);return true;
	} catch(const std::exception &e){error=std::string("Recording decode error: ")+e.what();return false;}
};

class Recorder {
	std::mutex lifecycle;
	std::mutex mutex;
	std::condition_variable ready;
	std::deque<Packet> queue;
	std::thread worker;
	bool stopping=false;
	uint64_t submitted=0;
	uint32_t revisions=0;
	size_t queuedBytes=0;
	std::string status="Idle",path;
	std::atomic<bool> active{false};
	std::atomic<bool> gameplay{false};
public:
	~Recorder(){Stop();}
	bool Active() const{return active.load();}
	void SetGameplay(bool value){gameplay.store(value);}
	bool Accepting() const{return Active()&&gameplay.load();}
	std::string Status(){std::lock_guard<std::mutex> lock(mutex);return status;}
	bool Start(const std::string &destination) {
		Stop();auto writer=std::make_shared<Writer>();
		if(!writer->Open(destination)){std::lock_guard<std::mutex> lock(mutex);status="Cannot exclusively create recording: "+destination;return false;}
		{std::lock_guard<std::mutex> lock(mutex);stopping=false;submitted=0;revisions=0;queuedBytes=0;queue.clear();path=destination;status="Recording (suspended whenever emulation is paused): "+path;active.store(true);}
		worker=std::thread([this,writer] {
			try {
				for(;;) {
					Packet p;
					{std::unique_lock<std::mutex> lock(mutex);ready.wait(lock,[&]{return stopping||!queue.empty();});if(queue.empty())break;queuedBytes-=Weight(queue.front());p=std::move(queue.front());queue.pop_front();}
					if(!writer->Append(p))throw std::runtime_error("write/format/storage bound reached");
				}
				if(!writer->Finish())throw std::runtime_error("completion write failed");
				std::lock_guard<std::mutex> lock(mutex);if(status.find("ERROR")==std::string::npos)status="Completed "+std::to_string(submitted)+" frames, "+std::to_string(writer->Size())+" bytes: "+path;
			} catch(const std::exception &e) {std::lock_guard<std::mutex> lock(mutex);status="ERROR: "+std::string(e.what())+"; recoverable prefix: "+path;queue.clear();stopping=true;}
			active.store(false);
		});return true;
	}
	void Fail(const std::string &why) {std::lock_guard<std::mutex> lock(mutex);status="ERROR: "+why+"; retained prefix: "+path;active.store(false);stopping=true;ready.notify_one();}
	bool Submit(Packet &&p) {
		std::lock_guard<std::mutex> lock(mutex);
		if(!Accepting()||stopping)return false;
		const size_t weight=Weight(p);
		if(queue.size()>=4 || !WithinBounds(p) || queuedBytes+weight>MaxQueuedBytes || submitted>=MaxFrames){status="ERROR: recording queue/frame limit reached; no frames silently dropped: "+path;active.store(false);stopping=true;ready.notify_one();return false;}
		queuedBytes+=weight;p.sequence=submitted++;queue.push_back(std::move(p));ready.notify_one();return true;
	}
	void Revision(const RemasterProfile &profile,RemasterAreaContext area={}) {
		if(!Active())return;
		try {
			Packet p;p.profile=std::make_shared<const RemasterProfile>(profile);p.revisionOnly=true;p.revisionArea=area;
			std::lock_guard<std::mutex> lock(mutex);if(!active.load()||stopping)return;
			if(queue.size()>=4 || revisions>=MaxFrames){status="ERROR: revision queue/event limit reached: "+path;active.store(false);stopping=true;ready.notify_one();return;}
			++revisions;queuedBytes+=Weight(p);queue.push_back(std::move(p));ready.notify_one();
		} catch(const std::exception &e){Fail(e.what());}
	}
	void Stop() {
		std::lock_guard<std::mutex> owner(lifecycle);
		{std::lock_guard<std::mutex> lock(mutex);active.store(false);stopping=true;ready.notify_one();}
		if(worker.joinable())worker.join();
	}
};
inline Recorder &Global(){static Recorder recorder;return recorder;}
} // namespace RemasterRecording
#endif
