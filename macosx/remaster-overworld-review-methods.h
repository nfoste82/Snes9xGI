// Foundation-only bundle validation is also usable by a command-line checker.
#ifndef S9X_AREA_REVIEW_VALIDATION
#define S9X_AREA_REVIEW_VALIDATION
#import <Foundation/Foundation.h>
#import <math.h>

static BOOL S9xAreaReviewString(id value)
{
	return [value isKindOfClass:NSString.class] && [value length] > 0;
}

static NSURL *S9xAreaReviewURL(NSURL *manifest, NSString *path)
{
	if (path.isAbsolutePath) return [NSURL fileURLWithPath:path];
	return [[manifest URLByDeletingLastPathComponent] URLByAppendingPathComponent:path];
}

static NSDictionary *S9xReadAreaReview(NSURL *url, NSString **error)
{
	NSError *readError = nil;
	NSData *data = [NSData dataWithContentsOfURL:url options:0 error:&readError];
	id root = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:&readError] : nil;
	if (![root isKindOfClass:NSDictionary.class])
	{
		*error = readError.localizedDescription ?: @"The manifest must be a JSON object.";
		return nil;
	}
	if (![root[@"format"] isEqual:@"S9XAREA-REVIEW1"])
	{
		*error = @"Expected an S9XAREA-REVIEW1 manifest.";
		return nil;
	}
	for (NSString *key in @[@"title", @"status", @"html", @"path"])
		if (!S9xAreaReviewString(root[key]))
		{
			*error = [NSString stringWithFormat:@"Missing or invalid manifest field: %@.", key];
			return nil;
		}
	if (![root[@"captures"] isKindOfClass:NSArray.class])
	{
		*error = @"The manifest must contain a captures array (which may be empty).";
		return nil;
	}
	NSUInteger index = 0;
	for (id capture in root[@"captures"])
	{
		++index;
		BOOL valid = [capture isKindOfClass:NSDictionary.class];
		if (valid)
		{
			valid = S9xAreaReviewString(capture[@"title"]) && S9xAreaReviewString(capture[@"path"]) &&
				[[capture[@"path"] pathExtension] caseInsensitiveCompare:@"s9xrmf"] == NSOrderedSame &&
				(capture[@"provenance"] != nil && capture[@"provenance"] != NSNull.null);
			id origin = capture[@"origin"];
			valid = valid && [origin isKindOfClass:NSArray.class] && [origin count] == 2;
			if (valid)
				for (id n in origin)
					valid = valid && [n isKindOfClass:NSNumber.class] && isfinite([n doubleValue]);
			for (NSString *key in @[@"matched_pixels", @"mismatched_pixels", @"solved_pixels", @"unresolved_pixels"])
			{
				id n = capture[key];
				valid = valid && [n isKindOfClass:NSNumber.class] && isfinite([n doubleValue]) &&
					[n doubleValue] >= 0 && floor([n doubleValue]) == [n doubleValue];
			}
		}
		if (!valid)
		{
			*error = [NSString stringWithFormat:@"Capture %lu has invalid title/path, origin, provenance, or pixel counts.", (unsigned long)index];
			return nil;
		}
	}
	return root;
}
#endif

#ifdef S9X_AREA_REVIEW_METHODS
// Included inside S9xEngine's implementation. Review packets stay independent
// of the profile editor; ordinary Open Remaster Frame keeps its existing behavior.
- (void)areaReviewError:(NSString *)message
{
	if (!message.length) return;
	NSAlert *alert = [NSAlert new];
	alert.messageText = @"Unable to Open Area Review";
	alert.informativeText = message;
	[alert runModal];
}

- (void)chooseRemasterAreaReview
{
	NSOpenPanel *panel = [NSOpenPanel openPanel];
	panel.title = @"Open Area Review";
	panel.allowedFileTypes = @[@"json"];
	panel.canChooseDirectories = NO;
	panel.allowsMultipleSelection = NO;
	NSString *last = [NSUserDefaults.standardUserDefaults stringForKey:@"S9xLastAreaReview"];
	if (last.length) panel.directoryURL = [[NSURL fileURLWithPath:last] URLByDeletingLastPathComponent];
	if ([panel runModal] == NSModalResponseOK)
		[self areaReviewError:[self openRemasterAreaReview:panel.URL]];
}

