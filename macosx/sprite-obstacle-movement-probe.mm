// Native benchmark harness without AppDelegate workspace/window restoration.
#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <cstdlib>
static void noEditorRefresh(id,SEL) {}
@interface S9xEngine : NSObject
- (void)startRemasterBenchmarkIfRequested;
@end
int main(int argc,const char **argv) { @autoreleasepool {
	[NSApplication sharedApplication];
	NSString *app=getenv("S9X_REMASTER_MOVEMENT_APP")?@(getenv("S9X_REMASTER_MOVEMENT_APP")):@"build/debug/Snes9x.app";
	NSString *framework=[[NSFileManager.defaultManager.currentDirectoryPath stringByAppendingPathComponent:app]
		stringByAppendingPathComponent:@"Contents/Frameworks/snes9x_framework.framework"];
	if(![[NSBundle bundleWithPath:framework] load])return 1;
	// This headless driver has no inspector controls. Runtime/frame paths remain production.
	method_setImplementation(class_getInstanceMethod(NSClassFromString(@"S9xEngine"),NSSelectorFromString(@"refreshRemasterEditingControls")),(IMP)noEditorRefresh);
	id engine=[[NSClassFromString(@"S9xEngine") alloc] init];
	NSView *__unsafe_unretained *view=(NSView *__unsafe_unretained *)dlsym(RTLD_DEFAULT,"s9xView");
	if(!view || !*view)return 2;
	NSWindow *window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,512,448)
		styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
	window.contentView=*view;[window makeKeyAndOrderFront:nil];
	[engine startRemasterBenchmarkIfRequested];
	[NSApp run];return 3;
} }
