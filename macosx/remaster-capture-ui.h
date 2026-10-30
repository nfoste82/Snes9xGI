#pragma once
#import <Foundation/Foundation.h>
#include <functional>

// Presentation and invalidation share one owner; never block the producer on
// UI (stop may join it). UI callers cancel inline, before external modal entry.
inline void DispatchRemasterCaptureUI(std::function<void()> action)
{
	if ([NSThread isMainThread]) action();
	else dispatch_async(dispatch_get_main_queue(), ^{ action(); });
}
