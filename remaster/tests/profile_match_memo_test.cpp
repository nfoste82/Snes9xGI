// Count calls to the authoritative matcher without production instrumentation.
#include "../runtime/profile.h"
#include <cassert>
#include <iostream>
static size_t matchCalls = 0;
static RemasterProfileMatch countedMatch(const RemasterProfile &p, const RemasterProfileMatchContext &c)
{ ++matchCalls; return S9xRemasterMatchProfile(p,c); }
#define S9xRemasterMatchProfile countedMatch
#include "../runtime/frame.h"
#undef S9xRemasterMatchProfile

static void verify(const RemasterProfile &p, RemasterFrame input)
{
    auto expected = input;
    // Independent original per-instance matching, including capture-local names.
    std::set<std::string> captured;
    for (const auto &g : input.assetGroups)
        if (g.name.compare(0,18,"capture_animation_")==0) captured.insert(g.name);
    std::set<std::tuple<RemasterTileContentId,RemasterSourceType,uint8_t,uint8_t>> keys;
    for (auto &i : expected.tileInstances) {
        auto m=S9xRemasterMatchProfile(p,{i.tileId,i.source,i.sourceIndex,i.palette});
        i.matchStatus=m.status; i.ruleLine=m.rule?m.rule->line:0;
        if (!captured.count(i.assetGroup)) i.assetGroup=m.assetGroup?m.assetGroup->name:std::string();
        i.material=m.material?m.material->name:std::string();
        keys.emplace(i.tileId,i.source,i.sourceIndex,i.palette);
    }
    S9xRemasterApplyContextBindings(p,expected);
    matchCalls=0;
    S9xRemasterApplyProfileToFrame(p,input);
    for (size_t n=0;n<input.tileInstances.size();++n) {
        const auto &a=input.tileInstances[n], &b=expected.tileInstances[n];
        assert(a.matchStatus==b.matchStatus && a.ruleLine==b.ruleLine);
        assert(a.material==b.material && a.assetGroup==b.assetGroup);
        assert(a.hFlip==b.hFlip && a.vFlip==b.vFlip && a.heightOffset==b.heightOffset);
    }
    assert(input.assetGroups.size()==p.assetGroups.size()+captured.size());
    size_t n=0;
    for (const auto &g:p.assetGroups) assert(input.assetGroups[n++].name==g.second.name);
    std::cout<<"instances="<<input.tileInstances.size()<<" unique="<<keys.size()<<" matcher_calls="<<matchCalls<<std::endl;
    assert(matchCalls==keys.size());
}

int main()
{
    RemasterProfile p;
    p.materials["a"].name="a"; p.materials["b"].name="b";
    RemasterTileContentId id={17,1,4};
    for (const auto *name:{"z","a"}) { p.assetGroups[name].name=name;p.assetGroups[name].tileIds={id}; }
    RemasterProfileRule r; r.hasTileHash=true; r.tileId=id; r.material="a"; r.line=11; r.specificity=1;
    p.rules.push_back(r);
    r.hasTileHash=false; r.hasAssetGroup=true; r.assetGroup="z"; r.hasPalette=true; r.palette=1; r.specificity=2; r.line=22;
    p.rules.push_back(r);
    // Equal specificity is ambiguous even with the same material/group.
    r.line=23; p.rules.push_back(r);
    r.hasSource=true; r.source=RemasterSourceType::Object; r.specificity=3; r.line=33; r.material="b";
    p.rules.push_back(r);
    r.hasSourceIndex=true; r.sourceIndex=2; r.specificity=4; r.line=44;
    p.rules.push_back(r);
    RemasterFrame f;
    f.assetGroups.push_back({"capture_animation_1",{id}});
    for (int repeat=0;repeat<16;++repeat)
        for (int variant=0;variant<8;++variant) {
            RemasterFrameTileInstance i; i.tileId=id; i.source=RemasterSourceType::Background; i.sourceIndex=1;
            if (variant==1) i.palette=1;
            if (variant==2 || variant==3) { i.palette=1;i.source=RemasterSourceType::Object; }
            if (variant==3) i.sourceIndex=2;
            if (variant==4) i.tileId.hash++;
            if (variant==5) i.tileId.bitDepth=8;
            if (variant==6) i.tileId.hashVersion=2;
            if (variant==7) i.assetGroup="capture_animation_1"; // same key, distinct placement state
            i.hFlip=repeat&1;i.vFlip=repeat&2;i.heightOffset=repeat-8;
            f.tileInstances.push_back(i);
        }
    // Independently pin the intended specificity outcomes, not just parity.
    assert(S9xRemasterMatchProfile(p,{id,RemasterSourceType::Background,1,1}).status==RemasterProfileMatchStatus::Ambiguous);
    assert(S9xRemasterMatchProfile(p,{id,RemasterSourceType::Background,1,1}).conflictingRuleLines==std::vector<size_t>({22,23}));
    assert(S9xRemasterMatchProfile(p,{id,RemasterSourceType::Object,2,1}).rule->line==44);
    assert(S9xRemasterMatchProfile(p,{id,RemasterSourceType::Background,1,0}).material->name=="a");
    auto absent=id;absent.hash++;
    assert(S9xRemasterMatchProfile(p,{absent,RemasterSourceType::Background,1,0}).status==RemasterProfileMatchStatus::NoMatch);
    verify(p,f);
    p.rules[0].material="b"; p.rules.erase(p.rules.begin()+2); verify(p,f);
    p=RemasterProfile(); verify(p,f); // same address, replacement and cached misses
    p.materials["a"].name="replacement"; r={};r.hasTileHash=true;r.tileId=id;r.material="a";r.line=99;
    p.rules.push_back(r);verify(p,f);
    verify(p,RemasterFrame());
    std::cout<<"PASS invocation-local matching parity, work, mutation/replacement\n";
}
