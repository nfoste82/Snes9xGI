#ifndef S9X_REMASTER_OVERWORLD_GEOMETRY_H
#define S9X_REMASTER_OVERWORLD_GEOMETRY_H

// Portable outdoor evidence/geometry core. No renderer, ROM, profile or dungeon
// dependencies. Coordinates and normals are in MAP/art space (+x east, +y south,
// +z up). Pixel arrays are placed/display order, NOT canonical asset order.
// An Unresolved result is an explicit override: callers must not fall back to
// hash-global geometry. Collision classification is never an elevation anchor.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <queue>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace remaster { namespace overworld {

enum class Behavior : uint8_t { Normal, Solid, Slope, Water, Pit, Transition,
                               Interactive, Special };
enum class Role : uint8_t { Unknown, Floor, Wall, Cap, Corner, Obstacle,
                           Transition, Empty, NonGeometric };
enum class Status : uint8_t { Unresolved, Classified, Solved, Conflict,
                             Overwritten, NonGeometric };
enum class Geometry : uint8_t { Flat, Face, Corner, Detail, Ramp, Sampled };

// Verified against Zelda3 tile_detect.c Overworld_GetTileAttributeAtLocation:
// V flip is NOT applied to the collision attribute; H adjustment is OR, not XOR.
inline uint8_t AdjustAttribute(uint8_t raw, uint16_t word) {
    return raw >= 0x10 && raw < 0x1c ? uint8_t(raw | ((word >> 14) & 1)) : raw;
}
inline Behavior ClassifyBehavior(uint8_t a) {
    if (a == 1 || a == 2 || a == 3 || a == 0x26 || a == 0x43)
        return Behavior::Solid;
    if ((a >= 0x10 && a <= 0x13) || (a >= 0x18 && a <= 0x1b))
        return Behavior::Slope;
    if (a == 8 || a == 9 || a == 0xb) return Behavior::Water;
    if (a == 0x20 || (a >= 0xb0 && a <= 0xbd)) return Behavior::Pit;
    if (a == 0xa || (a >= 0x1c && a <= 0x1f) || a == 0x22 ||
        (a >= 0x28 && a <= 0x37) || (a >= 0x3d && a <= 0x3f) ||
        a == 0x4b || (a >= 0x80 && a <= 0xaf)) return Behavior::Transition;
    if (a == 0 || (a >= 5 && a <= 7) || (a >= 0x14 && a <= 0x17) ||
        a == 0x21 || (a >= 0x23 && a <= 0x25) ||
        (a >= 0x38 && a <= 0x3c) || a == 0x41 || a == 0x45 ||
        a == 0x47 || a == 0x48 || a == 0x49 || a == 0x4a ||
        a == 0x5e || a == 0x5f || a == 0x60 || a == 0x61 ||
        a == 0x62 || a == 0x64 || a == 0x65 || a == 0x66 ||
        (a >= 0x6c && a <= 0x6f) || a == 0xbe || a == 0xbf ||
        (a >= 0xd0 && a <= 0xef)) return Behavior::Normal;
    if ((a >= 0x50 && a <= 0x5d) || a == 0x27 || a == 0x42 ||
        a == 0x46 || a == 0x63 || a == 0x67 ||
        (a >= 0x70 && a <= 0x7f) || a >= 0xc0) return Behavior::Interactive;
    return Behavior::Special;
}

struct CollisionSample { uint16_t x = 0, y = 0; uint8_t bits = 0; };
// TileDetect_Movement_X/Y exact three-point sample positions BEFORE the game's
// tilemap_location_calc_mask. This is a footprint primitive, not a legal-movement
// oracle: slopes, water, ledges, actors and state handlers must still be executed.
// Direction uses the original handler index 0..3, not a guessed compass enum.
inline std::array<CollisionSample,3> MovementSamples(uint16_t linkX, uint16_t linkY,
                                                    unsigned direction, bool movementX) {
    if (direction >= 4) throw std::invalid_argument("movement sample direction");
    const unsigned primary[4] = {8,24,0,15};
    const unsigned a[4] = {0,0,8,8}, b[4] = {8,8,16,16}, c[4] = {15,15,23,23};
    std::array<CollisionSample,3> out{};
    const unsigned offset[3] = {a[direction],b[direction],c[direction]};
    for (unsigned i = 0; i < 3; ++i)
        out[i] = {uint16_t(linkX+(movementX ? primary[direction] : offset[i])),
                  uint16_t(linkY+(movementX ? offset[i] : primary[direction])), uint8_t(1u<<i)};
    return out;
}

