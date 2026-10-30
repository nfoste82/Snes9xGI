#pragma once

#include <cstdint>
#include <atomic>
#include <memory>
#include <mutex>
#include <utility>

// Request publication, producer take, and UI completion are one token lifecycle.
// No packet/evidence is borrowed across threads; the caller's completion owns it.
template<class Payload> class RemasterPausedCaptureLifecycle
{
public:
	struct Request { uint64_t token; Payload payload; };
	uint64_t load() const { std::lock_guard<std::mutex> lock(mutex); return generation; }
	// dispatch must execute inline on the UI owner, otherwise enqueue there.
	// Snapshot at the invalidating event, NOT when a later producer consumes it.
	// A delayed event must never cancel a request published after that event.
	template<class Dispatch> void cancelOnUI(Dispatch dispatch) {
		const auto observed = load();
		dispatch([this, observed] {
			std::lock_guard<std::mutex> lock(mutex);
			if (generation != observed) return;
			++generation; pending.reset(); delivered = false;
		});
	}
	template<class Dispatch> void requestAdvance(std::atomic<bool> &advance, Dispatch dispatch) {
		cancelOnUI(std::move(dispatch));
		advance.store(true);
	}
	// Direct cancellation and request publication are UI-owner-only. Producer
	// callers must use cancelOnUI. Thus deliver's check/apply cannot be split by
	// producer cancellation; no mutex is held over UI callbacks or modal loops.
	void cancel() {
		std::lock_guard<std::mutex> lock(mutex);
		++generation; pending.reset(); delivered = false;
	}
	uint64_t request(Payload payload) {
		std::lock_guard<std::mutex> lock(mutex);
		const auto token = ++generation;
		pending = std::make_shared<const Request>(Request{token, std::move(payload)});
		delivered = false;
		return token;
	}
	// Called only by the paused producer, BEFORE arming/running the fresh frame.
	// A replacement during run cannot change this immutable request's payload.
	template<class Run, class Complete> bool produce(Run run, Complete complete) {
		std::shared_ptr<const Request> taken;
		{ std::lock_guard<std::mutex> lock(mutex); taken.swap(pending); }
		if (!taken) return false;
		auto result = run(taken->payload);
		complete(*taken, std::move(result));
		return true;
	}
	// Main/UI queue only: cancellation and UI replacement also run on that queue.
	// Do not hold the lock over UI work (which can enter a nested modal loop).
	template<class Apply> bool deliver(uint64_t token, bool available, Apply apply) {
		{
			std::lock_guard<std::mutex> lock(mutex);
			if (token != generation || delivered) return false;
			delivered = true;
		}
		apply(available);
		return true;
	}
private:
	mutable std::mutex mutex;
	uint64_t generation = 0;
	bool delivered = false;
	std::shared_ptr<const Request> pending;
};
