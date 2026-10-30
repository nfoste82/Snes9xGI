// No emulator, windows, defaults or profile access. Exercise the production
// dispatch adapter on the actual main queue with a manually pumped run loop.
#import "remaster-capture-ui.h"
#include "../remaster/runtime/paused_capture_lifecycle.h"
#include <cassert>
#include <thread>
#include <cstdio>

int main() {
	@autoreleasepool {
		RemasterPausedCaptureLifecycle<int> life;
		std::atomic<bool> advance{false};
		auto requestAdvance=[&] { life.requestAdvance(advance, DispatchRemasterCaptureUI); };
		auto drain=[] {
			__block bool done=false;
			dispatch_async(dispatch_get_main_queue(), ^{ done=true; });
			while (!done) CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.01,true);
		};
		life.request(1);
		std::thread older(requestAdvance); older.join();
		assert(advance.exchange(false)); // consume, then preempt before UI drain
		const auto newer=life.request(2);
		drain();
		assert(life.load()==newer);
		assert(life.produce([](int point){return point;},[&](const auto &r,int point) {
			assert(point==2 && r.token==newer);
		}));
		int installed=0;
		assert(life.deliver(newer,true,[&](bool) {
			std::thread duringCommit(requestAdvance); duringCommit.join();
			assert(life.load()==newer);
			installed=2;
		}));
		drain();
		assert(life.load()!=newer && installed==2);
		assert(!life.deliver(newer,true,[&](bool){installed=99;}));
		const auto current=life.request(3);
		life.cancelOnUI(DispatchRemasterCaptureUI); // inline before modal
		assert(life.load()!=current && installed==2);
		puts("native main-queue exact lifecycle schedules passed (no GUI)");
	}
}
