#include "../runtime/overworld_geometry.h"
#include <cstdlib>
#include <iostream>
using namespace remaster::overworld;
#define CHECK(x) do { if (!(x)) { std::cerr << __LINE__ << ": " #x "\n"; std::abort(); } } while (0)

int main() {
    for (int raw = 0; raw < 256; ++raw) for (int f = 0; f < 4; ++f) {
        uint16_t w = uint16_t(f << 14);
        CHECK(AdjustAttribute(raw,w) == (raw >= 16 && raw < 28 ? (raw | (f&1)) : raw));
    }
    CHECK(ClassifyBehavior(0) == Behavior::Normal);
    CHECK(ClassifyBehavior(1) == Behavior::Solid);
    CHECK(ClassifyBehavior(0x14) == Behavior::Normal);
    CHECK(ClassifyBehavior(8) == Behavior::Water);
    CHECK(ClassifyBehavior(0x28) == Behavior::Transition);
    CHECK(ClassifyBehavior(0x50) == Behavior::Interactive);
    auto samples = MovementSamples(100,200,0,false);
    CHECK(samples[0].x == 100 && samples[1].x == 108 && samples[2].x == 115);
    CHECK(samples[0].y == 208 && samples[2].bits == 4);
    samples = MovementSamples(100,200,2,true);
    CHECK(samples[0].x == 100 && samples[0].y == 208 && samples[2].y == 223);
    samples = MovementSamples(65535,65535,1,false);
    CHECK(samples[0].y == 23 && samples[2].x == 14);
    auto levels = SolveSupports(5,{{0,1,48,"measured endpoints"},{1,2,16,"obstacle rise"}},
                                  {{0,0,"ground datum"},{4,-8,"other surface"}});
    CHECK(levels[1].height == 48 && levels[2].height == 64);
    CHECK(levels[3].status == Status::Unresolved && levels[4].height == -8);
    auto conflict = SolveSupports(3,{{0,1,4,"a"},{1,2,4,"b"},{0,2,9,"bad cycle"}},{});
    for (const auto &l : conflict) CHECK(l.status == Status::Conflict);
    conflict = SolveSupports(2,{{0,1,4,"a"}},{{0,0,"datum"},{1,9,"bad pin"}});
    CHECK(conflict[0].status == Status::Conflict && conflict[1].status == Status::Conflict);

    MapKey key; key.graphics = "castle-theme"; key.state = "base-ROM-only";
    Family f; f.id = "test-only-wall-family"; f.evidence = "synthetic matched endpoint fixture";
    f.graphics = key.graphics; f.width = 2; f.height = 1; f.allowH = f.allowV = true;
    Member m; m.word = 369; m.map16 = 10; m.map16Member = 0;
    m.geometry = Geometry::Face; m.role = Role::Wall; m.low = 0; m.high = 1;
    m.axisX = 1; m.axisY = 0; m.span = 15; m.normalX = 1; m.normalY = 0;
    f.members.push_back(m); m.x = 1; m.map16Member = 1; f.members.push_back(m);
    std::vector<Placement> p(2);
    for (size_t i = 0; i < p.size(); ++i) {
        p[i].x = 120+int(i); p[i].y = 64; p[i].map16 = 10; p[i].map16Member = i;
        p[i].word = 369; p[i].rawCollision = 1; p[i].hash = 0xb3881650e2d5d461ULL;
    }
    Assembly a; a.family = &f; a.x = 120; a.y = 64; a.supports = {{0,0},{1,1}};
    auto r = Resolve(key,p,{a},levels);
    CHECK(r[0].hasGeometry && r[1].hasGeometry);
    CHECK(r[0].local[7] == 22 && r[1].local[0] == 26 && r[1].local[7] == 48);
    CHECK(r[1].normals[0].x == 1 && r[1].normals[0].z == 0);
    // Same rise over unequal projected spans; structural normal stays independent.
    Family tall = f; tall.members.resize(1); tall.width = 1;
    tall.members[0].span = 7;
    a.family = &tall;
    r = Resolve(key,{p[0]},{a},levels);
    CHECK(r[0].local[7] == 48 && r[0].normals[0].x == 1);
    for (int h = 0; h < 2; ++h) for (int v = 0; v < 2; ++v) {
        a.flipH = h; a.flipV = v;
        auto q = p[0]; q.word = FlippedWord(q.word,h,v); q.map16Member ^= (h ? 1 : 0) ^ (v ? 2 : 0);
        r = Resolve(key,{q},{a},levels);
        CHECK(r[0].hasGeometry);
        CHECK(r[0].local[(v ? 7 : 0)*8+(h ? 0 : 7)] == 48);
        CHECK(r[0].normals[0].x == (h ? -1 : 1));
    }
    a.flipH = a.flipV = false; a.family = &f;
    r = Resolve(key,p,{a,a},levels);
    CHECK(r[0].status == Status::Conflict && !r[0].hasGeometry);
    auto overwritten = p; overwritten[1].writer = 7;
    r = Resolve(key,overwritten,{a},levels);
    CHECK(r[0].status == Status::Unresolved);
    f.members[1].replaceable = true;
    r = Resolve(key,overwritten,{a},levels);
    CHECK(r[0].hasGeometry && r[1].status == Status::Unresolved);
    f.members[1].replaceable = false;
    // Shared hash, identical flips: wall, upper floor, unresolved, separate plane.
    Family floor = tall; floor.id = "synthetic-floor";
    floor.members[0].word = 271; floor.members[0].role = Role::Floor;
    floor.members[0].geometry = Geometry::Flat; floor.members[0].support = 0;
    floor.members[0].detail.fill(3);
    auto q = p[0]; q.x += 5; q.word = 271; q.rawCollision = 0;
    auto unknown = q; unknown.x += 1;
    auto under = q; under.surface = 1;
    Assembly af = a; af.family = &floor; af.x = q.x; af.supports = {{0,1}};
    auto au = af; au.surface = 1; au.supports = {{0,4}};
    auto pp = p; pp.push_back(q); pp.push_back(unknown); pp.push_back(under);
    r = Resolve(key,pp,{a,af,au},levels);
    CHECK(r[0].role == Role::Wall && r[0].base == 0);
    CHECK(r[2].role == Role::Floor && r[2].base == 48 && r[2].local[0] == 3);
    CHECK(r[3].role == Role::Unknown && !r[3].hasGeometry);
    CHECK(r[4].base == -8 && r[4].hasGeometry);
    CHECK(r[2].normals[0].z == 1);
    // Mixed cap/face pixel patches preserve structural normals and transform as
    // one assembly. A flat hash-global override cannot represent these samples.
    Family mixed = floor; auto &mm=mixed.members[0];
    mm.geometry=Geometry::Sampled; mm.low=0; mm.high=1; mm.detail.fill(0);
    for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
        int i=y*8+x; mm.progress[i]=y<2?65535:uint16_t((7-y)*65535/5);
        mm.pixelNormals[i]=y<2?Normal{0,0,1}:Normal{0,1,0};
    }
    Assembly am=af; am.family=&mixed; am.supports={{0,0},{1,1}};
    auto mr=Resolve(key,{q},{am},levels);
    CHECK(mr[0].hasGeometry && mr[0].local[0]==48 && mr[0].local[63]==0);
    CHECK(mr[0].normals[0].z==1 && mr[0].normals[63].y==1);
    am.flipV=true; auto mq=q; mq.word^=0x8000; mq.map16Member^=2;
    mr=Resolve(key,{mq},{am},levels);
    CHECK(mr[0].hasGeometry && mr[0].local[56]==48 && mr[0].normals[0].y==-1);
    // Face high-edge/cap seam survives whole-assembly H/V transformations.
    Family seam=f;
    for(size_t j=0;j<2;++j) {
        auto &sm=seam.members[j]; sm.geometry=Geometry::Sampled;
        sm.low=0;sm.high=1;sm.detail.fill(0);
        for(int y=0;y<8;++y)for(int x=0;x<8;++x) {
            sm.progress[y*8+x]=j?65535:uint16_t(x*65535/7);
            sm.pixelNormals[y*8+x]=j?Normal{0,0,1}:Normal{-1,0,0};
        }
    }
    for(int h=0;h<2;++h)for(int v=0;v<2;++v) {
        auto sp=p; Assembly sa;sa.family=&seam;sa.x=120;sa.y=64;
        sa.flipH=h;sa.flipV=v;sa.supports={{0,0},{1,1}};
        for(size_t i=0;i<2;++i) {
            sp[i].x=120+(h?1-int(i):int(i));sp[i].word=FlippedWord(p[i].word,h,v);
            sp[i].map16Member=p[i].map16Member^(h?1:0)^(v?2:0);
        }
        auto sr=Resolve(key,sp,{sa},levels);
        CHECK(sr[0].hasGeometry&&sr[1].hasGeometry);
        for(int y=0;y<8;++y) {
            int fy=y*8+(h?0:7),cy=y*8+(h?7:0);
            CHECK(sr[0].local[fy]==sr[1].local[cy]);
            CHECK(sr[0].normals[fy].x==(h?1:-1));
            CHECK(sr[1].normals[cy].z==1);
        }
    }
    // Obstacle relief is relative to its proven terrace support, not nearby
    // courtyard/screen position; authored detail is composed exactly once.
    Family obstacle = floor; obstacle.members[0].role = Role::Obstacle;
    obstacle.members[0].geometry = Geometry::Detail;
    obstacle.members[0].detail.fill(12);
    Assembly ao = af; ao.family = &obstacle;
    auto raised = Resolve(key,{q},{ao},levels);
    CHECK(raised[0].role == Role::Obstacle && raised[0].base == 48 && raised[0].local[0] == 12);
    floor.contacts = {{0,0,1},{0,1,1}};
    std::vector<Assembly> contactAssemblies{af};
    BindSupports(contactAssemblies,{{q.x,q.y+1,0,0,1,"foot"},{q.x+1,q.y+1,0,0,1,"seam"}});
    CHECK(contactAssemblies[0].supports.at(0) == 1);
    BindSupports(contactAssemblies,{{q.x,q.y+1,0,0,1,"foot"},{q.x+1,q.y+1,0,0,0,"conflicting seam"}});
    CHECK(contactAssemblies[0].supports.empty());
    BindSupports(contactAssemblies,{{q.x,q.y+1,0,0,1,"only one contact"}});
    CHECK(contactAssemblies[0].supports.empty());
    // Corner cannot borrow a single face, nor average incompatible endpoints.
    tall.members[0].geometry = Geometry::Corner;
    tall.members[0].cornerLow = 2; tall.members[0].cornerHigh = 3;
    a.family = &tall;
    r = Resolve(key,{p[0]},{a},levels); CHECK(!r[0].hasGeometry);
    a.supports[2] = 0; a.supports[3] = 2;
    r = Resolve(key,{p[0]},{a},levels); CHECK(r[0].status == Status::Conflict);
    a.supports[3] = 1;
    r = Resolve(key,{p[0]},{a},levels); CHECK(r[0].hasGeometry);
    tall.members[0].mask[0] = 0;
    r = Resolve(key,{p[0]},{a},levels); CHECK(!r[0].hasGeometry);
    tall.members[0].mask[0] = 1;
    tall.members[0].span = 0;
    r = Resolve(key,{p[0]},{a},levels); CHECK(!r[0].hasGeometry);
    a.family = &f;
    r = Resolve(key,p,{a},{}); CHECK(!r[0].hasGeometry);
    MapKey changed = key; changed.state = "overlay-revision-1"; CHECK(changed != key);
    changed = key; changed.provenance = "writer-2"; CHECK(changed != key);
    changed = key; changed.graphics = "different-gfx";
    r = Resolve(changed,p,{a},levels); CHECK(r[0].status == Status::Unresolved);
    std::vector<Family> retainedFamilies{f}; // assemblies borrow family lifetime
    auto discovered = Discover(key,p,retainedFamilies);
    CHECK(discovered.size() == 1 && discovered[0].x == 120 && discovered[0].y == 64);
    bool rejected = false;
    try { Resolve(key,{p[0],p[0]},{},{}); } catch (const std::invalid_argument &) { rejected = true; }
    CHECK(rejected);
    // Exact non-owning guards may validate a neighboring assembly without
    // competing for its geometry. Missing/overwritten guard rejects the owner.
    Family guarded=f; guarded.members[1].owns=false;
    Assembly ag=a; ag.family=&guarded;
    auto gr=Resolve(key,p,{ag},levels);
    CHECK(gr[0].hasGeometry && gr[1].status==Status::Unresolved);
    auto gp=p; gp[1].word^=1;
    gr=Resolve(key,gp,{ag},levels); CHECK(!gr[0].hasGeometry);
    std::cout << "overworld geometry: collision, support, assembly, anisotropy, flips, overwrite, ambiguity, overlap OK\n";
}
