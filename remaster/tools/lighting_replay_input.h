#ifndef REMASTER_LIGHTING_REPLAY_INPUT_H
#define REMASTER_LIGHTING_REPLAY_INPUT_H
// Own-input replay only: packet, contemporaneous profile and complete native
// sidecars. Controlled event mutations must be labelled by the caller.
struct LightingReplayInput {
    std::unique_ptr<RemasterFrame> captured=std::make_unique<RemasterFrame>();
    RemasterProfile profile;
    RemasterDungeonFloorContext context;
    std::vector<uint8_t> input,templates;
    static std::vector<uint8_t> read(const std::string &p) {
        std::ifstream in(p,std::ios::binary); if(!in)throw std::runtime_error("missing sidecar: "+p);
        return {std::istreambuf_iterator<char>(in),{}};
    }
    LightingReplayInput(const std::string &path) {
        input=read(path+".dungeon-input.bin"); templates=read(path+".dungeon-templates.bin");
        if(input.size()!=90120 || templates.size()!=12884)throw std::runtime_error("incomplete sidecars");
        auto bytes=read(path+".dungeon.json"); std::string text(bytes.begin(),bytes.end());
        auto n=[&](const char *key,int index=0)->uint64_t {
            auto pos=text.find(std::string("\"")+key+"\":"); if(pos==std::string::npos)throw std::runtime_error(key);
            pos=text.find(':',pos)+1; if(text[pos]=='[')pos++; for(int i=0;i<index;i++)pos=text.find(',',pos)+1;
            return std::stoull(text.substr(pos));
        };
        std::vector<RemasterProfileDiagnostic> errors;
        if(!S9xReadRemasterFrame(path,*captured) || !S9xRemasterLoadProfile(path+".profile.toml",profile,errors))throw std::runtime_error("own packet/profile");
        auto &c=context;c.verifiedAlttpRom=c.indoors=true;c.roomIndex=n("room");c.previousRoomIndex=n("previous_room");
        c.submodule=n("submodule");c.collisionMode=n("collision_mode");c.linkX=n("link_xy");c.linkY=n("link_xy",1);c.linkPlane=n("plane");
        if(text.find("\"module\":")!=std::string::npos)c.module=n("module");
        if(text.find("\"rom_identity\":")!=std::string::npos)c.romIdentity=n("rom_identity");
        c.groundedEvidence=text.find("\"grounded_evidence\":true")!=std::string::npos;
        c.backgroundScrollX=n("scroll");c.backgroundScrollY=n("scroll",1);c.graphicsBase=n("graphics_base");c.bg1GraphicsBase=n("bg1_graphics_base");
        for(int i=0;i<2;i++){c.ppuMapBase[i]=n("ppu_map_base",i);c.ppuMapSize[i]=n("ppu_map_size",i);}
        c.constructedMaps=input.data()+8;c.collisionAttributes=input.data()+16392;c.graphics=input.data()+24584;c.dungeonTemplateData=templates.data()+8;
        if(std::ifstream(path+".terrain-input.bin").good())S9xRemasterReadTerrainEvidence(path+".terrain-input.bin",c);
        if(std::ifstream(path+".sprite-input.bin").good())S9xRemasterReadSpriteObstacleEvidence(path+".sprite-input.bin",c);
        S9xRemasterDungeonStairsFromAttributes(c);
        S9xRemasterSetLiveFramesEnabled(true);S9xRemasterSetProfile(profile);S9xRemasterInvalidateDungeonContinuity();
    }
    void run(RemasterFrame &f) {
        auto &s=S9xRemasterState();const auto &p=*captured;
        S9xRemasterBeginFrame(p.width*p.height,p.width,p.width,p.height);
        s.tileInstances=p.tileInstances;for(auto &i:s.tileInstances){i.heightOffset=0;i.hasPlacementHeight=false;}
        for(const auto &a:p.assets){auto &t=s.observedTiles[a.tileId.hash];t.hash=a.tileId.hash;t.bitDepth=a.tileId.bitDepth;std::copy_n(a.indices,64,t.indices);}
        for(size_t i=0;i<p.mainPixels.size();i++) {
            s.mainOwners[i]=p.mainPixels[i].owner;s.subOwners[i]=p.subPixels[i].owner;
            s.mainInstanceIds[i]=p.mainPixels[i].instanceId;s.subInstanceIds[i]=p.subPixels[i].instanceId;
            s.mainTilePixels[i]=p.mainPixels[i].tilePixel;s.subTilePixels[i]=p.subPixels[i].tilePixel;
        }
        s.scenePixels=p.scenePixels;s.sceneRgb=p.sceneRgb555;
        S9xRemasterEndFrame(p.originalRgb555.data(),p.width,p.width,p.height,&f,&context);
    }
};
#endif