struct MapKey {
    std::string rom, state, graphics, metadata, provenance;
    uint16_t area = 0;
    bool operator==(const MapKey &b) const {
        return std::tie(rom,state,graphics,metadata,provenance,area) ==
               std::tie(b.rom,b.state,b.graphics,b.metadata,b.provenance,b.area);
    }
    bool operator!=(const MapKey &b) const { return !(*this == b); }
};
struct Placement {
    int32_t x = 0, y = 0; // stable global Map8 cells, never viewport coordinates
    uint8_t layer = 0, surface = 0;
    uint16_t quadrant = 0, map32 = 0, map16 = 0, word = 0;
    uint8_t map32Member = 0, map16Member = 0, rawCollision = 0;
    uint32_t writer = 0; // final writer; 0 = original ROM expansion
    uint64_t hash = 0;   // diagnostic identity ONLY; never a rule selector
};
struct Normal { float x = 0, y = 0, z = 1; };
struct Result {
    Role role = Role::Unknown;
    Status status = Status::Unresolved;
    Behavior behavior = Behavior::Special;
    uint8_t rawCollision = 0, collision = 0;
    int32_t assembly = -1, support = -1, base = 0;
    bool hasGeometry = false;
    std::array<int16_t,64> local{};
    std::array<Normal,64> normals{};
    std::string evidence = "missing assembly semantics";
};

// Families match complete final metatile/assembly words and provenance, never
// hashes/palettes alone. Offsets are canonical Map8 cells in the family. Flips
// transform offsets, pixel masks, ramps and vectors together. A partial overwrite
// is accepted only when the member is explicitly replaceable. It is never claimed
// by the old assembly. Caller supplies dispatch/writer evidence for new members.
struct Member {
    int x = 0, y = 0;
    uint16_t map16 = 0, word = 0;
    uint8_t map16Member = 0;
    Role role = Role::Unknown;
    Geometry geometry = Geometry::Flat;
    int support = -1, low = -1, high = -1;
    float normalX = 0, normalY = 0;
    // progress = (assembly pixel dot axis - start) / span; NO tile restart.
    float axisX = 0, axisY = 1, start = 0, span = 1;
    // A corner requires both independently solved face endpoint pairs.
    int cornerLow = -1, cornerHigh = -1;
    bool replaceable = false;
    bool owns = true; // exact provenance guard, validated but not geometry-owned
    std::array<uint8_t,64> mask = [] { std::array<uint8_t,64> a{}; a.fill(1); return a; }();
    std::array<int16_t,64> detail{}; // additive ONCE, canonical pixel order
    // Authored mixed face/cap/corner patches. 0..65535 normalized assembly
    // progress, with independent structural normals; never inferred from art hash.
    std::array<uint16_t,64> progress{};
    std::array<Normal,64> pixelNormals{};
};
struct Contact { int endpoint = -1, x = 0, y = 0; };
struct Family {
    std::string id, evidence, graphics;
    int width = 0, height = 0;
    bool allowH = false, allowV = false;
    std::vector<Member> members;
    std::vector<Contact> contacts; // canonical support seams/footprint samples
};
struct Assembly {
    // Borrowed immutable family. Must outlive discovery, resolve and consumers.
    const Family *family = nullptr;
    int32_t x = 0, y = 0;
    uint8_t layer = 0, surface = 0;
    bool flipH = false, flipV = false;
    uint32_t writer = 0;
    // Family-local endpoint IDs -> state/plane-specific support graph nodes.
    std::map<int,int> supports;
};
struct Constraint { int from = -1, to = -1; int32_t rise = 0; std::string evidence; };
struct Anchor { int node = -1; int32_t height = 0; std::string evidence; };
struct Level { Status status = Status::Unresolved; int32_t height = 0; };
struct SupportObservation {
    int32_t x = 0, y = 0;
    uint8_t layer = 0, surface = 0;
    int node = -1;
    std::string evidence; // accepted movement/support producer, never proximity
};

