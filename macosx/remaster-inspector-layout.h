#pragma once
#import <Cocoa/Cocoa.h>

// Fixed-coordinate inspector canvas; keep the post-placement migration shared
// with the AppKit regression (saved frames can predate the context status row).
static inline NSSize S9xInspectorCanvasSize() { return NSMakeSize(760, 794); }
static inline NSRect S9xInspectorStatusFrame() { return NSMakeRect(20, 672, 720, 112); }
static inline NSRect S9xInspectorDetailsFrame() { return NSMakeRect(244, 20, 496, 170); }

static inline void S9xInspectorSetContextHidden(NSArray<NSView *> *views, BOOL grouping)
{
	for (NSView *view in views) view.hidden = grouping;
}

static inline void S9xInspectorRestoreLayout(NSPanel *panel, NSView *canvas,
	NSScrollView *details, NSRect visibleFrame)
{
	const NSPoint topLeft = NSMakePoint(NSMinX(panel.frame), NSMaxY(panel.frame));
	const NSSize fullSize = S9xInspectorCanvasSize();
	const NSRect fullFrame = [panel frameRectForContentRect:NSMakeRect(0, 0, fullSize.width, fullSize.height)];
	const CGFloat chromeHeight = NSHeight(fullFrame) - fullSize.height;
	const NSSize fitSize = NSMakeSize(MIN(fullSize.width, NSWidth(visibleFrame)),
		MIN(fullSize.height, NSHeight(visibleFrame) - chromeHeight));
	const BOOL needsScroll = fitSize.width < fullSize.width || fitSize.height < fullSize.height;
	if (needsScroll && panel.contentView == canvas) {
		NSScrollView *viewport = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, fitSize.width, fitSize.height)];
		viewport.hasVerticalScroller = YES;
		viewport.hasHorizontalScroller = YES;
		viewport.autohidesScrollers = YES;
		viewport.borderType = NSNoBorder;
		panel.contentView = viewport;
		viewport.documentView = canvas;
	} else if (!needsScroll && panel.contentView != canvas) {
		[(NSScrollView *)panel.contentView setDocumentView:nil];
		panel.contentView = canvas;
	}
	panel.contentMinSize = fitSize;
	[panel setContentSize:fitSize];
	// AppKit has already resized flexible descendants during restoration and
	// content-view replacement. Reset the fixed canvas and details view last.
	canvas.autoresizingMask = NSViewNotSizable;
	canvas.frame = NSMakeRect(0, 0, fullSize.width, fullSize.height);
	canvas.bounds = canvas.frame;
	details.frame = S9xInspectorDetailsFrame();
	NSRect frame = panel.frame;
	frame.origin.x = MAX(NSMinX(visibleFrame), MIN(topLeft.x, NSMaxX(visibleFrame) - NSWidth(frame)));
	frame.origin.y = MAX(NSMinY(visibleFrame), MIN(topLeft.y - NSHeight(frame), NSMaxY(visibleFrame) - NSHeight(frame)));
	[panel setFrame:frame display:NO];
	if (needsScroll) [canvas scrollRectToVisible:S9xInspectorStatusFrame()];
}

static inline NSView *S9xInspectorCanvas(NSPanel *panel)
{
	return [panel.contentView isKindOfClass:[NSScrollView class]] ?
		[(NSScrollView *)panel.contentView documentView] : panel.contentView;
}
