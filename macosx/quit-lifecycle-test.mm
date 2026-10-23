#import <Cocoa/Cocoa.h>
#include <dlfcn.h>

@interface S9xView : NSView @end

@interface S9xEngine : NSObject
- (BOOL)loadROM:(NSURL *)url;
- (NSString *)loadRemasterProfile:(NSURL *)url;
- (void)pause;
- (void)quit;
- (BOOL)isRunning;
@end

int main(int argc, const char **argv)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        NSString *root = NSFileManager.defaultManager.currentDirectoryPath;
        NSString *framework = [root stringByAppendingPathComponent:
            @"build/release/Snes9x.app/Contents/Frameworks/snes9x_framework.framework"];
        if (![[NSBundle bundleWithPath:framework] load]) return 1;
        S9xEngine *engine = [[NSClassFromString(@"S9xEngine") alloc] init];
        if (!engine) return 2;
        // Resolve the framework's view without linking the framework at startup.
        S9xView *__unsafe_unretained *view = (S9xView *__unsafe_unretained *)dlsym(RTLD_DEFAULT, "s9xView");
        if (!view || !*view) return 6;
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 512, 448)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        window.contentView = *view;
        [window makeKeyAndOrderFront:nil];
        NSString *mode = argc > 1 ? @(argv[1]) : @"empty";
        if (![mode isEqualToString:@"empty"]) {
            NSURL *rom = [NSURL fileURLWithPath:[NSHomeDirectory()
                stringByAppendingPathComponent:@"Downloads/Zelda_ALTTP.zip"]];
            if (![engine loadROM:rom]) return 3;
            if (![mode isEqualToString:@"immediate"]) {
                NSString *error = [engine loadRemasterProfile:[NSURL fileURLWithPath:
                    [root stringByAppendingPathComponent:@"remaster/profiles/ALttP/alttp-profile.toml"]]];
                if (error) { NSLog(@"%@", error); return 4; }
                NSDate *until = [NSDate dateWithTimeIntervalSinceNow:1.0];
                while (until.timeIntervalSinceNow > 0)
                    [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
                if ([mode isEqualToString:@"paused"]) [engine pause];
            }
        }
        [engine quit];
        [engine quit]; // AppDelegate can invoke both stop/quit during termination.
        if ([engine isRunning]) return 5;
        if ([mode isEqualToString:@"restart"]) {
            NSURL *rom = [NSURL fileURLWithPath:[NSHomeDirectory()
                stringByAppendingPathComponent:@"Downloads/Zelda_ALTTP.zip"]];
            if (![engine loadROM:rom]) return 7;
            [engine quit];
            if ([engine isRunning]) return 8;
        }
        NSLog(@"Quit lifecycle smoke passed: %@", mode);
    }
    return 0; // Exercise actual framework static destruction, not _exit().
}