// Bind family endpoints through declared transformed seam contacts. All contacts
// for an endpoint must be observed and agree on one support node; no nearest
// neighbor or collision-cell flood fill. Distinct copies can have distinct bases.
inline void BindSupports(std::vector<Assembly> &assemblies,
                         const std::vector<SupportObservation> &observations) {
    using Cell = std::tuple<int32_t,int32_t,uint8_t,uint8_t>;
    std::map<Cell,std::vector<int>> cells;
    for (const auto &o : observations)
        if (o.node >= 0 && !o.evidence.empty()) cells[{o.x,o.y,o.layer,o.surface}].push_back(o.node);
    for (auto &a : assemblies) {
        if (!a.family) continue;
        std::map<int,std::vector<int>> endpoints;
        for (const auto &c : a.family->contacts) {
            auto &nodes = endpoints[c.endpoint];
            int x = a.x + (a.flipH ? a.family->width-1-c.x : c.x);
            int y = a.y + (a.flipV ? a.family->height-1-c.y : c.y);
            auto it = cells.find({x,y,a.layer,a.surface});
            if (it == cells.end()) nodes.push_back(-1);
            else nodes.insert(nodes.end(),it->second.begin(),it->second.end());
        }
        for (auto &e : endpoints) {
            a.supports.erase(e.first); // never retain stale previous-state binding
            const auto &ns = e.second;
            if (!ns.empty() && ns.front() >= 0 &&
                std::all_of(ns.begin(),ns.end(),[&](int n) { return n == ns.front(); }))
                a.supports[e.first] = ns.front();
        }
    }
}

// Every connected contradictory component is rejected in its entirety. Directed
// transitions remain directed in the input; equality solving is not navigation.
inline std::vector<Level> SolveSupports(size_t count,
        const std::vector<Constraint> &edges, const std::vector<Anchor> &anchors) {
    std::vector<std::vector<std::pair<int,int64_t>>> adj(count);
    for (const auto &e : edges) {
        if (e.from < 0 || e.to < 0 || size_t(e.from) >= count || size_t(e.to) >= count || e.evidence.empty())
            throw std::invalid_argument("unsupported support constraint");
        adj[e.from].push_back({e.to,e.rise}); adj[e.to].push_back({e.from,-int64_t(e.rise)});
    }
    std::vector<std::vector<int32_t>> pins(count);
    for (const auto &a : anchors) {
        if (a.node < 0 || size_t(a.node) >= count || a.evidence.empty())
            throw std::invalid_argument("unsupported support anchor");
        pins[a.node].push_back(a.height);
    }
    std::vector<Level> out(count);
    std::vector<bool> seen(count,false);
    std::vector<int64_t> rel(count);
    for (size_t root = 0; root < count; ++root) {
        if (seen[root]) continue;
        std::vector<int> component{int(root)}; seen[root] = true;
        bool conflict = false, anchored = false; int64_t datum = 0;
        for (size_t i = 0; i < component.size(); ++i) {
            const int n = component[i];
            for (int32_t pin : pins[n]) {
                int64_t d = int64_t(pin) - rel[n];
                if (anchored && datum != d) conflict = true;
                datum = d; anchored = true;
            }
            for (auto e : adj[n]) {
                int64_t z = rel[n] + e.second;
                if (seen[e.first]) { if (rel[e.first] != z) conflict = true; }
                else { seen[e.first] = true; rel[e.first] = z; component.push_back(e.first); }
            }
        }
        for (int n : component)
            if (anchored && (rel[n] + datum < INT32_MIN || rel[n] + datum > INT32_MAX)) conflict = true;
        for (int n : component) {
            out[n].status = conflict ? Status::Conflict : anchored ? Status::Solved : Status::Unresolved;
            if (out[n].status == Status::Solved) out[n].height = int32_t(rel[n] + datum);
        }
    }
    return out;
}

inline uint16_t FlippedWord(uint16_t w, bool h, bool v) {
    return uint16_t(w ^ (h ? 0x4000 : 0) ^ (v ? 0x8000 : 0));
}

