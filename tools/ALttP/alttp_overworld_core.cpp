// Standalone numeric bridge for alttp_overworld_geometry.py. No runtime/frame
// dependency; compile with c++ -std=c++17 -O2 tools/ALttP/alttp_overworld_core.cpp.
// Text stream OWCORE1, strictly bounded. Output one result per input placement.
#include "../../remaster/overworld_geometry.h"
#include <iostream>
#include <iomanip>
using namespace remaster::overworld;

static void jsonString(const std::string &s) {
    std::cout << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') std::cout << '\\' << c;
        else if (c < 32) {
            const char *hex = "0123456789abcdef";
            std::cout << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else std::cout << c;
    }
    std::cout << '"';
}

static size_t count(size_t maximum) {
    size_t n; if (!(std::cin >> n) || n > maximum) throw std::runtime_error("invalid bridge count");
    return n;
}
static int number(int low, int high) {
    int n; if (!(std::cin >> n) || n < low || n > high) throw std::runtime_error("invalid bridge integer");
    return n;
}
int main() {
    try {
        std::string magic; std::cin >> magic;
        if (magic != "OWCORE1") throw std::runtime_error("invalid bridge format");
        MapKey key;
        std::cin >> std::quoted(key.rom) >> std::quoted(key.state) >> std::quoted(key.graphics)
                 >> std::quoted(key.metadata) >> std::quoted(key.provenance);
        key.area = number(0,159);
        std::vector<Placement> p(count(160*4096));
        for (auto &v : p) {
            v.x = number(-65536,65536); v.y = number(-65536,65536);
            v.layer = number(0,3); v.surface = number(0,255);
            v.quadrant = number(0,159); v.map32 = number(0,65535); v.map16 = number(0,3751);
            v.word = number(0,65535); v.map32Member = number(0,3); v.map16Member = number(0,3);
            v.rawCollision = number(0,255); v.writer = number(0,INT32_MAX);
        }
        size_t nodes = count(65536);
        std::vector<Constraint> edges(count(65536));
        for (auto &e : edges) std::cin >> e.from >> e.to >> e.rise >> std::quoted(e.evidence);
        std::vector<Anchor> anchors(count(65536));
        for (auto &a : anchors) std::cin >> a.node >> a.height >> std::quoted(a.evidence);
        auto levels = SolveSupports(nodes,edges,anchors);
        std::vector<SupportObservation> observations(count(65536));
        for (auto &o : observations) {
            o.x = number(-65536,65536); o.y = number(-65536,65536);
            o.layer = number(0,3); o.surface = number(0,255);
            o.node = number(0,65535); std::cin >> std::quoted(o.evidence);
        }
        std::vector<Family> families(count(65536));
        std::vector<std::map<int,int>> bindings(families.size());
        for (size_t fi = 0; fi < families.size(); ++fi) {
            auto &f = families[fi];
            std::cin >> std::quoted(f.id) >> std::quoted(f.evidence) >> std::quoted(f.graphics);
            f.width = number(1,1024); f.height = number(1,1024);
            f.allowH = number(0,1); f.allowV = number(0,1);
            f.contacts.resize(count(256));
            for (auto &c : f.contacts) {
                c.endpoint = number(0,65535); c.x = number(-1024,1024); c.y = number(-1024,1024);
            }
            for (size_t b = count(256); b; --b) {
                int local = number(0,65535), global = number(0,65535);
                if (!bindings[fi].emplace(local,global).second) throw std::runtime_error("duplicate binding");
            }
            f.members.resize(count(65536));
            for (auto &m : f.members) {
                m.x = number(0,1023); m.y = number(0,1023); m.map16 = number(0,3751);
                m.word = number(0,65535); m.map16Member = number(0,3);
                m.role = Role(number(0,8)); m.geometry = Geometry(number(0,5));
                std::cin >> m.support >> m.low >> m.high >> m.normalX >> m.normalY
                    >> m.axisX >> m.axisY >> m.start >> m.span >> m.cornerLow >> m.cornerHigh;
                m.replaceable = number(0,1);
                m.owns = number(0,1);
                for (auto &d : m.detail) d = number(INT16_MIN,INT16_MAX);
                if (m.geometry == Geometry::Sampled) {
                    for (auto &v : m.progress) v = number(0,65535);
                    for (auto &n : m.pixelNormals) std::cin >> n.x >> n.y >> n.z;
                }
            }
        }
        if (!std::cin) throw std::runtime_error("truncated bridge input");
        std::cin >> std::ws;
        if (!std::cin.eof()) throw std::runtime_error("trailing bridge input");
        auto assemblies = Discover(key,p,families);
        for (auto &a : assemblies) a.supports = bindings[size_t(a.family - families.data())];
        BindSupports(assemblies,observations);
        auto results = Resolve(key,p,assemblies,levels);
        std::cout << "{\"assemblies\":" << assemblies.size() << ",\"assembly_records\":[";
        for (size_t i = 0; i < assemblies.size(); ++i) {
            if (i) std::cout << ',';
            const auto &a = assemblies[i];
            std::cout << "{\"id\":" << i << ",\"family\":"; jsonString(a.family->id);
            std::cout << ",\"x\":" << a.x << ",\"y\":" << a.y << ",\"flip_h\":"
                << (a.flipH ? "true" : "false") << ",\"flip_v\":" << (a.flipV ? "true" : "false")
                << ",\"supports\":[";
            bool first = true;
            for (auto binding : a.supports) {
                if (!first) std::cout << ',';
                first = false; std::cout << '[' << binding.first << ',' << binding.second << ']';
            }
            std::cout << "]}";
        }
        std::cout << "],\"levels\":[";
        for (size_t i = 0; i < levels.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << "[" << int(levels[i].status) << ',' << levels[i].height << ']';
        }
        std::cout << "],\"placements\":[";
        for (size_t i = 0; i < results.size(); ++i) {
            if (i) std::cout << ',';
            const auto &r = results[i];
            std::cout << "{\"role\":" << int(r.role) << ",\"status\":" << int(r.status)
                << ",\"assembly\":" << r.assembly << ",\"support\":" << r.support
                << ",\"base\":" << r.base << ",\"has_geometry\":" << (r.hasGeometry ? "true" : "false")
                << ",\"local\":[";
            for (size_t j = 0; j < 64; ++j) { if (j) std::cout << ','; std::cout << r.local[j]; }
            std::cout << "],\"normals\":[";
            for (size_t j = 0; j < 64; ++j) {
                if (j) std::cout << ',';
                auto n = r.normals[j]; std::cout << '[' << n.x << ',' << n.y << ',' << n.z << ']';
            }
            std::cout << "],\"evidence\":"; jsonString(r.evidence);
            std::cout << '}';
        }
        std::cout << "]}\n";
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
}