- (BOOL)areaReviewCaptureAvailable:(NSInteger)index
{
	NSArray *captures = self.areaReviewManifest[@"captures"];
	if (index < 0 || index >= (NSInteger)captures.count) return NO;
	NSURL *url = S9xAreaReviewURL(self.areaReviewURL, captures[index][@"path"]);
	BOOL directory = NO;
	return [NSFileManager.defaultManager fileExistsAtPath:url.path isDirectory:&directory] && !directory;
}

- (NSInteger)areaReviewNeighbor:(NSInteger)direction
{
	NSInteger count = [self.areaReviewManifest[@"captures"] count];
	for (NSInteger i = self.areaReviewIndex + direction; i >= 0 && i < count; i += direction)
		if ([self areaReviewCaptureAvailable:i]) return i;
	return -1;
}

- (void)refreshAreaReview
{
	NSArray *captures = self.areaReviewManifest[@"captures"];
	NSUInteger available = 0;
	for (NSInteger i = 0; i < (NSInteger)captures.count; ++i)
		if ([self areaReviewCaptureAvailable:i]) ++available;
	self.areaReviewStatus.stringValue = [NSString stringWithFormat:@"INCOMPLETE AREA REVIEW — %@\n%lu of %lu captures available. Captures do not establish complete-area coverage.",
		self.areaReviewManifest[@"status"], (unsigned long)available, (unsigned long)captures.count];
	self.areaReviewPicker.enabled = captures.count > 0;
	[self.areaReviewPicker selectItemAtIndex:self.areaReviewIndex];
	self.areaReviewPrevious.enabled = [self areaReviewNeighbor:-1] >= 0;
	self.areaReviewNext.enabled = [self areaReviewNeighbor:1] >= 0;
	if (self.areaReviewIndex < 0)
	{
		self.areaReviewDetails.stringValue = @"No capture opened. Use the complete-area overview for coverage and remaining evidence gaps.";
		return;
	}
	NSDictionary *capture = captures[self.areaReviewIndex];
	self.areaReviewDetails.stringValue = [NSString stringWithFormat:@"Origin: %@\nMatched: %@    Mismatched: %@\nSolved: %@    Unresolved: %@\nProvenance: %@\n%@",
		capture[@"origin"], capture[@"matched_pixels"], capture[@"mismatched_pixels"],
		capture[@"solved_pixels"], capture[@"unresolved_pixels"], capture[@"provenance"], capture[@"path"]];
}

- (NSString *)openAreaReviewCapture:(NSInteger)index
{
	if (![self areaReviewCaptureAvailable:index]) return @"The selected capture file is missing.";
	NSDictionary *capture = self.areaReviewManifest[@"captures"][index];
	const RemasterDebugLight light = GetRemasterDebugLight();
	NSString *error = nil;
	self.openingAreaReviewFrame = YES;
	@try { error = [self openRemasterFrame:S9xAreaReviewURL(self.areaReviewURL, capture[@"path"])]; }
	@finally { self.openingAreaReviewFrame = NO; }
	if (!error)
	{
		SetRemasterDebugLight(light);
		[self refreshRemasterDebugLightControls];
		self.areaReviewIndex = index;
	}
	[self refreshAreaReview];
	return error;
}

- (void)selectAreaReviewCapture:(id)sender
{
	[self areaReviewError:[self openAreaReviewCapture:self.areaReviewPicker.indexOfSelectedItem]];
}

- (void)stepAreaReview:(NSButton *)sender
{
	NSInteger index = [self areaReviewNeighbor:sender.tag];
	if (index >= 0) [self areaReviewError:[self openAreaReviewCapture:index]];
}

- (void)openAreaReviewOverview:(id)sender
{
	NSURL *url = S9xAreaReviewURL(self.areaReviewURL, self.areaReviewManifest[@"html"]);
	if (![NSFileManager.defaultManager fileExistsAtPath:url.path] || ![NSWorkspace.sharedWorkspace openURL:url])
		[self areaReviewError:[NSString stringWithFormat:@"Could not open the complete-area overview: %@", url.path]];
}

