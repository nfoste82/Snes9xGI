#import <Cocoa/Cocoa.h>
#include <dlfcn.h>
#include <cstdlib>

@interface S9xEngine : NSObject
- (NSString *)restorePreviousWorkspace;
- (void)savePreviousWorkspace;
- (void)showRemasterProfileSettings;
- (void)showRemasterAreaSettings;
- (BOOL)isPaused;
- (void)quit;
@end

static NSWindow *FindWindow(NSString *title)
{
    for (NSWindow *window in NSApp.windows)
        if ([window.title isEqualToString:title]) return window;
    return nil;
}

static void Check(BOOL condition, NSString *message)
{
    if (!condition) {
        NSLog(@"FAIL: %@", message);
        std::exit(1);
    }
}

int main(int argc, const char **argv)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        NSString *root = NSFileManager.defaultManager.currentDirectoryPath;
        NSString *configuration = argc > 1 ? @(argv[1]) : @"debug";
        NSString *framework = [root stringByAppendingPathComponent:[NSString stringWithFormat:
            @"build/%@/Snes9x.app/Contents/Frameworks/snes9x_framework.framework", configuration]];
        Check([[NSBundle bundleWithPath:framework] load], @"Load built framework");
        S9xEngine *engine = [[NSClassFromString(@"S9xEngine") alloc] init];
        NSView *__unsafe_unretained *view = (NSView *__unsafe_unretained *)dlsym(RTLD_DEFAULT, "s9xView");
        Check(engine && view && *view, @"Create engine and view");
        NSWindow *game = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 512, 448)
            styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:NO];
        game.contentView = *view;
        [game makeKeyAndOrderFront:nil];

        NSUserDefaults *defaults = NSUserDefaults.standardUserDefaults;
        NSString *key = @"S9xPreviousWorkspace";
        id previous = [defaults objectForKey:key];
        NSString *profile = [root stringByAppendingPathComponent:@"remaster/profiles/ALttP/alttp-profile.toml"];
        NSMutableDictionary *saved = [@{
            @"games": @[[NSHomeDirectory() stringByAppendingPathComponent:@"Downloads/Zelda_ALTTP.zip"]],
            @"profile": profile, @"paused": @YES, @"sceneControls": @YES,
            @"npcPoses": @NO, @"areaSettings": @YES
        } mutableCopy];
        @try {
            [defaults setObject:saved forKey:key];
            NSString *error = [engine restorePreviousWorkspace];
            Check(!error.length, error ?: @"Restore workspace");
            Check(FindWindow(@"Profile Controls").visible, @"Restore Profile Controls");
            Check([[defaults dictionaryForKey:key][@"profile"] isEqualToString:profile], @"Retain matching profile path");
            NSWindow *area = FindWindow(@"Area Settings");
            Check(area.visible, @"Restore Area Settings");
            NSDate *until = [NSDate dateWithTimeIntervalSinceNow:0.3];
            while (until.timeIntervalSinceNow > 0)
                [NSRunLoop.currentRunLoop runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.01]];
            Check([engine isPaused], @"Restore stays paused after startup");

            [area close];
            Check(![[defaults dictionaryForKey:key][@"areaSettings"] boolValue], @"Closing Area Settings is saved");
            [engine showRemasterAreaSettings];
            Check([[defaults dictionaryForKey:key][@"areaSettings"] boolValue], @"Opening Area Settings is saved");
            [FindWindow(@"Profile Controls") close];
            Check(![[defaults dictionaryForKey:key][@"sceneControls"] boolValue], @"Closing Profile Controls is saved");

            // Loading a profile normally opens controls: restore must still honor
            // explicit closed flags, including windows already open in this process.
            saved[@"sceneControls"] = @NO;
            saved[@"areaSettings"] = @NO;
            [defaults setObject:saved forKey:key];
            error = [engine restorePreviousWorkspace];
            Check(!error.length, error ?: @"Restore closed panels");
            Check(!FindWindow(@"Profile Controls").visible && !area.visible, @"Closed panels stay closed");

            // A failed profile must not expose stale controls or erase retry intent.
            saved[@"profile"] = [profile stringByAppendingString:@".missing"];
            saved[@"sceneControls"] = @YES;
            saved[@"areaSettings"] = @YES;
            [defaults setObject:saved forKey:key];
            error = [engine restorePreviousWorkspace];
            Check(error.length > 0, @"Report missing profile");
            Check(!FindWindow(@"Profile Controls").visible && !area.visible, @"No stale controls after profile failure");
            Check([[defaults dictionaryForKey:key][@"areaSettings"] boolValue], @"Keep panel intent for retry");

            // Legacy records have no areaSettings key; do not open it implicitly.
            saved[@"profile"] = profile;
            saved[@"sceneControls"] = @YES;
            [saved removeObjectForKey:@"areaSettings"];
            [defaults setObject:saved forKey:key];
            error = [engine restorePreviousWorkspace];
            Check(!error.length, error ?: @"Restore legacy workspace");
            Check(FindWindow(@"Profile Controls").visible && !area.visible, @"Legacy window visibility");
        } @finally {
            [engine quit];
            if (previous) [defaults setObject:previous forKey:key];
            else [defaults removeObjectForKey:key];
        }
        NSLog(@"Workspace window restoration checks passed");
    }
    return 0;
}
