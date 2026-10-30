#include "../runtime/paused_capture_lifecycle.h"
#include <cassert>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

struct Input { int point; bool select = true; };
struct Result { int packet, room; bool valid; };
using Lifecycle = RemasterPausedCaptureLifecycle<Input>;

// The dispatch adapter is the same contract used by mac-os.mm: inline on UI,
// enqueue from the producer. These schedules exercise executable scheduling,
// not source labels standing in for asynchronous cancellation.
static void advanceSchedules(bool commitOnly) {
	Lifecycle life;
	std::vector<std::function<void()>> ui;
	auto producerCancel = [&] {
		life.cancelOnUI([&](auto action) { ui.push_back(action); });
	};
	std::atomic<bool> advance{false};
	auto advanceRequest = [&] {
		life.requestAdvance(advance, [&](auto action){ui.push_back(action);});
	};
	int presented=0, completed=0;
	if (!commitOnly) {
	// Earlier general advance, producer consumes/exchanges then is preempted.
	life.request({1}); advanceRequest(); assert(advance.exchange(false));
	const auto newer=life.request({2});
	if (!ui.empty()) { ui.front()(); ui.clear(); } // delayed cancellation
	assert(life.produce([](const Input &i){return i.point;},
		[&](const Lifecycle::Request &r,int point) {
			assert(r.token==newer && point==2); ++completed;
			assert(life.deliver(r.token,true,[&](bool){presented=point;}));
		}));
	assert(completed==1 && presented==2);
	// Inverse order: cancellation on UI precedes delivery, so no stale frame
	// may enter presentation. A later capture remains deliverable after it.
	const auto invalidated=life.request({20}); advanceRequest();
	ui.front()(); ui.clear(); assert(advance.exchange(false));
	assert(!life.deliver(invalidated,true,[](bool){assert(false);}));
	// Current cancellation still rejects pending/queued work.
	const auto old=life.request({3}); producerCancel();
	if (!ui.empty()) { ui.front()(); ui.clear(); }
	assert(!life.produce([](const Input &){return 0;},[](const Lifecycle::Request &,int){assert(false);}));
	assert(!life.deliver(old,true,[](bool){assert(false);}));
	}
	// Producer requests an advance AFTER UI token-check, BEFORE presentation
	// commit. It can enqueue, but cannot mutate the UI-owned generation mid-commit.
	const auto token=life.request({4});
	assert(life.deliver(token,true,[&](bool) {
		std::thread producer(advanceRequest); producer.join();
		assert(life.load()==token); // cancellation has not linearized yet
		presented=4; // same non-modal commit boundary as trusted live presentation
	}));
	assert(presented==4 && advance.exchange(false));
	ui.front()(); ui.clear();
	assert(life.load()!=token);
	assert(!life.deliver(token,true,[&](bool){presented=99;}));
	assert(presented==4); // no stale installation AFTER invalidation
	// UI cancellation is synchronous, including before entering an external modal.
	const auto modal=life.request({5});
	life.cancelOnUI([](auto action){action();});
	assert(!life.deliver(modal,true,[](bool){assert(false);}));
	std::cout<<"exact advance/exchange/new request and token-check/commit schedules passed\n";
}

int main(int argc, char **) {
	advanceSchedules(argc>1);
	Lifecycle life;
	std::vector<std::function<void()>> callbacks;
	int frames=0, displayed=0, selected=0, room=0, unavailable=0;
	bool frameOK=true;
	std::function<void()> duringFrame;
	auto produce=[&] {
		return life.produce([&](const Input &) {
			const int fresh=++frames;
			if(duringFrame) { auto hook=std::move(duringFrame); duringFrame=nullptr; hook(); }
			return Result{fresh,fresh,frameOK};
		}, [&](const Lifecycle::Request &request, Result result) {
			callbacks.push_back([&,request,result] {
				life.deliver(request.token,result.valid,[&](bool valid) {
					if(!valid){++unavailable;return;}
					displayed=result.packet; room=result.room;
					if(request.payload.select)selected=request.payload.point;
				});
			});
		});
	};
	assert(!produce()); // producer before publication (old advance-before-pending race)
	life.request({11}); assert(produce()); callbacks.back()();
	assert(frames==1 && displayed==1 && room==1 && selected==11);
	callbacks.back()(); assert(displayed==1); // duplicate is not a new completion
	for(bool reverse:{false,true}) {
		life.request({21}); assert(produce()); auto first=callbacks.back();
		life.request({22}); assert(produce()); auto second=callbacks.back();
		if(reverse){second();first();}else{first();second();}
		assert(selected==22 && displayed==frames && room==frames);
	}
	// Replacement while the old producer is executing: never borrow new point/token.
	life.request({31}); duringFrame=[&]{life.request({32});}; assert(produce());
	const int before=displayed; callbacks.back()(); assert(displayed==before);
	assert(produce()); callbacks.back()(); assert(selected==32 && displayed==frames);
	// Explicit failed completion cannot reuse a previously completed packet/room.
	frameOK=false; life.request({40}); assert(produce()); callbacks.back()();
	assert(unavailable==1 && selected==32); frameOK=true;
	for(const std::string reason:{"valid import","invalid import","cancelled import",
		"recording open","invalid recording","cancelled recording","recording scrub",
		"layer refresh","resume","defrost","session","stop"}) {
		life.request({50}); assert(produce()); auto queued=callbacks.back();
		life.cancel(); displayed=-1;room=0;selected=0; queued();
		assert(displayed==-1 && room==0 && selected==0);
		life.request({51}); life.cancel(); assert(!produce());
		life.request({52}); duringFrame=[&]{life.cancel();}; assert(produce());
		callbacks.back()(); assert(displayed==-1 && selected==0);
		std::cout<<reason<<": queued/pending/in-flight rejected\n";
	}
	std::cout<<"paused capture production lifecycle passed; frames="<<frames<<" unavailable="<<unavailable<<'\n';
	// Real threads with a deterministic barrier at producer execution. Publishing
	// a replacement while the first frame runs must preserve both immutable inputs.
	std::atomic<int> stage{0};
	const auto firstToken=life.request({61});
	std::thread producer([&] {
		assert(life.produce([&](const Input &input) {
			assert(input.point==61); stage.store(1);
			while(stage.load()!=2)std::this_thread::yield();
			return 61;
		}, [&](const Lifecycle::Request &request, int value) {
			assert(request.token==firstToken && value==61);
			assert(!life.deliver(request.token,true,[](bool){assert(false);}));
		}));
	});
	while(stage.load()!=1)std::this_thread::yield();
	const auto secondToken=life.request({62}); stage.store(2); producer.join();
	assert(life.produce([](const Input &input){return input.point;},
		[&](const Lifecycle::Request &request,int value){
			assert(request.token==secondToken && value==62);
			assert(life.deliver(request.token,true,[](bool valid){assert(valid);}));
		}));
	std::cout<<"threaded in-flight replacement passed\n";
}