using Cell = std::tuple<int32_t,int32_t,uint8_t,uint8_t>;
inline bool MatchAssembly(const MapKey &key, const std::vector<Placement> &placements,
        const std::map<Cell,size_t> &index, const Assembly &a,
        std::vector<std::pair<size_t,const Member *>> &members) {
    members.clear();
    if (!a.family) return false;
    const auto &f = *a.family;
    if (f.evidence.empty() || f.graphics != key.graphics || f.width <= 0 || f.height <= 0 ||
        (a.flipH && !f.allowH) || (a.flipV && !f.allowV) || f.members.empty()) return false;
    std::map<std::pair<int,int>,bool> unique;
    for (const auto &m : f.members) {
        if (m.x < 0 || m.y < 0 || m.x >= f.width || m.y >= f.height ||
            !unique.emplace(std::make_pair(m.x,m.y),true).second) return false;
        int x = a.x + (a.flipH ? f.width-1-m.x : m.x);
        int y = a.y + (a.flipV ? f.height-1-m.y : m.y);
        auto it = index.find(Cell{x,y,a.layer,a.surface});
        if (it == index.end()) return false;
        const auto &p = placements[it->second];
        if (p.writer != a.writer) {
            if (m.replaceable) continue;
            return false;
        }
        uint8_t part = uint8_t(m.map16Member ^ (a.flipH ? 1 : 0) ^ (a.flipV ? 2 : 0));
        if (p.map16 != m.map16 || p.map16Member != part ||
            p.word != FlippedWord(m.word,a.flipH,a.flipV)) return false;
        if (m.owns) members.push_back({it->second,&m});
    }
    return !members.empty();
}

inline std::vector<Result> Resolve(const MapKey &key, const std::vector<Placement> &placements,
        const std::vector<Assembly> &assemblies, const std::vector<Level> &levels) {
    std::map<Cell,size_t> index;
    std::vector<Result> out(placements.size());
    for (size_t i = 0; i < placements.size(); ++i) {
        const auto &p = placements[i];
        if (!index.emplace(Cell{p.x,p.y,p.layer,p.surface},i).second)
            throw std::invalid_argument("duplicate outdoor placement on same support surface");
        out[i].rawCollision = p.rawCollision;
        out[i].collision = AdjustAttribute(p.rawCollision,p.word);
        out[i].behavior = ClassifyBehavior(out[i].collision);
    }
    struct Claim { size_t assembly; const Member *member; };
    std::vector<std::vector<Claim>> claims(placements.size());
    for (size_t ai = 0; ai < assemblies.size(); ++ai) {
        const auto &a = assemblies[ai];
        std::vector<std::pair<size_t,const Member *>> members;
        if (MatchAssembly(key,placements,index,a,members))
            for (auto m : members) claims[m.first].push_back({ai,m.second});
    }
    for (size_t i = 0; i < out.size(); ++i) {
        auto &r = out[i];
        if (claims[i].empty()) continue;
        if (claims[i].size() != 1) {
            r.status = Status::Conflict; r.evidence = "ambiguous assembly ownership"; continue;
        }
        const auto &c = claims[i][0]; const auto &a = assemblies[c.assembly]; const auto &m = *c.member;
        r.role = m.role; r.assembly = int32_t(c.assembly); r.status = Status::Classified;
        r.evidence = a.family->id + ": " + a.family->evidence;
        if (m.role == Role::Empty || m.role == Role::NonGeometric) { r.status = Status::NonGeometric; continue; }
        auto level = [&](int local) -> Level {
            auto it = a.supports.find(local);
            return it == a.supports.end() || it->second < 0 || size_t(it->second) >= levels.size()
                ? Level{} : levels[it->second];
        };
        Level base = level(m.support), low = level(m.low), high = level(m.high);
        auto support = a.supports.find(m.support);
        if (support != a.supports.end()) r.support = support->second;
        bool sampled = m.geometry == Geometry::Sampled;
        bool face = m.geometry == Geometry::Face || m.geometry == Geometry::Corner || m.geometry == Geometry::Ramp || sampled;
        if (face) {
            base = low;
            auto it = a.supports.find(m.low);
            if (it != a.supports.end()) r.support = it->second;
        }
        if (base.status == Status::Conflict || (face && high.status == Status::Conflict)) {
            r.status = Status::Conflict; r.evidence += "; conflicting support endpoints"; continue;
        }
        if (base.status != Status::Solved || (face && high.status != Status::Solved)) {
            r.evidence += "; missing solved support endpoints"; continue;
        }
        if (m.geometry == Geometry::Corner) {
            auto cl = level(m.cornerLow), ch = level(m.cornerHigh);
            if (cl.status != Status::Solved || ch.status != Status::Solved) {
                r.evidence += "; missing second corner facing"; continue;
            }
            if (cl.height != low.height || ch.height != high.height) {
                r.status = Status::Conflict; r.evidence += "; corner endpoint disagreement"; continue;
            }
        }
        if (face && !sampled && (!std::isfinite(m.span) || m.span <= 0 ||
            !std::isfinite(m.axisX) || !std::isfinite(m.axisY) || !std::isfinite(m.start) ||
            !std::isfinite(m.normalX) || !std::isfinite(m.normalY) ||
            m.normalX*m.normalX + m.normalY*m.normalY <= 0)) {
            r.evidence += "; invalid face projection"; continue;
        }
        r.base = base.height;
        bool valid = true;
        for (int y = 0; y < 8; ++y) for (int x = 0; x < 8; ++x) {
            int canonical = y*8+x, placed = (a.flipV ? 7-y : y)*8 + (a.flipH ? 7-x : x);
            // No partial pixel default geometry: masks need a separately owned
            // surface in the transport. Until then reject mixed face/cap tiles.
            if (!m.mask[canonical]) { valid = false; continue; }
            double local = m.detail[canonical];
            if (sampled) {
                local += double(m.progress[canonical])/65535 * (int64_t(high.height)-low.height);
                auto n=m.pixelNormals[canonical];
                double len=std::sqrt(double(n.x)*n.x+double(n.y)*n.y+double(n.z)*n.z);
                if (!std::isfinite(len) || len < 1e-6) { valid=false; continue; }
                r.normals[placed]={float((a.flipH ? -1 : 1)*n.x/len),
                                   float((a.flipV ? -1 : 1)*n.y/len),float(n.z/len)};
            } else if (face) {
                double t = ((m.x*8+x)*double(m.axisX) + (m.y*8+y)*double(m.axisY) - m.start) / m.span;
                if (t < -1e-6 || t > 1.000001) { valid = false; continue; }
                local += t * (int64_t(high.height)-low.height);
                if (m.geometry == Geometry::Ramp) {
                    double slope = double(int64_t(high.height)-low.height)/m.span;
                    double nx=-m.axisX*slope, ny=-m.axisY*slope;
                    double len=std::sqrt(nx*nx+ny*ny+1);
                    r.normals[placed] = {float((a.flipH ? -1 : 1)*nx/len),
                                         float((a.flipV ? -1 : 1)*ny/len),float(1/len)};
                } else {
                    double len = std::hypot(m.normalX,m.normalY);
                    r.normals[placed] = {float((a.flipH ? -1 : 1)*m.normalX/len),
                                         float((a.flipV ? -1 : 1)*m.normalY/len),0};
                }
            }
            if (local < INT16_MIN || local > INT16_MAX) { valid = false; continue; }
            r.local[placed] = int16_t(std::lround(local));
        }
        if (valid) { r.status = Status::Solved; r.hasGeometry = true; }
        else { r.local.fill(0); r.normals.fill(Normal{}); r.evidence += "; unsupported mask/projection/range"; }
    }
    return out;
}

