#ifndef _REMASTER_DUNGEON_BUILD_WORK_H_
#define _REMASTER_DUNGEON_BUILD_WORK_H_
#include <cstdint>
// Standalone diagnostics only. No clocks, counters or state in app builds.
#ifdef ALTTP_TEST_BUILD_WORK
#include <chrono>
#include <cstdio>
struct RemasterDungeonBuildWork {
	double ms[16]={}; uint64_t calls[16]={},allocations[16]={},bytes[16]={};
	int active=0;
	uint64_t decodedTiles=0,normalSamples=0,movementNodes=0,movementHits=0,graphNodes=0,graphEdges=0;
};
inline RemasterDungeonBuildWork &S9xRemasterDungeonBuildWork(){static RemasterDungeonBuildWork w;return w;}
struct RemasterDungeonBuildPhase {
	using Clock=std::chrono::steady_clock;
	int previous,phase;Clock::time_point start;
	explicit RemasterDungeonBuildPhase(int p):previous(S9xRemasterDungeonBuildWork().active),phase(p),start(Clock::now()) {auto &w=S9xRemasterDungeonBuildWork();w.active=p;w.calls[p]++;}
	void next(int p){auto &w=S9xRemasterDungeonBuildWork();w.ms[phase]+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();phase=p;w.active=p;w.calls[p]++;start=Clock::now();}
	~RemasterDungeonBuildPhase(){auto &w=S9xRemasterDungeonBuildWork();w.ms[phase]+=std::chrono::duration<double,std::milli>(Clock::now()-start).count();w.active=previous;}
};
#define S9X_BUILD_PHASE(p) RemasterDungeonBuildPhase buildPhase(p)
#define S9X_BUILD_NEXT(p) buildPhase.next(p)
#define S9X_BUILD_WORK(field,n) (S9xRemasterDungeonBuildWork().field+=(n))
#else
#define S9X_BUILD_PHASE(p) ((void)0)
#define S9X_BUILD_NEXT(p) ((void)0)
#define S9X_BUILD_WORK(field,n) ((void)0)
#endif
#ifdef ALTTP_TEST_CORNER_WORK
inline uint64_t &S9xRemasterDungeonCornerOrigins(){static uint64_t n=0;return n;}
#define S9X_CORNER_ORIGIN() (++S9xRemasterDungeonCornerOrigins())
#else
#define S9X_CORNER_ORIGIN() ((void)0)
#endif
#endif
