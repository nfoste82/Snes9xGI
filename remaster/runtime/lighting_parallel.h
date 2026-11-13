#ifndef REMASTER_LIGHTING_PARALLEL_H
#define REMASTER_LIGHTING_PARALLEL_H

#include <cstddef>
#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

// Synchronous completion is required: callers own the input snapshot and each
// worker writes a disjoint output range. No work escapes the update invocation.
template<class Function>
inline void S9xRemasterLightingParallel(size_t count, Function &function)
{
#ifdef __APPLE__
	dispatch_apply_f(count, dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), &function,
		[](void *context, size_t worker) { (*static_cast<Function *>(context))(worker); });
#else
	for (size_t worker = 0; worker < count; ++worker) function(worker);
#endif
}
#endif