- (void)revealAreaReview:(id)sender
{
	NSURL *url = S9xAreaReviewURL(self.areaReviewURL, self.areaReviewManifest[@"path"]);
	if (![NSFileManager.defaultManager fileExistsAtPath:url.path])
		[self areaReviewError:[NSString stringWithFormat:@"Review path is missing: %@", url.path]];
	else if (![NSWorkspace.sharedWorkspace openURL:url])
		[self areaReviewError:[NSString stringWithFormat:@"Could not open review path: %@", url.path]];
}

- (NSString *)openRemasterAreaReview:(NSURL *)fileURL
{
	if (running && !pauseEmulation) return @"Pause emulation before opening an area review.";
	if (!running && s9xthreadrunning) return @"Wait for emulation to stop before opening an area review.";
	NSString *error = nil;
	NSDictionary *manifest = S9xReadAreaReview(fileURL, &error);
	if (!manifest) return error;
	if (!self.areaReviewPanel)
	{
		NSPanel *panel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 680, 370)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
		self.areaReviewPanel = panel;
		panel.releasedWhenClosed = NO;
		panel.hidesOnDeactivate = NO;
		self.areaReviewStatus = [NSTextField wrappingLabelWithString:@""];
		self.areaReviewStatus.frame = NSMakeRect(16, 290, 648, 64);
		[panel.contentView addSubview:self.areaReviewStatus];
		self.areaReviewPicker = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(16, 250, 648, 28) pullsDown:NO];
		self.areaReviewPicker.target = self;
		self.areaReviewPicker.action = @selector(selectAreaReviewCapture:);
		[panel.contentView addSubview:self.areaReviewPicker];
		self.areaReviewDetails = [NSTextField wrappingLabelWithString:@""];
		self.areaReviewDetails.frame = NSMakeRect(16, 70, 648, 172);
		self.areaReviewDetails.selectable = YES;
		[panel.contentView addSubview:self.areaReviewDetails];
		NSArray *titles = @[@"Previous", @"Next", @"Open complete-area overview", @"Open Review Folder"];
		SEL actions[] = {@selector(stepAreaReview:), @selector(stepAreaReview:), @selector(openAreaReviewOverview:), @selector(revealAreaReview:)};
		CGFloat xs[] = {16, 116, 216, 504}, widths[] = {96, 96, 280, 160};
		for (NSUInteger i = 0; i < titles.count; ++i)
		{
			NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(xs[i], 20, widths[i], 32)];
			button.title = titles[i]; button.bezelStyle = NSBezelStyleRounded;
			button.target = self; button.action = actions[i]; button.tag = i == 0 ? -1 : 1;
			[panel.contentView addSubview:button];
			if (i == 0) self.areaReviewPrevious = button;
			if (i == 1) self.areaReviewNext = button;
		}
		[panel center];
	}
	self.areaReviewManifest = manifest;
	self.areaReviewURL = fileURL;
	self.areaReviewIndex = -1;
	self.areaReviewPanel.title = [@"Area Review — " stringByAppendingString:manifest[@"title"]];
	[self.areaReviewPicker removeAllItems];
	NSInteger index = 0;
	for (NSDictionary *capture in manifest[@"captures"])
	{
		BOOL available = [self areaReviewCaptureAvailable:index++];
		// Prefix with the index so duplicate capture titles remain distinct.
		[self.areaReviewPicker addItemWithTitle:[NSString stringWithFormat:@"%ld. %@%@", (long)index,
			capture[@"title"], available ? @"" : @" (missing)"]];
		self.areaReviewPicker.lastItem.enabled = available;
	}
	self.areaReviewPicker.autoenablesItems = NO;
	[NSUserDefaults.standardUserDefaults setObject:fileURL.path forKey:@"S9xLastAreaReview"];
	[self refreshAreaReview];
	[self.areaReviewPanel makeKeyAndOrderFront:nil];
	NSInteger first = [self areaReviewNeighbor:1];
	return first >= 0 ? [self openAreaReviewCapture:first] : nil;
}
#endif
