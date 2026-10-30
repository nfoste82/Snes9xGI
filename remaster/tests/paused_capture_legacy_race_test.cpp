// Deterministic counterexample to the startup mac-os.mm scheduler. This is a
// retained baseline witness, NOT an alternative implementation or production
// gate. Exit 1 means the independently required exactly-one-fresh-frame oracle
// rejects the old advance-before-pending publication ordering.
#include <iostream>

int main()
{
	bool advance=false, pending=false, evidenceRequested=false;
	int frames=0, capturedFrame=0, evidenceFrame=0;
	auto producer=[&] {
		if(advance) {
			advance=false;
			++frames;
			if(evidenceRequested) { evidenceFrame=frames; evidenceRequested=false; }
		}
		if(pending) { pending=false; capturedFrame=frames; }
	};
	// Old UI publishes the advance before pending; the producer does not acquire
	// the request mutex unless it observes pending, so this schedule is permitted.
	evidenceRequested=true;
	advance=true;
	producer();
	pending=true;
	if(frames!=1 || capturedFrame!=1 || pending) {
		std::cerr<<"EXPECTED BASELINE FAILURE: frames="<<frames<<" captured="<<capturedFrame
			<<" evidence="<<evidenceFrame<<" pending="<<pending
			<<"; request was not completed on its fresh frame\n";
		return 1;
	}
	return 0;
}
