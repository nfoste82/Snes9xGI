// Remember the physical display, not just coordinates that may overlap a
// different monitor after disconnection or rearrangement.
#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include <math.h>

static NSString *S9xDisplayIdentifier(NSScreen *screen)
{
	NSNumber *number = screen.deviceDescription[@"NSScreenNumber"];
	if (!number) return nil;
	CFUUIDRef uuid = CGDisplayCreateUUIDFromDisplayID(number.unsignedIntValue);
	if (!uuid) return nil;
	NSString *identifier = CFBridgingRelease(CFUUIDCreateString(NULL, uuid));
	CFRelease(uuid);
	return identifier;
}

static void S9xSaveWindowPlacement(NSWindow *window, NSString *name)
{
	if (!window.screen || (window.styleMask & NSWindowStyleMaskFullScreen)) return;
	NSString *display = S9xDisplayIdentifier(window.screen);
	if (!display) return;
	[NSUserDefaults.standardUserDefaults setObject:@{@"frame": NSStringFromRect(window.frame),
		@"screenFrame": NSStringFromRect(window.screen.frame), @"display": display}
		forKey:[@"S9xWindowPlacement." stringByAppendingString:name]];
}

static BOOL S9xRestoreWindowPlacement(NSWindow *window, NSString *name)
{
	NSDictionary *saved = [NSUserDefaults.standardUserDefaults dictionaryForKey:
		[@"S9xWindowPlacement." stringByAppendingString:name]];
	if (![saved[@"frame"] isKindOfClass:NSString.class] ||
		![saved[@"screenFrame"] isKindOfClass:NSString.class] ||
		![saved[@"display"] isKindOfClass:NSString.class]) return NO;
	NSRect frame = NSRectFromString(saved[@"frame"]);
	NSRect oldScreen = NSRectFromString(saved[@"screenFrame"]);
	if (!isfinite(frame.origin.x) || !isfinite(frame.origin.y) ||
		!isfinite(frame.size.width) || !isfinite(frame.size.height) ||
		!isfinite(oldScreen.origin.x) || !isfinite(oldScreen.origin.y) ||
		frame.size.width <= 0 || frame.size.height <= 0) return NO;
	for (NSScreen *screen in NSScreen.screens)
	{
		if (![S9xDisplayIdentifier(screen) isEqual:saved[@"display"]]) continue;
		frame.origin.x += NSMinX(screen.frame) - NSMinX(oldScreen);
		frame.origin.y += NSMinY(screen.frame) - NSMinY(oldScreen);
		NSRect visible = screen.visibleFrame;
		// Keep the title bar accessible after resolution/menu-bar/Dock changes.
		frame.origin.x = MAX(NSMinX(visible), MIN(frame.origin.x, NSMaxX(visible) - MIN(NSWidth(frame), NSWidth(visible))));
		frame.origin.y = MIN(frame.origin.y, NSMaxY(visible) - NSHeight(frame));
		frame.origin.y = MAX(frame.origin.y, NSMinY(visible) - MAX(0, NSHeight(frame) - NSHeight(visible)));
		[window setFrame:frame display:NO];
		return YES;
	}
	return NO;
}

static void S9xInitializeWindowPlacement(NSWindow *window, NSString *name)
{
	if (!S9xRestoreWindowPlacement(window, name)) [window center];
	__weak NSWindow *weakWindow = window;
	for (NSNotificationName event in @[NSWindowDidMoveNotification, NSWindowDidResizeNotification])
		[NSNotificationCenter.defaultCenter addObserverForName:event object:window queue:nil
			usingBlock:^(NSNotification *notification) {
				NSWindow *strongWindow = weakWindow;
				if (strongWindow) S9xSaveWindowPlacement(strongWindow, name);
			}];
}
