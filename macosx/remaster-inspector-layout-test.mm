// clang++ -std=c++14 -fobjc-arc -framework Cocoa macosx/remaster-inspector-layout-test.mm -o <test>
// Creates only hidden test panels: never activates an app or reads user defaults.
#import "remaster-inspector-layout.h"
#include <cstdio>

static int failures = 0, checks = 0;
static void check(bool value, const char *message)
{
	++checks;
	if (!value) { ++failures; fprintf(stderr, "FAIL: %s\n", message); }
}

static void exercise(NSSize saved, NSRect screen)
{
	NSPanel *panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(100, 100, 760, 724)
		styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow
		backing:NSBackingStoreBuffered defer:NO];
	panel.releasedWhenClosed = NO;
	NSView *canvas = panel.contentView;
	NSTextField *status = [NSTextField wrappingLabelWithString:@""];
	status.frame = S9xInspectorStatusFrame();
	status.font = [NSFont systemFontOfSize:11];
	[canvas addSubview:status];
	NSButton *assign = [[NSButton alloc] initWithFrame:NSMakeRect(210, 604, 170, 28)];
	assign.title = @"Assign Context";
	[canvas addSubview:assign];
	NSPopUpButton *scope=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(20,638,300,26) pullsDown:NO];
	[canvas addSubview:scope];
	NSPopUpButton *include=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(330,638,200,26) pullsDown:YES];
	[canvas addSubview:include];
	NSPopUpButton *exclude=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(540,638,200,26) pullsDown:YES];
	[canvas addSubview:exclude];
	NSScrollView *details = [[NSScrollView alloc] initWithFrame:S9xInspectorDetailsFrame()];
	details.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
	[canvas addSubview:details];
	[panel setContentSize:saved]; // Actual AppKit autoresizing during saved placement restoration.
	const NSPoint restoredTopLeft = NSMakePoint(NSMinX(panel.frame), NSMaxY(panel.frame));
	for (int reopen = 0; reopen < 2; ++reopen) {
		S9xInspectorRestoreLayout(panel, canvas, details, screen);
		check(NSEqualSizes(canvas.bounds.size, S9xInspectorCanvasSize()), "full canvas survives restore/reopen");
		check(NSContainsRect(canvas.bounds, status.frame), "status including binding count is not clipped");
		check(!NSIntersectsRect(status.frame, assign.frame), "status clears context controls");
		check(!NSIntersectsRect(status.frame, scope.frame) && !NSIntersectsRect(include.frame,exclude.frame), "scope/filter row nonoverlapping");
		check(NSContainsRect(canvas.bounds, exclude.frame), "filters within full canvas");
		check(NSEqualRects(details.frame, S9xInspectorDetailsFrame()), "flexible lower scroll geometry reset");
		check(NSMaxY(details.frame) < 197, "lower scroll clears shift controls");
		check(NSContainsRect(screen, panel.frame), "window fits available screen");
		if (NSWidth(screen) > 1000 && NSHeight(screen) > 1000)
			check(NSMinX(panel.frame) == restoredTopLeft.x && NSMaxY(panel.frame) == restoredTopLeft.y,
				"restore preserves top-left placement when it fits");
		for (int mode = 0; mode < 3; ++mode) {
			S9xInspectorSetContextHidden(@[status, assign], mode == 2);
			check(status.hidden == (mode == 2) && assign.hidden == (mode == 2), "type/location visible; grouping hidden");
			if (mode == 2) continue;
			status.stringValue = [NSString stringWithFormat:
				@"Edit / preview: None (default asset; Assign clears bindings)\n"
				 "Picked occurrence: None (default asset)\n"
				 "%@: 4096 distinct bindings; 4096 visible unavailable excluded. Hidden/offscreen not included.\n"
				 "Replacements: 4096. Choose context then Assign. None cannot disable sole auto.",
				 mode == 0 ? @"Visible/current frame" : @"Picked locations"];
			NSSize measured = [status.cell cellSizeForBounds:NSMakeRect(0, 0, NSWidth(status.frame), 10000)];
			check(measured.height <= NSHeight(status.frame), "four-line count text fits using actual AppKit font metrics");
		}
		if (panel.contentView != canvas) {
			NSScrollView *viewport = (NSScrollView *)panel.contentView;
			check([viewport isKindOfClass:[NSScrollView class]], "small screen uses scrolling, not clipped controls");
			check(viewport.hasVerticalScroller && viewport.hasHorizontalScroller, "both axes accessible on small screens");
			[canvas scrollRectToVisible:status.frame];
			check(NSContainsRect(canvas.visibleRect, status.frame), "small-screen status reachable");
			[canvas scrollRectToVisible:NSMakeRect(20, 8, 210, 30)];
			check(NSContainsRect(canvas.visibleRect, NSMakeRect(20, 8, 210, 30)), "small-screen bottom controls reachable");
			[canvas scrollRectToVisible:assign.frame];
			check(NSContainsRect(canvas.visibleRect, assign.frame), "small-screen assignment reachable");
		}
		[panel orderOut:nil]; // Reuse the same nonresizable panel on the next pass.
	}
	[panel close];
}

int main()
{
	@autoreleasepool {
		[NSApplication sharedApplication];
		for (NSValue *size in @[[NSValue valueWithSize:NSMakeSize(760,724)],
			[NSValue valueWithSize:NSMakeSize(760,646)], [NSValue valueWithSize:NSMakeSize(900,900)]]) {
			exercise(size.sizeValue, NSMakeRect(0, 0, 1600, 1400));
			exercise(size.sizeValue, NSMakeRect(0, 0, 800, 650));
			exercise(size.sizeValue, NSMakeRect(0, 0, 720, 600));
		}
	}
	printf("Inspector AppKit layout: %d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