// Automatically find exact reusable families at metatile members. No position,
// artwork hash, nearest support or rectangle whitelist participates in discovery.
// Assemblies still need evidence-backed endpoint bindings before they can solve.
inline std::vector<Assembly> Discover(const MapKey &key, const std::vector<Placement> &p,
                                      const std::vector<Family> &families) {
    std::vector<Assembly> found;
    std::map<Cell,size_t> index;
    for (size_t i = 0; i < p.size(); ++i)
        if (!index.emplace(Cell{p[i].x,p[i].y,p[i].layer,p[i].surface},i).second)
            throw std::invalid_argument("duplicate outdoor placement on same support surface");
    for (const auto &f : families) {
        if (f.members.empty() || f.graphics != key.graphics) continue;
        const auto &m = f.members.front();
        for (const auto &tile : p) for (int h = 0; h <= int(f.allowH); ++h)
            for (int v = 0; v <= int(f.allowV); ++v) {
                if (tile.map16 != m.map16 || tile.word != FlippedWord(m.word,h,v) ||
                    tile.map16Member != (m.map16Member ^ (h ? 1 : 0) ^ (v ? 2 : 0))) continue;
                Assembly a; a.family = &f; a.x = tile.x-(h ? f.width-1-m.x : m.x);
                a.y = tile.y-(v ? f.height-1-m.y : m.y); a.flipH = h; a.flipV = v;
                a.layer = tile.layer; a.surface = tile.surface; a.writer = tile.writer;
                std::vector<std::pair<size_t,const Member *>> members;
                if (MatchAssembly(key,p,index,a,members)) found.push_back(a);
            }
    }
    return found;
}

} } // namespace remaster::overworld
#endif
