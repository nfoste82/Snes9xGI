#define S9X_REMASTER_PROFILING 0
#include "../runtime/live_trace.h"
#include <cassert>

int main()
{
	using namespace RemasterLightingTrace;
	setenv("S9X_REMASTER_LIGHTING_TRACE", "unused", 1);
	setenv("S9X_REMASTER_LIVE_TRACE", "must-not-write", 1);
	assert(!enabled() && !legacyEnabled() && !RemasterLiveTrace::available());
	Frame frame; frame.id = 1;
	{ Scope outer(&frame, EndFrame); Scope inner(&frame, Solve); assert(!activeScope()); }
	for (auto calls : frame.calls) assert(calls == 0);
	for (auto ms : frame.exclusive) assert(ms == 0);
	RemasterLiveTrace::request("must-not-write", 1);
	assert(!boundedActive().load());
	RemasterLiveTrace::beginRefresh(1);
	assert(!RemasterLiveTrace::refreshFrame().id && !RemasterLiveTrace::pacingActive());
}
