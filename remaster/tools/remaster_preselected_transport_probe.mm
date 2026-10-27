// Same standalone production-shader replay as the reference probe, but consume
// the field pass's exact donors just as the production renderer now does.
#include "../runtime/emitter_patches.h"
#include "../runtime/hidden_emitter_samples.h"
#define S9xRemasterEmitterPatches(...) S9xRemasterEmitterPatches(__VA_ARGS__, &fields.channelSources)
#include "remaster_stair_transport_probe.mm"
