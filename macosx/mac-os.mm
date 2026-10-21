/*****************************************************************************\
     Snes9x - Portable Super Nintendo Entertainment System (TM) emulator.
                This file is licensed under the Snes9x License.
   For further information, consult the LICENSE file in the root directory.
\*****************************************************************************/

/***********************************************************************************
  SNES9X for Mac OS (c) Copyright John Stiles

  Snes9x for Mac OS X

  (c) Copyright 2001 - 2011  zones
  (c) Copyright 2002 - 2005  107
  (c) Copyright 2002         PB1400c
  (c) Copyright 2004         Alexander and Sander
  (c) Copyright 2004 - 2005  Steven Seeger
  (c) Copyright 2005         Ryan Vogt
  (c) Copyright 2019-2023	Michael Donald Buckley
 ***********************************************************************************/

#import <Cocoa/Cocoa.h>
#import <mach/mach_time.h>

#include <OpenGL/OpenGL.h>
#include <OpenGL/CGLRenderers.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

#import "snes9x.h"
#import "memmap.h"
#import "apu.h"
#import "controls.h"
#import "crosshairs.h"
#import "cheats.h"
#import "movie.h"
#import "snapshot.h"
#import "display.h"
#import "blit.h"
#import "remaster/remaster.h"
#include "remaster/emitter_groups.h"
#include "remaster/obstacle_group_editor.h"
#include "remaster/emission_appearance.h"
#include "remaster/inspector_height.h"
#include "remaster/profile_merge.h"
#include "remaster/npc_pose.h"

#ifdef DEBUGGER
#import "debug.h"
#endif

#import <pthread.h>

#import "mac-prefix.h"
#import "mac-audio.h"
#import "mac-cheat.h"
#import "mac-cocoatools.h"
#import "mac-controls.h"
#import "mac-dialog.h"
#import "mac-file.h"
#import "mac-gworld.h"
#import "mac-joypad.h"
#import "mac-keyboard.h"
#import "mac-musicbox.h"
#import "mac-netplay.h"
#import "mac-render.h"
#import "mac-screenshot.h"
#import "mac-snes9x.h"
#import "mac-stringtools.h"
#import "mac-os.h"
#include "remaster-benchmark.h"
#import "window-placement.h"
#include <CommonCrypto/CommonDigest.h>

static std::atomic<bool> benchmarkActive {false};
static RemasterBenchmarkTag benchmarkTag;
static std::mutex benchmarkSamplesMutex;
static std::vector<RemasterBenchmarkSample> benchmarkSamples;
static NSString *benchmarkStatePath, *benchmarkOutputPath;
static NSMutableDictionary *benchmarkReport;
static RemasterSceneSettings benchmarkSceneSettings;
static unsigned benchmarkWarmupFrames = 30, benchmarkMeasureFrames = 180;
static NSString *benchmarkMovement = @"none";
static bool benchmarkDeterministicPresentation = true;
static double benchmarkMeasurementStarted = 0;
static NSString *benchmarkInitialRgbHash;
static unsigned benchmarkFrame = 0;
static int benchmarkCase = -1;
static double benchmarkCaseStarted = 0;
static const char *benchmarkNames[] = {
	"Direct lighting only", "Direct + Indirect (0 bounces)",
	"Direct + Indirect (1 bounce)", "Direct + Indirect (8 bounces)",
	"Composite (0 bounces)", "Composite (1 bounce)", "Composite (8 bounces)"
};
static const uint8_t benchmarkBounces[] = {0, 0, 1, 8, 0, 1, 8};
static const RemasterLightingView benchmarkViews[] = {
	RemasterLightingView::DirectContribution,
	RemasterLightingView::DirectAndIndirectContribution,
	RemasterLightingView::DirectAndIndirectContribution,
	RemasterLightingView::DirectAndIndirectContribution,
	RemasterLightingView::Composite, RemasterLightingView::Composite, RemasterLightingView::Composite
};

bool RemasterBenchmarkActive() { return benchmarkActive.load(); }
bool RemasterBenchmarkDeterministicPresentation() { return RemasterBenchmarkActive() && benchmarkDeterministicPresentation; }
RemasterBenchmarkTag GetRemasterBenchmarkTag() { return benchmarkTag; }
void RecordRemasterBenchmarkSample(const RemasterBenchmarkSample &sample)
{
	std::lock_guard<std::mutex> lock(benchmarkSamplesMutex);
	benchmarkSamples.push_back(sample);
}

static NSString *BenchmarkSha256(NSData *data)
{
	unsigned char digest[CC_SHA256_DIGEST_LENGTH];
	CC_SHA256(data.bytes, static_cast<CC_LONG>(data.length), digest);
	NSMutableString *text = [NSMutableString string];
	for (unsigned char byte : digest) [text appendFormat:@"%02x", byte];
	return text;
}

static void WriteBenchmarkReport()
{
	NSError *error = nil;
	NSData *data = [NSJSONSerialization dataWithJSONObject:benchmarkReport options:NSJSONWritingPrettyPrinted error:&error];
	if (!data || ![data writeToFile:[benchmarkOutputPath stringByAppendingPathComponent:@"report.json"]
		options:NSDataWritingAtomic error:&error])
	{
		NSLog(@"Benchmark report failed: %@", error);
		exit(2);
	}
}

static void FailBenchmark(NSString *message)
{
	NSLog(@"Benchmark failed: %@", message);
	if (benchmarkReport) { benchmarkReport[@"status"] = @"failed"; benchmarkReport[@"error"] = message; WriteBenchmarkReport(); }
	exit(2);
}

static NSDictionary *BenchmarkStatistics(std::vector<double> values)
{
	if (values.empty()) return @{@"count": @0};
	std::sort(values.begin(), values.end());
	double sum = 0;
	for (double value : values) sum += value;
	auto percentile = [&](unsigned p) { return values[(values.size() * p + 99) / 100 - 1]; };
	return @{@"count": @(values.size()), @"mean": @(sum / values.size()), @"median": @(percentile(50)),
		@"p95": @(percentile(95)), @"p99": @(percentile(99)), @"max": @(values.back())};
}

static void BeginBenchmarkCase()
{
	DrainRemasterPresentation();
	if (!S9xUnfreezeGame(benchmarkStatePath.fileSystemRepresentation)) FailBenchmark(@"Could not restore benchmark save state");
	Settings.Mute = true;
	S9xClearSamples();
	ResetRemasterBenchmarkSampling();
	GFX.InfoString = "";
	GFX.InfoStringTimeout = 0;
	benchmarkFrame = 0;
	benchmarkInitialRgbHash = nil;
	benchmarkTag = {benchmarkCase, 0, false};
	RemasterSceneSettings settings = benchmarkSceneSettings;
	settings.indirectBounceCount = benchmarkBounces[benchmarkCase];
	S9xRemasterSetSceneSettings(settings);
	SetLiveRemasterPresentation(true, benchmarkViews[benchmarkCase]);
	benchmarkCaseStarted = [NSProcessInfo processInfo].systemUptime;
	benchmarkMeasurementStarted = benchmarkCaseStarted;
	NSLog(@"Benchmark case: %s", benchmarkNames[benchmarkCase]);
}

static void FinishBenchmarkFrame()
{
	if (!RemasterBenchmarkActive()) return;
	if (!benchmarkInitialRgbHash)
	{
		const RemasterFrame *initial = S9xRemasterCompletedFrame();
		if (initial) benchmarkInitialRgbHash = BenchmarkSha256([NSData dataWithBytes:initial->originalRgb555.data()
			length:initial->originalRgb555.size() * sizeof(uint16_t)]);
	}
	benchmarkFrame++;
	if (benchmarkFrame == benchmarkWarmupFrames)
	{
		DrainRemasterPresentation();
		benchmarkMeasurementStarted = [NSProcessInfo processInfo].systemUptime;
	}
	if (benchmarkFrame < benchmarkWarmupFrames + benchmarkMeasureFrames) return;
	DrainRemasterPresentation();
	const double ended = [NSProcessInfo processInfo].systemUptime;
	benchmarkReport[@"directional_disabled_override"] = @(std::getenv("S9X_REMASTER_BENCHMARK_DIRECTIONAL_OFF") != nullptr);
	benchmarkReport[@"reference_receiver_emission_override"] = @(std::getenv("S9X_REMASTER_REFERENCE_RECEIVER_EMISSION") != nullptr);
	NSMutableArray *raw = [NSMutableArray array];
	std::vector<double> gpu, fields, mesh, waits, setup, drawable, queue;
	std::vector<double> refresh, pacing, submitToGpu, driver;
	std::map<std::string, std::vector<double>> stages;
	uint64_t dropped = 0;
	{
		std::lock_guard<std::mutex> lock(benchmarkSamplesMutex);
		for (const auto &sample : benchmarkSamples)
		{
			if (sample.tag.caseIndex != benchmarkCase) continue;
			const auto &m = sample.metrics;
			gpu.push_back(m.gpuFrameMs); fields.push_back(m.lightingFieldMs);
			mesh.push_back(m.lightingMeshMs); waits.push_back(m.lightingFieldWaitMs);
			setup.push_back(m.lightingPreparationMs); drawable.push_back(m.drawableMs); queue.push_back(m.presentationQueueMs);
			refresh.push_back(m.emulationFrameMs); pacing.push_back(m.pacingWaitMs);
			submitToGpu.push_back(sample.submitToGpuMs); driver.push_back(sample.driverMs);
			dropped += m.droppedPresentations;
			NSMutableDictionary *frameStages = [NSMutableDictionary dictionary];
			for (const auto &stage : sample.gpuStages)
			{
				stages[stage.first].push_back(stage.second);
				frameStages[@(stage.first.c_str())] = @(stage.second);
			}
			[raw addObject:@{@"emulated_frame": @(sample.tag.frame), @"gpu_ms": @(m.gpuFrameMs),
				@"gpu_stages_ms": frameStages,
				@"submit_to_gpu_ms": @(sample.submitToGpuMs), @"driver_ms": @(sample.driverMs),
				@"scene_fields_ms": @(m.lightingFieldMs), @"mesh_ms": @(m.lightingMeshMs),
				@"field_wait_ms": @(m.lightingFieldWaitMs), @"gpu_setup_ms": @(m.lightingPreparationMs),
				@"drawable_ms": @(m.drawableMs), @"queue_ms": @(m.presentationQueueMs),
				@"latest_refresh_ms": @(m.emulationFrameMs), @"latest_pacing_ms": @(m.pacingWaitMs),
				@"emitters": @(m.directEmitterCount), @"direct_samples": @(m.directSampleCount),
				@"sphere_lights": @(m.directSphereCount), @"sphere_samples": @(m.directSphereSampleCount),
				@"direct_batch_size": @(m.directBatchSize),
				@"depth_emitters": @(m.directDepthEmitterCount), @"source_build_ms": @(m.directSourceBuildMs),
				@"emitter_signature": [NSString stringWithFormat:@"%016llx", static_cast<unsigned long long>(m.directEmitterSignature)],
				@"completed_uptime": @(sample.completedTime), @"dropped_since_previous_completion": @(m.droppedPresentations)}];
		}
		benchmarkSamples.clear();
	}
	NSMutableDictionary *result = [@{@"name": @(benchmarkNames[benchmarkCase]), @"bounces": @(benchmarkBounces[benchmarkCase]),
		@"view": @(static_cast<unsigned>(benchmarkViews[benchmarkCase])), @"wall_seconds_including_warmup_and_drain": @(ended - benchmarkCaseStarted),
		@"gpu_ms": BenchmarkStatistics(gpu), @"scene_fields_ms": BenchmarkStatistics(fields),
		@"mesh_ms": BenchmarkStatistics(mesh), @"field_wait_ms": BenchmarkStatistics(waits),
		@"gpu_setup_ms": BenchmarkStatistics(setup), @"drawable_ms": BenchmarkStatistics(drawable),
		@"queue_ms": BenchmarkStatistics(queue), @"completed_measured_frames": @(raw.count),
		@"dropped_since_completions": @(dropped), @"samples": raw,
		@"ram_sha256": BenchmarkSha256([NSData dataWithBytes:Memory.RAM length:0x20000])} mutableCopy];
	result[@"measurement_wall_seconds"] = @(ended - benchmarkMeasurementStarted);
	NSMutableDictionary *stageStatistics = [NSMutableDictionary dictionary];
	for (const auto &stage : stages)
		stageStatistics[@(stage.first.c_str())] = BenchmarkStatistics(stage.second);
	result[@"gpu_stages_ms"] = stageStatistics;
	result[@"latest_refresh_ms"] = BenchmarkStatistics(refresh);
	result[@"submit_to_gpu_ms"] = BenchmarkStatistics(submitToGpu);
	result[@"driver_ms"] = BenchmarkStatistics(driver);
	result[@"latest_pacing_ms"] = BenchmarkStatistics(pacing);
	result[@"completed_frames_per_wall_second"] = @(raw.count / (ended - benchmarkMeasurementStarted));
	if (!raw.count) FailBenchmark(@"No measured GPU frames completed; increase duration or use deterministic presentation");
	if (benchmarkDeterministicPresentation && raw.count != benchmarkMeasureFrames) FailBenchmark(@"Deterministic benchmark did not render every measured frame");
	const RemasterFrame *frame = S9xRemasterCompletedFrame();
	if (!frame) FailBenchmark(@"No remaster frame produced");
	NSString *capture = [NSString stringWithFormat:@"case-%02d.s9xrmf", benchmarkCase + 1];
	if (!S9xWriteRemasterFrame(*frame, [benchmarkOutputPath stringByAppendingPathComponent:capture].fileSystemRepresentation))
		FailBenchmark(@"Could not write benchmark frame capture");
	result[@"capture"] = capture;
	result[@"width"] = @(frame->width);
	result[@"height"] = @(frame->height);
	result[@"original_rgb555_sha256"] = BenchmarkSha256([NSData dataWithBytes:frame->originalRgb555.data()
		length:frame->originalRgb555.size() * sizeof(uint16_t)]);
	result[@"initial_rgb555_sha256"] = benchmarkInitialRgbHash ?: @"";
	result[@"scene_pixels_changed"] = @(![result[@"original_rgb555_sha256"] isEqual:benchmarkInitialRgbHash]);
	NSString *image = [NSString stringWithFormat:@"case-%02d.png", benchmarkCase + 1];
	if (!SaveRemasterBenchmarkImage([benchmarkOutputPath stringByAppendingPathComponent:image].fileSystemRepresentation))
		FailBenchmark(@"Could not save benchmark image");
	result[@"image"] = image;
	NSArray *previousCases = benchmarkReport[@"cases"];
	result[@"matches_first_case_ram"] = @(!previousCases.count || [result[@"ram_sha256"] isEqual:previousCases[0][@"ram_sha256"]]);
	[(NSMutableArray *)benchmarkReport[@"cases"] addObject:result];
	WriteBenchmarkReport();
	benchmarkCase++;
	if (benchmarkCase < 7 && !getenv("S9X_REMASTER_BENCHMARK_COMPOSITE_ONLY")) BeginBenchmarkCase();
	else
	{
		benchmarkTag = {};
		benchmarkReport[@"status"] = @"complete";
		WriteBenchmarkReport();
		NSLog(@"Benchmark complete: %@", benchmarkOutputPath);
		// Standalone benchmark process: avoid saving benchmark-driven SRAM or UI settings.
		exit(0);
	}
}

#define	kRecentMenu_MAX		20

volatile bool8		running             = false;
volatile bool8		s9xthreadrunning    = false;

// Enter before launching so an immediate quit also waits for a not-yet-started thread.
static dispatch_group_t emulationThreadGroup = dispatch_group_create();
static std::atomic<bool> emulationStopRequested{false};

volatile bool8		windowExtend        = true;

uint32				controlPad[MAC_MAX_PLAYERS];

uint8				romDetect           = 0,
					interleaveDetect    = 0,
					videoDetect         = 0,
					headerDetect        = 0;

WindowRef			gWindow             = NULL;
uint32				glScreenW,
					glScreenH;
CGRect				glScreenBounds;

CGFloat				rawMouseX, rawMouseY = 0;
int16				mouseX, mouseY = 0;

CGImageRef			macIconImage[118];
int					macPadIconIndex,
					macLegendIconIndex,
					macMusicBoxIconIndex, 
					macFunctionIconIndex;

int					macFrameSkip        = -1;
int32				skipFrames          = 3;
int64				lastFrame           = 0;

int64               machTimeNumerator   = 0;
int64               machTimeDenominator = 0;

int					macFastForwardRate  = 5,
					macFrameAdvanceRate = 1000000;

unsigned long		spcFileCount        = 0,
					pngFileCount        = 0;

bool8				cartOpen            = false,
					autofire            = false;

bool8				autoRes             = false,
					glstretch           = true,
					gl32bit             = true,
					vsync               = true,
					drawoverscan        = false;
int					videoMode           = VIDEOMODE_BLOCKY;

SInt32				macSoundVolume      = 80;	// %
uint32				macSoundBuffer_ms   = 80;	// ms
uint32				macSoundInterval_ms = 16;   // ms
bool8				macSoundLagEnable   = false;
uint16				aueffect            = 0;

uint8				saveInROMFolder     = 2;	// 0 : Snes9x  1 : ROM  2 : Application Support
NSString   			*saveFolderPath;

int					macCurvatureWarp    = 15,
					macAspectRatio      = 0;

bool8				startopendlog       = false,
					showtimeinfrz       = false,
					enabletoggle        = true,
					savewindowpos       = false,
					onscreeninfo        = true;
int					inactiveMode        = 2;
int					musicboxmode        = kMBXSoundEmulation;

bool8				applycheat          = false;
S9xDeviceSetting	deviceSetting       = Gamepads,
					deviceSettingMaster = Gamepads;
int					macControllerOption = SNES_JOYPAD;
AutoFireState		autofireRec[MAC_MAX_PLAYERS];

bool8				macQTRecord         = false;
uint16				macQTMovFlag        = 0;

uint16				macRecordFlag       = 0x3,
					macPlayFlag         = 0x1;
wchar_t				macRecordWChar[MOVIE_MAX_METADATA];

char				npServerIP[256],
					npName[256];

bool8				lastoverscan        = false;

CGPoint				unlimitedCursor;

#ifdef MAC_PANTHER_SUPPORT
IconRef				macIconRef[118];
#endif

id<S9xInputDelegate>    inputDelegate = nil;

typedef enum
{
    ToggleBG0,
    ToggleBG1,
    ToggleBG2,
    ToggleBG3,
    ToggleSprites,
    SwapJoypads,
    SoundChannel0,
    SoundChannel1,
    SoundChannel2,
    SoundChannel3,
    SoundChannel4,
    SoundChannel5,
    SoundChannel6,
    SoundChannel7,
    SoundChannelsOn,
    ToggleDisplayPressedKeys,
    ToggleDisplayMovieFrame,
    IncreaseFrameAdvanceRate,
    DecreaseFrameAdvanceRate,
    ToggleEmulationPause,
    AdvanceFrame,
    kNumFunctionButtons
} S9xFunctionButtonCommand;

uint8 functionButtons[kNumFunctionButtons] = {
    kVK_F1,
    kVK_F2,
    kVK_F3,
    kVK_F4,
    kVK_F5,
    kVK_F6,
    kVK_ANSI_1,
    kVK_ANSI_2,
    kVK_ANSI_3,
    kVK_ANSI_4,
    kVK_ANSI_5,
    kVK_ANSI_6,
    kVK_ANSI_7,
    kVK_ANSI_8,
    kVK_ANSI_9,
    kVK_ANSI_0,
    kVK_ANSI_Minus,
    kVK_ANSI_Q,
    kVK_ANSI_W,
    kVK_ANSI_O,
    kVK_ANSI_P
};

bool8               pressedKeys[MAC_MAX_PLAYERS][kNumButtons] = { 0 };
bool8               pressedGamepadButtons[MAC_MAX_PLAYERS][kNumButtons] = { 0 };
bool8               pressedFunctionButtons[kNumFunctionButtons] = { 0 };
bool8               pressedRawKeyboardButtons[MAC_NUM_KEYCODES] = { 0 };
bool8               heldFunctionButtons[kNumFunctionButtons] = { 0 };
pthread_mutex_t     keyLock;

S9xView             *s9xView;

enum
{
       mApple          = 128,
       iAbout          = 1,

       mFile           = 129,
       iOpen           = 1,
       iOpenMulti      = 2,
       iOpenRecent     = 3,
       iClose          = 5,
       iRomInfo        = 7,

       mControl        = 134,
       iKeyboardLayout = 1,
       iISpLayout      = 2,
       iAutoFire       = 4,
       iISpPreset      = 6,

       mEdit           = 130,

       mEmulation      = 131,
       iResume         = 1,
       iSoftReset      = 3,
       iReset          = 4,
       iDevice         = 6,

       mCheat          = 132,
       iApplyCheats    = 1,
       iGameGenie      = 3,
       iCheatFinder    = 4,

       mOption         = 133,
       iFreeze         = 1,
       iDefrost        = 2,
       iFreezeTo       = 4,
       iDefrostFrom    = 5,
       iRecordMovie    = 7,
       iPlayMovie      = 8,
       iQTMovie        = 10,
       iSaveSPC        = 12,
       iSaveSRAM       = 13,
       iCIFilter       = 15,
       iMusicBox       = 17,

       mNetplay        = 135,
       iServer         = 1,
       iClient         = 2,

       mPresets        = 201,

       mDevice         = 202,

       mRecentItem     = 203
};

struct GameViewInfo
{
	int		globalLeft;
	int		globalTop;
	int		width;
	int		height;
};

static volatile bool8	rejectinput     = false;

static bool8			pauseEmulation  = false,
						escKeyDown      = false,
						frameAdvance    = false,
						useMouse		= false;

static int				frameCount      = 0;

static bool8			frzselecting    = false;
@class S9xRemasterTileView;
static bool8			remasterFramePresenting = false;
static RemasterFrame	remasterSceneFrame;
static RemasterFrame remasterNpcInspectorFrame;
static bool remasterNpcInspectorActive = false;
static RemasterFrame &RemasterInspectorFrame()
{
	return remasterNpcInspectorActive ? remasterNpcInspectorFrame : remasterSceneFrame;
}
// Existing editor helpers consume a retained inspection packet, whether selected
// in the scene or the NPC window. Gameplay presentation keeps its own packet.
#define remasterReplayFrame RemasterInspectorFrame()

@interface S9xRemasterNpcPoseView : NSView
@property (nonatomic, weak) S9xEngine *editor;
@end
static NSPanel *remasterNpcPanel;
static NSPopUpButton *remasterNpcTypeSelector, *remasterNpcActorSelector, *remasterNpcFacingSelector, *remasterNpcPoseSelector, *remasterNpcDebugSelector;
static NSTextField *remasterNpcStatus;
static S9xRemasterNpcPoseView *remasterNpcPoseView;
static NSArray *remasterNpcActors;
static NSArray *remasterNpcTypes, *remasterNpcContexts;
static NSDictionary *remasterNpcActor, *remasterNpcPose;
static NSArray *remasterNpcFilteredPoses;
static NSPopUpButton *remasterNpcActionSelector;
static NSArray *remasterNpcVariantPoses;
static std::vector<RemasterFrame> remasterNpcVariantFrames;
static NSString *remasterNpcRomHash;
static RemasterFrame remasterNpcPreviewFrame;
static RemasterNpcPoseBounds remasterNpcCanvasBounds;
static bool remasterNpcPicking = false;
static bool remasterNpcPlaying = false;
static NSButton *remasterNpcPlayButton;
static NSButton *remasterNpcSaveButton;
static NSButton *remasterNpcHeightRampButton, *remasterNpcHeightAllPosesButton;
static NSButton *remasterNpcOverwriteButton, *remasterNpcNormalApplyButton;
static NSPopUpButton *remasterNpcNormalSelector;
static NSView *remasterNpcNormalControls;
static NSView *remasterNpcHeightRangeView;
static NSTextField *remasterNpcHeightMinInput, *remasterNpcHeightMaxInput;
static double remasterNpcHeightDisplayMin = 0, remasterNpcHeightDisplayMax = 32;
static NSTimer *remasterNpcTimer;
static NSButton *remasterFlipLayerHorizontalButton, *remasterFlipLayerVerticalButton;
static NSArray<NSButton *> *remasterShiftLayerButtons;

static bool PresentRemasterInspectionFrame(const RemasterFrame &frame, RemasterDebugMode mode,
	const std::vector<RemasterTileContentId> *selection, bool lighting, RemasterLightingView view)
{
	if (&frame == &remasterNpcInspectorFrame)
	{
		[remasterNpcPoseView setNeedsDisplay:YES];
		return true;
	}
	return DrawRemasterFrame(frame, mode, selection, lighting, view);
}
// Only this frontend's retained-frame calls use the editor-aware presentation.
#define DrawRemasterFrame PresentRemasterInspectionFrame
static bool			remasterLightingEnabled = false;
static RemasterLightingView remasterLightingView = RemasterLightingView::Composite;
static RemasterDebugMode remasterReplayDebugMode = RemasterDebugMode::Original;
static NSTextField		*remasterDebugOverlay;
static NSPanel			*remasterDebugLightPanel;
static NSButton			*remasterDebugLightEnabledButton;
static NSTextField		*remasterDebugLightInputs[3];
static NSColorWell		*remasterDebugLightColor;
// A paused live packet may only be read by its producer. Zero means no redraw.
static std::atomic<uint32_t> remasterDebugLightRedraw { 0 };
static std::mutex remasterDebugLightRedrawMutex;
static std::atomic<bool> remasterPausedCapturePending { false };
static std::mutex remasterPausedCaptureMutex;
static NSString *remasterPausedCapturePath;
static NSPoint remasterPausedCapturePoint;
static bool remasterPausedCaptureSelect = false;
static std::atomic<uint64_t> remasterCaptureSession { 0 };

static NSURL *RemasterCaptureDirectory ()
{
	NSURL *support = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
		inDomain:NSUserDomainMask appropriateForURL:nil create:YES error:nil];
	NSURL *directory = [[support URLByAppendingPathComponent:@"Snes9x" isDirectory:YES]
		URLByAppendingPathComponent:@"Remaster" isDirectory:YES];
	return [[NSFileManager defaultManager] createDirectoryAtURL:directory withIntermediateDirectories:YES
		attributes:nil error:nil] ? directory : nil;
}

static void ClearRemasterDebugLightRedraw ()
{
	// Drain an already-consumed preview before a transition. Release this lock
	// before calling the renderer or changing renderer state on the main thread.
	std::lock_guard<std::mutex> lock(remasterDebugLightRedrawMutex);
	remasterDebugLightRedraw.store(0);
}

static bool			remasterSelectionValid = false;
static RemasterTileContentId remasterSelectedTile;
static std::vector<RemasterTileContentId> remasterSelectedTiles;
static std::vector<RemasterObstacleEditorCell> remasterObstacleSelection;
static NSView *remasterObstacleView;
static NSTextField *remasterObstacleName, *remasterObstacleZ, *remasterObstacleStatus;
static NSPopUpButton *remasterObstacleMembers;
static NSButton *remasterObstacleCreate, *remasterObstacleContact, *remasterObstacleApply;
static NSButton *remasterObstacleDelete, *remasterObstacleShow;
static NSView *remasterObstacleFootprint;
static NSInteger remasterObstacleGroupIndex = -1, remasterObstacleMemberIndex = -1;
static std::string remasterObstacleFocusedGroup;
static NSPanel			*remasterInspectorPanel;
static NSPanel			*remasterProfileSettingsPanel;
static NSSlider			*remasterBounceSlider;
static NSTextField		*remasterBounceInput;
static NSTextField		*remasterBounceDescription;
static NSTextField		*remasterHeightScaleInput;
static NSSlider			*remasterHeightPreviewMultiplierSlider;
static NSTextField		*remasterHeightPreviewMultiplierInput;
static NSTextField		*remasterHeightPreviewMinInput;
static NSTextField		*remasterHeightPreviewMaxInput;
static NSInteger		remasterHeightPreviewMin = 0;
static NSInteger		remasterHeightPreviewMax = 32;
static NSTextField		*remasterCameraDirectionInputs[3];
static NSSlider			*remasterIndirectRoughnessSlider;
static NSTextField		*remasterIndirectRoughnessInput;
static NSSlider			*remasterOriginalSceneSlider;
static NSTextField		*remasterOriginalSceneInput;
static NSSlider			*remasterReflectanceBoostSlider;
static NSTextField		*remasterReflectanceBoostInput;
static NSSlider			*remasterSamplesSlider;
static NSSlider *remasterEmitterPatchSlider;
static NSTextField *remasterEmitterPatchInput;
static NSSlider *remasterRepresentativeSlider;
static NSTextField *remasterRepresentativeInput;
static NSButton *remasterRepresentativeEnabled;
static NSTextField		*remasterSamplesInput;
static NSButton			*remasterSampleAccumulationButton;
static NSButton			*remasterSettingsSaveButton;
static NSButton			*remasterMetricsButton;
static NSTextField		*remasterMetricsText;
static NSTimer			*remasterMetricsTimer;
static NSTextView		*remasterInspectorText;
static NSScrollView *remasterInspectorScrollView;
static S9xRemasterTileView *remasterTilePreview;
static S9xRemasterTileView *remasterArtworkPreview;
static S9xRemasterTileView *remasterNormalAxesView;
static NSTextField		*remasterVariantLabel;
static NSButton			*remasterPreviousVariantButton;
static NSButton			*remasterNextVariantButton;
static NSButton			*remasterMetadataToolButton;
static bool remasterMetadataToolOpening = false;
static NSPopUpButton	*remasterLayerSelector;
static NSView *remasterSemanticsView;
static NSPopUpButton *remasterRoleSelector, *remasterFacingSelector, *remasterSecondFacingSelector, *remasterCornerSelector;
static NSButton *remasterContextDependentButton;
static NSButton *remasterDoorframeBrush, *remasterDoorframeAnimationButton;
static NSTextField *remasterSemanticNoteInput;
static NSPopUpButton	*remasterMaterialBrush;
static NSSlider			*remasterValueBrush;
static NSTextField		*remasterValueInput;
static NSButton			*remasterValueDownButton;
static NSButton			*remasterValueUpButton;
static NSTextField		*remasterValueLabel;
static NSButton			*remasterOcclusionFillOpaqueButton;
static NSPopUpButton	*remasterHeightSamplingSelector;
static NSPopUpButton	*remasterHeightPreviewMode;
static NSPopUpButton *remasterHeightDomainSelector, *remasterHeightOccurrenceSelector;
static NSTextField *remasterHeightReport;
static uint32_t remasterHeightInstanceId = 0;
static std::vector<uint32_t> remasterHeightOccurrences;
static NSButton			*remasterHeightArtworkVisibleButton;
static NSButton			*remasterHeightDataVisibleButton;
static NSButton			*remasterHeightApplyAnimationButton;
static NSButton			*remasterHeightFillTileButton;
static NSButton			*remasterHeightDecreaseTileLargeButton;
static NSButton			*remasterHeightDecreaseTileButton;
static NSButton			*remasterHeightIncreaseTileButton;
static NSButton			*remasterHeightIncreaseTileLargeButton;
static NSPopUpButton	*remasterNormalPreset;
static NSTextField		*remasterNormalPixelScopeLabel;
static NSTextField		*remasterNormalXInput;
static NSTextField		*remasterNormalYInput;
static NSTextField		*remasterNormalZInput;
static NSButton			*remasterNormalUpdateButton;
static NSButton			*remasterNormalBackButton;
static NSButton			*remasterNormalFillTileButton;
static NSButton			*remasterNormalApplyAnimationButton;
static NSButton			*remasterOppositeFacingDirectButton;
static NSButton *remasterNormalIgnoreTransformButton;
static NSButton *remasterGeometryByFacingButton;
static NSPopUpButton *remasterGeometryFacingSelector;
static unsigned remasterGeometryFacing = 0;
static NSColorWell		*remasterEmissionColor;
static NSTextField *remasterEmissionDepthInput;
static NSTextField *remasterEmissionDepthLabel;
static NSButton *remasterOmnidirectionalEmissionButton;
static NSPopUpButton	*remasterEmissionPaintMode;
static NSPopUpButton	*remasterEmissionColorScope;
static NSButton			*remasterEmissionFromTileButton;
static NSButton			*remasterEmissionFromAnimationButton;
static NSButton *remasterCombineEmittersButton;
static NSButton			*remasterArtworkVisibleButton;
static NSButton			*remasterEmissionVisibleButton;
static NSButton			*remasterResetLayerButton;
static NSButton			*remasterResetTileButton;
static NSButton			*remasterCopyLayerButton;
static NSButton			*remasterPasteLayerButton;
static NSButton			*remasterCopyTileButton;
static NSButton			*remasterPasteTileButton;
static NSButton			*remasterApplyTileToVariantsButton;
static NSButton			*remasterSaveProfileButton;
static std::vector<RemasterTileContentId> remasterVariants;
static size_t			remasterVariantIndex;
static RemasterProfile	remasterEditingProfile;
static NSURL			*remasterEditingProfileURL;
static NSData *remasterProfileDiskSnapshot;
static std::set<std::string> remasterPendingProfileConflicts;
static bool			remasterEditingProfileLoaded = false;
static bool			remasterEditingProfileDirty = false;
static bool			remasterMetadataNeedsSync = true;
static bool			remasterTilePixelSelected = false;
static size_t		remasterSelectedTilePixel = 0;
static NSUndoManager	*remasterUndoManager;
static std::string	remasterSavedProfileText;
static RemasterSceneSettings remasterSavedSceneSettings;
static std::map<std::string, RemasterDirectionalLight> remasterSavedAreaLights;
static NSPanel *remasterAreaPanel;
static NSTextField *remasterAreaName, *remasterAreaDirection[3], *remasterAreaBrightness;
static NSButton *remasterAreaEnabled, *remasterAreaSave;
static S9xRemasterTileView *remasterAreaAxes;
static NSButton *remasterAreaBack;
static NSSlider *remasterAreaBrightnessSlider;
static NSColorWell *remasterAreaColor;
static NSTimer *remasterAreaTimer;
static RemasterAreaContext remasterDisplayedArea;
static bool remasterAreaNeedsRefresh = true;

static bool RemasterAreaDirty()
{
	return remasterEditingProfile.areaLights != remasterSavedAreaLights;
}
static bool			remasterNonSceneDirty = false;
@interface S9xRemasterAssetUndoSnapshot : NSObject
{
@public
	std::map<RemasterTileContentId, RemasterAssetMetadata> values;
	std::set<RemasterTileContentId> missing;
	std::map<RemasterTileContentId, std::map<std::string, RemasterTileContext>> contexts;
	uint32_t schemaVersion;
}
@end
@implementation S9xRemasterAssetUndoSnapshot
@end
static S9xRemasterAssetUndoSnapshot *remasterStrokeSnapshot;
static uint64_t		remasterStrokeVisited = 0;
static bool			remasterStrokeChanged = false;
static bool			remasterStrokeDragging = false;

static size_t RemasterNonSceneProfileOffset (const std::string &text)
{
	size_t offset = text.size();
	for (const char *header : { "\n[materials.", "\n[asset_groups.", "\n[[obstacle_groups]]", "\n[[assets]]", "\n[[rules]]" })
	{
		const size_t found = text.find(header);
		if (found != std::string::npos)
			offset = std::min(offset, found);
	}
	return offset;
}

static void UpdateRemasterEditingProfileDirty (const std::string &current)
{
	remasterMetadataNeedsSync = true;
	[remasterNpcPoseView setNeedsDisplay:YES];
	remasterNonSceneDirty = current.compare(RemasterNonSceneProfileOffset(current), std::string::npos,
		remasterSavedProfileText, RemasterNonSceneProfileOffset(remasterSavedProfileText), std::string::npos) != 0;
	remasterEditingProfileDirty = remasterNonSceneDirty || RemasterAreaDirty() || !S9xRemasterSceneSettingsEqual(
		S9xRemasterGetSceneSettings(remasterEditingProfile), remasterSavedSceneSettings);
	remasterEditingProfileDirty |= !remasterPendingProfileConflicts.empty();
}

static S9xRemasterAssetUndoSnapshot *CaptureRemasterAssetSnapshot (
	const std::set<RemasterTileContentId> &tileIds)
{
	S9xRemasterAssetUndoSnapshot *snapshot = [S9xRemasterAssetUndoSnapshot new];
	snapshot->schemaVersion = remasterEditingProfile.schemaVersion;
	for (const RemasterTileContentId &tileId : tileIds)
	{
		const auto contexts = remasterEditingProfile.tileContexts.find(tileId);
		if (contexts != remasterEditingProfile.tileContexts.end()) snapshot->contexts.emplace(tileId, contexts->second);
		const auto found = remasterEditingProfile.assets.find(tileId);
		if (found == remasterEditingProfile.assets.end())
			snapshot->missing.insert(tileId);
		else
			snapshot->values.emplace(tileId, found->second);
	}
	return snapshot;
}
static NSPoint		remasterStrokeStartPoint;
static int16_t		remasterStrokeValue = 0;
static std::array<uint8_t, 3> remasterStrokeEmissionRgb;
static uint8_t		remasterStrokeEmissionIntensity = 0;
static NSInteger	remasterStrokeEmissionMode = 0;
static std::array<uint8_t, 3> remasterStrokeNormal = {{ 128, 128, 255 }};
static bool remasterNormalComponentMixed[3] = {};
static NSString *const RemasterLastProfilePathKey = @"RemasterLastProfilePath";
static NSArray<NSString *> *workspaceGamePaths;
static NSString *workspaceProfilePath;
static BOOL workspaceSceneOpen = NO, workspaceNpcOpen = NO, restoringWorkspace = NO;
static NSString *const PreviousWorkspaceKey = @"S9xPreviousWorkspace";
static std::atomic<bool> workspaceStartPaused {false};
static NSString *const RemasterTileClipboardType = @"org.snes9x.remaster.tile-metadata";
static RemasterAssetMetadata remasterClipboardMetadata;
static NSInteger remasterClipboardLayer = -1;
static bool remasterClipboardHasData = false;
static NSString *remasterClipboardToken;
static unsigned remasterClipboardGeometryFacing = 0;
static std::string remasterTileContextName;
static NSTextField *remasterTileContextInput;
static NSPopUpButton *remasterContextSelectionMode;
static NSButton *remasterAssignContextButton;
static bool remasterSelectLocations = false;
static std::set<uint32_t> remasterSelectedContextInstances;
static NSButton *remasterRemoveContextButton, *remasterPreviousContextButton, *remasterNextContextButton, *remasterAddContextButton;

static RemasterTileContext *RemasterSelectedContext(const RemasterTileContentId &tile)
{
	if (remasterTileContextName.empty()) return nullptr;
	auto found = remasterEditingProfile.tileContexts.find(tile);
	if (found == remasterEditingProfile.tileContexts.end()) return nullptr;
	auto context = found->second.find(remasterTileContextName);
	return context == found->second.end() ? nullptr : &context->second;
}

#include "../remaster/layer_editor.h"

static bool RemasterEffectiveHeightPreview ()
{
	return remasterLayerSelector.indexOfSelectedItem == RemasterEditorHeight &&
		remasterHeightDomainSelector.indexOfSelectedItem == 1;
}

static RemasterGeometryLayers &RemasterEditingGeometry(RemasterAssetMetadata &asset)
{
	const auto stored = remasterEditingProfile.assets.find(asset.tileId);
	if (stored != remasterEditingProfile.assets.end() && &stored->second == &asset)
		if (auto *context = RemasterSelectedContext(asset.tileId)) return *context;
	return S9xRemasterFlipGeometryForEditing(asset, remasterGeometryFacing);
}

static const RemasterGeometryLayers &RemasterEditingGeometry(const RemasterAssetMetadata &asset)
{
	const auto stored = remasterEditingProfile.assets.find(asset.tileId);
	if (stored != remasterEditingProfile.assets.end() && &stored->second == &asset)
		if (auto *context = RemasterSelectedContext(asset.tileId)) return *context;
	return S9xRemasterFlipGeometryForEditing(asset, remasterGeometryFacing);
}

static unsigned RemasterInspectorPreviewFlip()
{
	if (RemasterSelectedContext(remasterSelectedTile)) return 0;
	const auto found = remasterEditingProfile.assets.find(remasterSelectedTile);
	return found != remasterEditingProfile.assets.end() && found->second.geometryByFlip &&
		remasterGeometryFacing >= 1 && remasterGeometryFacing <= 4 ? remasterGeometryFacing - 1 : 0;
}

static const std::array<int16_t, 64> *RemasterAuthoredHeight (const RemasterTileContentId &tile,
	RemasterHeightSampling &sampling)
{
	if (auto *context = RemasterSelectedContext(tile)) {
		sampling = context->heightSampling;
		return context->hasHeight ? &context->height : nullptr;
	}
	const auto found = remasterEditingProfile.assets.find(tile);
	if (remasterEditingProfileLoaded && found != remasterEditingProfile.assets.end() && found->second.geometryByFlip)
	{
		const auto &g = RemasterEditingGeometry(found->second);
		sampling = g.heightSampling;
		return g.hasHeight ? &g.height : nullptr;
	}
	return S9xRemasterInspectorAuthoredHeight(remasterEditingProfileLoaded ? &remasterEditingProfile : nullptr,
		remasterReplayFrame, tile, sampling);
}

static std::array<uint8_t, 3> EncodeRemasterNormal (float x, float y, float z)
{
	const double length = std::hypot(std::hypot(static_cast<double>(x), static_cast<double>(y)), static_cast<double>(z));
	if (length < 0.0001f)
		return {{ 128, 128, 255 }};
	return {{
		static_cast<uint8_t>(std::lround((x / length * 0.5f + 0.5f) * 255.0f)),
		static_cast<uint8_t>(std::lround((y / length * 0.5f + 0.5f) * 255.0f)),
		static_cast<uint8_t>(std::lround((z / length * 0.5f + 0.5f) * 255.0f))
	}};
}

static void DecodeRemasterNormal (const uint8_t *encoded, float &x, float &y, float &z)
{
	x = encoded[0] / 127.5f - 1.0f;
	y = encoded[1] / 127.5f - 1.0f;
	z = encoded[2] / 127.5f - 1.0f;
	const float length = std::sqrt(x * x + y * y + z * z);
	if (length < 0.0001f)
	{
		x = y = 0.0f;
		z = 1.0f;
	}
	else
	{
		x /= length;
		y /= length;
		z /= length;
	}
}

static std::array<uint8_t, 3> EffectiveRemasterNormal (const RemasterTileContentId &tileId, size_t pixel)
{
	if (auto *context = RemasterSelectedContext(tileId)) {
		const size_t p = pixel * 3;
		return context->hasNormals ? std::array<uint8_t,3>{{context->normalXyz[p], context->normalXyz[p+1], context->normalXyz[p+2]}} : std::array<uint8_t,3>{{128,128,255}};
	}
	auto editable = remasterEditingProfile.assets.find(tileId);
	if (editable != remasterEditingProfile.assets.end() && RemasterEditingGeometry(editable->second).hasNormals)
	{
		const size_t offset = pixel * 3;
		const auto &g = RemasterEditingGeometry(editable->second);
		return {{ g.normalXyz[offset], g.normalXyz[offset + 1], g.normalXyz[offset + 2] }};
	}
	if (editable != remasterEditingProfile.assets.end() && editable->second.geometryByFlip) return {{128, 128, 255}};
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, tileId);
	if (captured && captured->hasNormals)
	{
		const size_t offset = pixel * 3;
		return {{ captured->normalXyz[offset], captured->normalXyz[offset + 1], captured->normalXyz[offset + 2] }};
	}
	return {{ 128, 128, 255 }};
}

static bool EffectiveRemasterOppositeFacing (const RemasterTileContentId &tileId)
{
	if (auto *context = RemasterSelectedContext(tileId)) return context->directLightingOppositeFacing;
	auto editable = remasterEditingProfile.assets.find(tileId);
	if (editable != remasterEditingProfile.assets.end())
		return editable->second.directLightingOppositeFacing;
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, tileId);
	return captured && captured->directLightingOppositeFacing;
}

static RemasterAssetMetadata EffectiveRemasterMetadata (const RemasterTileContentId &tileId)
{
	RemasterAssetMetadata metadata;
	metadata.tileId = tileId;
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, tileId);
	if (captured)
	{
		metadata.materialSelectors = captured->materialSelectors;
		metadata.occlusion = captured->occlusion;
		metadata.normalXyz = captured->normalXyz;
		metadata.emissionRgba = captured->emissionRgba;
		metadata.emissionDepth = captured->emissionDepth;
		metadata.emissionOmnidirectional = captured->emissionOmnidirectional;
		metadata.combineEmitters = captured->combineEmitters;
		metadata.tileRole = captured->tileRole;
		metadata.hasDoorframeMask = captured->hasDoorframeMask;
		metadata.doorframeMask = captured->doorframeMask;
		metadata.hasMaterialSelectors = captured->hasMaterialSelectors;
		metadata.hasOcclusion = captured->hasOcclusion;
		metadata.hasNormals = captured->hasNormals;
		metadata.hasEmission = captured->hasEmission;
		metadata.directLightingOppositeFacing = captured->directLightingOppositeFacing;
		metadata.normalIgnoreTileTransform = captured->normalIgnoreTileTransform;
		metadata.geometryByFacing = captured->geometryByFacing;
		metadata.facingGeometry = captured->facingGeometry;
		metadata.geometryByFlip = captured->geometryByFlip;
		metadata.flipGeometry = captured->flipGeometry;
		metadata.heightSampling = captured->heightSampling;
	}
	auto editable = remasterEditingProfile.assets.find(tileId);
	if (editable == remasterEditingProfile.assets.end())
	{
		RemasterHeightSampling sampling;
		if (const auto *height = RemasterAuthoredHeight(tileId, sampling))
		{
			metadata.height = *height;
			metadata.heightSampling = sampling;
			metadata.hasHeight = true;
		}
		return metadata;
	}
	const RemasterAssetMetadata &source = editable->second;
	if (source.hasMaterialSelectors)
	{
		metadata.materialSelectors = source.materialSelectors;
		metadata.hasMaterialSelectors = true;
	}
	if (source.hasOcclusion)
	{
		metadata.occlusion = source.occlusion;
		metadata.hasOcclusion = true;
	}
	RemasterHeightSampling authoredSampling;
	if (const auto *authored = RemasterAuthoredHeight(tileId, authoredSampling))
	{
		metadata.height = *authored;
		metadata.heightSampling = authoredSampling;
		metadata.hasHeight = true;
	}
	if (source.hasNormals)
	{
		metadata.normalXyz = source.normalXyz;
		metadata.hasNormals = true;
	}
	if (source.hasEmission)
	{
		metadata.emissionRgba = source.emissionRgba;
		metadata.hasEmission = true;
	}
	metadata.directLightingOppositeFacing = source.directLightingOppositeFacing;
	metadata.normalIgnoreTileTransform = source.normalIgnoreTileTransform;
	metadata.geometryByFacing = source.geometryByFacing;
	metadata.facingGeometry = source.facingGeometry;
	metadata.geometryByFlip = source.geometryByFlip;
	metadata.flipGeometry = source.flipGeometry;
	if (S9xRemasterHasGeometryVariants(source))
	{
		metadata.height = source.height; metadata.hasHeight = source.hasHeight; metadata.heightSampling = source.heightSampling;
		metadata.normalXyz = source.normalXyz; metadata.hasNormals = source.hasNormals;
	}
	metadata.emissionDepth = source.emissionDepth;
	metadata.emissionOmnidirectional = source.emissionOmnidirectional;
	metadata.combineEmitters = source.combineEmitters;
	metadata.tileRole = source.tileRole;
	metadata.hasDoorframeMask = source.hasDoorframeMask;
	metadata.doorframeMask = source.doorframeMask;
	metadata.hasTileSemantics = source.hasTileSemantics;
	metadata.tileSemanticNormals = source.tileSemanticNormals;
	metadata.tileSemanticCorner = source.tileSemanticCorner;
	metadata.tileSemanticContextDependent = source.tileSemanticContextDependent;
	metadata.tileSemanticNote = source.tileSemanticNote;
	if (auto *context = RemasterSelectedContext(tileId)) {
		static_cast<RemasterGeometryLayers &>(metadata) = *context;
		metadata.geometryByFacing = metadata.geometryByFlip = false;
		metadata.facingGeometry = {}; metadata.flipGeometry = {};
		metadata.normalIgnoreTileTransform = context->normalIgnoreTileTransform;
		metadata.directLightingOppositeFacing = context->directLightingOppositeFacing;
	}
	return metadata;
}

static NSString *RemasterMetadataProjectRoot ()
{
	NSString *root = [[NSUserDefaults standardUserDefaults] stringForKey:@"RemasterMetadataToolProjectPath"];
	if (!root) root = [[@(__FILE__) stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
	return root;
}

static void RegisterRemasterSemanticProfile ()
{
	if (!remasterEditingProfileURL) return;
	NSString *path = [RemasterMetadataProjectRoot() stringByAppendingPathComponent:@"build/remaster-geometry/semantic-profile.json"];
	NSData *data = [NSJSONSerialization dataWithJSONObject:@{@"profile": remasterEditingProfileURL.path} options:0 error:nil];
	[data writeToFile:path atomically:YES];
}

static bool RemasterClipboardAvailable (NSInteger layer)
{
	if (!remasterClipboardHasData || !remasterClipboardToken)
		return false;
	NSString *token = [[NSPasteboard generalPasteboard] stringForType:RemasterTileClipboardType];
	return [token isEqualToString:remasterClipboardToken] && remasterClipboardLayer == layer;
}

static bool RemasterMetadataHasData (const RemasterAssetMetadata &metadata)
{
	return metadata.hasMaterialSelectors || metadata.hasOcclusion || metadata.hasHeight || metadata.hasNormals ||
		metadata.hasEmission || metadata.emissionDepth > 0 || metadata.emissionOmnidirectional || !metadata.combineEmitters.empty() || metadata.directLightingOppositeFacing || metadata.normalIgnoreTileTransform || metadata.geometryByFacing ||
		std::any_of(metadata.facingGeometry.begin(), metadata.facingGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) || metadata.geometryByFlip ||
		std::any_of(metadata.flipGeometry.begin(), metadata.flipGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; }) || metadata.hasTileSemantics || metadata.tileRole != RemasterTileRole::Unknown;
}

static void CopyRemasterMetadataLayer (const RemasterAssetMetadata &source, RemasterAssetMetadata &destination,
	NSInteger layer)
{
	if (layer == RemasterEditorMetadata)
	{
		destination.tileRole = source.tileRole;
		if (destination.tileRole != RemasterTileRole::Door)
		{
			destination.hasDoorframeMask = false;
			destination.doorframeMask = 0;
		}
		destination.hasTileSemantics = source.hasTileSemantics;
		destination.tileSemanticNormals = source.tileSemanticNormals;
		destination.tileSemanticCorner = source.tileSemanticCorner;
		destination.tileSemanticContextDependent = source.tileSemanticContextDependent;
		destination.tileSemanticNote = source.tileSemanticNote;
	}
	else if (layer == RemasterEditorDoorframe)
	{
		S9xRemasterCopyDoorframe(source, destination);
	}
	else if (layer == RemasterEditorMaterial)
	{
		destination.materialSelectors = source.materialSelectors;
		destination.hasMaterialSelectors = source.hasMaterialSelectors;
	}
	else if (layer == RemasterEditorOcclusion)
	{
		destination.occlusion = source.occlusion;
		destination.hasOcclusion = source.hasOcclusion;
	}
	else if (layer == RemasterEditorHeight)
	{
		auto &d = RemasterEditingGeometry(destination); const auto &s = S9xRemasterFlipGeometryForEditing(source, remasterGeometryFacing);
		d.height = s.height; d.hasHeight = s.hasHeight; d.heightSampling = s.heightSampling;
	}
	else if (layer == RemasterEditorNormal)
	{
		auto &d = RemasterEditingGeometry(destination); const auto &s = S9xRemasterFlipGeometryForEditing(source, remasterGeometryFacing);
		d.normalXyz = s.normalXyz; d.hasNormals = s.hasNormals;
		if (auto *context = RemasterSelectedContext(destination.tileId)) {
			context->directLightingOppositeFacing = source.directLightingOppositeFacing;
			context->normalIgnoreTileTransform = source.normalIgnoreTileTransform;
			return;
		}
		destination.directLightingOppositeFacing = source.directLightingOppositeFacing;
		destination.normalIgnoreTileTransform = source.normalIgnoreTileTransform;
	}
	else if (layer == RemasterEditorEmission)
	{
		destination.emissionRgba = source.emissionRgba;
		destination.hasEmission = source.hasEmission;
		destination.emissionDepth = source.emissionDepth;
		destination.emissionOmnidirectional = source.emissionOmnidirectional;
	}
}

static uint32_t RemasterMetadataSchemaVersion (const RemasterAssetMetadata &metadata)
{
	uint32_t version = RemasterMetadataHasData(metadata) ? 2 : 0;
	if (metadata.hasTileSemantics) version = 20;
	else if (metadata.tileRole != RemasterTileRole::Unknown) version = 18;
	if (metadata.tileRole == RemasterTileRole::Door || metadata.hasDoorframeMask) version = 21;
	if (metadata.emissionDepth > 0) version = std::max<uint32_t>(version, 14);
	if (metadata.emissionOmnidirectional) version = std::max<uint32_t>(version, 19);
	if (!metadata.combineEmitters.empty()) version = std::max<uint32_t>(version, 17);
	if (metadata.hasEmission)
		version = std::max<uint32_t>(version, 3);
	if (metadata.hasNormals)
		version = std::max<uint32_t>(version, 5);
	if (metadata.directLightingOppositeFacing)
		version = std::max<uint32_t>(version, 6);
	if (metadata.normalIgnoreTileTransform) version = std::max<uint32_t>(version, 23);
	if (metadata.geometryByFacing || std::any_of(metadata.facingGeometry.begin(), metadata.facingGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; })) version = 24;
	if (metadata.geometryByFlip || std::any_of(metadata.flipGeometry.begin(), metadata.flipGeometry.end(), [](const auto &g) { return g.hasHeight || g.hasNormals; })) version = 25;
	return version;
}

enum RemasterEmissionPaintMode
{
	RemasterEmissionPaintIntensity,
	RemasterEmissionPaintRgb,
	RemasterEmissionPaintBoth
};

enum RemasterHeightPreviewMode
{
	RemasterHeightPreviewRaw,
	RemasterHeightPreviewReconstructed,
	RemasterHeightPreviewNormals
};

static std::array<bool, 64> RemasterVisibleTileColors (const RemasterFrame &frame,
	const RemasterTileContentId &tileId, std::array<std::array<uint8_t, 3>, 64> &colors);

static NSString *RemasterLightingViewName (RemasterLightingView view)
{
	switch (view)
	{
	case RemasterLightingView::Occlusion: return @"Occlusion";
	case RemasterLightingView::Visibility: return @"Visibility";
	case RemasterLightingView::DirectContribution: return @"Direct Light";
	case RemasterLightingView::Difference: return @"Difference";
	case RemasterLightingView::Height: return @"Height";
	case RemasterLightingView::IndirectContribution: return @"Indirect Light";
	case RemasterLightingView::Normal: return @"Normals";
	case RemasterLightingView::DirectAndIndirectContribution: return @"Direct + Indirect Light";
	case RemasterLightingView::Emission: return @"Emission";
	case RemasterLightingView::Mesh: return @"Mesh (green fallback, cyan sheets, amber shells, pink walls)";
	case RemasterLightingView::Composite:
	default: return @"Composite";
	}
}

static float SampleRemasterHeight (const std::array<int16_t, 64> &height,
	RemasterHeightSampling sampling, float x, float y)
{
	x = std::max(0.0f, std::min(7.0f, x));
	y = std::max(0.0f, std::min(7.0f, y));
	if (sampling == RemasterHeightSampling::Nearest)
		return height[static_cast<size_t>(std::lround(y)) * 8 + static_cast<size_t>(std::lround(x))] / 255.0f;
	const size_t x0 = static_cast<size_t>(std::floor(x));
	const size_t y0 = static_cast<size_t>(std::floor(y));
	const size_t x1 = std::min<size_t>(7, x0 + 1);
	const size_t y1 = std::min<size_t>(7, y0 + 1);
	const float tx = x - x0;
	const float ty = y - y0;
	const float top = height[y0 * 8 + x0] * (1.0f - tx) + height[y0 * 8 + x1] * tx;
	const float bottom = height[y1 * 8 + x0] * (1.0f - tx) + height[y1 * 8 + x1] * tx;
	return (top * (1.0f - ty) + bottom * ty) / 255.0f;
}

static bool SetRemasterEmissionFromVisibleColors (RemasterProfile &profile, const RemasterFrame &frame,
	const RemasterTileContentId &tileId, int selectedPixel, uint8_t intensity)
{
	std::array<std::array<uint8_t, 3>, 64> visibleColors = {};
	const std::array<bool, 64> visible = RemasterVisibleTileColors(frame, tileId, visibleColors);
	if (std::find(visible.begin(), visible.end(), true) == visible.end())
		return false;
	const bool hadAsset = profile.assets.count(tileId);
	RemasterAssetMetadata &metadata = profile.assets[tileId];
	metadata.tileId = tileId;
	const bool hadLayer = metadata.hasEmission;
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(frame, tileId);
	if (!hadLayer && captured && captured->hasEmission)
		metadata.emissionRgba = captured->emissionRgba;
	bool changed = false;
	bool imported = false;
	for (size_t pixel = 0; pixel < 64; pixel++)
	{
		if (!visible[pixel] || (selectedPixel >= 0 && pixel != static_cast<size_t>(selectedPixel)))
			continue;
		imported = true;
		const size_t offset = pixel * 4;
		const uint8_t newRed = visibleColors[pixel][0];
		const uint8_t newGreen = visibleColors[pixel][1];
		const uint8_t newBlue = visibleColors[pixel][2];
		if (metadata.emissionRgba[offset] != newRed || metadata.emissionRgba[offset + 1] != newGreen ||
			metadata.emissionRgba[offset + 2] != newBlue || metadata.emissionRgba[offset + 3] != intensity)
		{
			metadata.emissionRgba[offset] = newRed;
			metadata.emissionRgba[offset + 1] = newGreen;
			metadata.emissionRgba[offset + 2] = newBlue;
			metadata.emissionRgba[offset + 3] = intensity;
			changed = true;
		}
	}
	if (!imported)
	{
		if (!hadAsset)
			profile.assets.erase(tileId);
		return false;
	}
	metadata.hasEmission = true;
	profile.schemaVersion = std::max<uint32_t>(profile.schemaVersion, 3);
	return changed || !hadLayer;
}

static std::array<bool, 64> RemasterVisibleTileColors (const RemasterFrame &frame,
	const RemasterTileContentId &tileId, std::array<std::array<uint8_t, 3>, 64> &colors)
{
	if (const RemasterFrameArtworkColors *artwork = S9xRemasterFrameArtworkColorsForTile(frame, tileId))
	{
		std::array<bool, 64> visible = {};
		for (size_t pixel = 0; pixel < 64; pixel++)
			if (artwork->visiblePixels & (UINT64_C(1) << pixel))
			{
				const uint16_t color = artwork->rgb555[pixel];
				colors[pixel] = {{ static_cast<uint8_t>((((color >> 10) & 31) * 527 + 23) >> 6),
					static_cast<uint8_t>((((color >> 5) & 31) * 527 + 23) >> 6),
					static_cast<uint8_t>(((color & 31) * 527 + 23) >> 6) }};
				visible[pixel] = true;
			}
		return visible;
	}
	std::array<uint32_t, 64> red = {};
	std::array<uint32_t, 64> green = {};
	std::array<uint32_t, 64> blue = {};
	std::array<uint32_t, 64> count = {};
	for (uint32_t occurrence : S9xRemasterFrameOccurrences(frame, tileId))
	{
		const RemasterFramePixel &pixel = frame.mainPixels[occurrence];
		if (pixel.tilePixel >= 64)
			continue;
		const uint16_t color = frame.originalRgb555[occurrence];
		red[pixel.tilePixel] += (((color >> 10) & 31) * 527 + 23) >> 6;
		green[pixel.tilePixel] += (((color >> 5) & 31) * 527 + 23) >> 6;
		blue[pixel.tilePixel] += ((color & 31) * 527 + 23) >> 6;
		count[pixel.tilePixel]++;
	}
	std::array<bool, 64> visible = {};
	for (size_t pixel = 0; pixel < 64; pixel++)
		if (count[pixel])
		{
			colors[pixel] = {{ static_cast<uint8_t>(red[pixel] / count[pixel]),
				static_cast<uint8_t>(green[pixel] / count[pixel]), static_cast<uint8_t>(blue[pixel] / count[pixel]) }};
			visible[pixel] = true;
		}
	return visible;
}

static bool SyncRemasterEditingMetadataToFrame (RemasterFrame &frame = remasterReplayFrame)
{
	if (!remasterEditingProfileLoaded)
		return false;
	if (&frame == &remasterReplayFrame && !remasterMetadataNeedsSync)
		return false;
	S9xRemasterApplyProfileToFrame(remasterEditingProfile, frame);
	// Global editor preview only; no persistent runtime placement binding.
	if (!remasterTileContextName.empty() && !remasterSelectLocations) {
		auto preview = remasterEditingProfile;
		for (const auto &entry : preview.tileContexts) {
			auto c = entry.second.find(remasterTileContextName);
			if (c == entry.second.end()) continue;
			auto &asset = preview.assets[entry.first]; asset.tileId = entry.first;
			static_cast<RemasterGeometryLayers &>(asset) = c->second;
			asset.geometryByFlip = asset.geometryByFacing = false;
			asset.flipGeometry = {}; asset.facingGeometry = {};
			asset.normalIgnoreTileTransform = c->second.normalIgnoreTileTransform;
			asset.directLightingOppositeFacing = c->second.directLightingOppositeFacing;
		}
		S9xRemasterApplyProfileToFrame(preview, frame);
	}
	if (&frame == &remasterReplayFrame)
		remasterMetadataNeedsSync = false;
	return true;
}

static uint16			changeAuto[2] = { 0x0000, 0x0000 };

static void Initialize (void);
static void Deinitialize (void);
static void InitAutofire (void);
static void ProcessInput (void);
static void ChangeAutofireSettings (int, int);
static void ChangeTurboRate (int);
static void UpdateFreezeDefrostScreen (int, CGImageRef, uint8 *, CGContextRef);
static void * MacSnes9xThread (void *);
static inline void EmulationLoop (void);

int main (int argc, const char *argv[])
{
    return NSApplicationMain(argc, argv);
}

static void * MacSnes9xThread (void *)
{
    Settings.StopEmulation = false;
    s9xthreadrunning = true;
	if (RemasterBenchmarkActive()) BeginBenchmarkCase();

    EmulationLoop();

    s9xthreadrunning = false;
    Settings.StopEmulation = true;

    return (NULL);
}

void CopyPressedKeys(bool8 keys[MAC_MAX_PLAYERS][kNumButtons], bool8 gamepadButtons[MAC_MAX_PLAYERS][kNumButtons])
{
    pthread_mutex_lock(&keyLock);
    memcpy(keys, pressedKeys, sizeof(pressedKeys));
    memcpy(gamepadButtons, pressedGamepadButtons, sizeof(pressedGamepadButtons));
    pthread_mutex_unlock(&keyLock);
}

static inline void EmulationLoop (void)
{
    bool8    olddisplayframerate = false;
    int        storedMacFrameSkip  = macFrameSkip;

	const bool startPaused = workspaceStartPaused.exchange(false);
	if (pauseEmulation && !startPaused)
	{
		[s9xView.emulationDelegate emulationResumed];
	}

    pauseEmulation = startPaused;
    frameAdvance   = false;
	[s9xView updatePauseOverlay];

    if (macQTRecord)
    {
        olddisplayframerate = Settings.DisplayFrameRate;
        Settings.DisplayFrameRate = false;
    }

    MacStartSound();

    if (Settings.NetPlay)
    {
//        if (Settings.NetPlayServer)
//        {
//            NPServerDetachNetPlayThread();
//            NPServerStartClients();
//
//            while (running)
//            {
//                NPServerProcessInput();
//                S9xMainLoop();
//            }
//
//            NPServerStopNetPlayThread();
//            NPServerStopServer();
//        }
//        else
//        {
//            NPClientDetachNetPlayThread();
//            NPClientNetPlayWaitStart();
//
//            while (running)
//            {
//                NPClientProcessInput();
//                S9xMainLoop();
//            }
//
//            NPClientStopNetPlayThread();
//            NPClientDisconnect();
//
//            NPClientRestoreConfig();
//        }
    }
    else
    {
        while (!emulationStopRequested.load() && running)
        {
            ProcessInput();

            if (!pauseEmulation)
            {
                ClearRemasterDebugLightRedraw();
                S9xMainLoop();
				FinishBenchmarkFrame();
            }
            else
            {
                if (frameAdvance)
                {
                    macFrameSkip = 1;
                    skipFrames = 1;
                    frameAdvance = false;
                    S9xMainLoop();
                    macFrameSkip = storedMacFrameSkip;
                }

				// Copy/write on the producer thread: the UI must never borrow a
				// mutable live packet, even while the emulator is paused.
				if (remasterPausedCapturePending.exchange(false))
				{
					const uint64_t session = remasterCaptureSession.load();
					NSString *path; NSPoint point; bool select;
					{
						std::lock_guard<std::mutex> lock(remasterPausedCaptureMutex);
						path = remasterPausedCapturePath; point = remasterPausedCapturePoint;
						select = remasterPausedCaptureSelect;
					}
					const RemasterFrame *frame = S9xRemasterCompletedFrame();
					std::string written;
					std::vector<uint8_t> bytes;
					bool success = false;
					if (!frzselecting && frame && frame->width && frame->height)
					{
						static const unsigned char alttpHash[32] = {
							0x66,0x87,0x1d,0x66,0xbe,0x19,0xad,0x2c,0x34,0xc9,0x27,0xd6,0xb1,0x4c,0xd8,0xeb,
							0x6f,0xc3,0x18,0x19,0x65,0xb6,0xe5,0x17,0xcb,0x36,0x1f,0x73,0x16,0x00,0x9c,0xfb};
						auto word = [] (int address) { return Memory.RAM[address] | (Memory.RAM[address + 1] << 8); };
						const std::string located = S9xRemasterCaptureLocationPath(path.fileSystemRepresentation,
							std::memcmp(Memory.ROMSHA256, alttpHash, 32) == 0, Memory.RAM[0x1b] != 0,
							word(0xa0), word(0x22), word(0x20));
						success = S9xSerializeRemasterFrame(*frame, bytes) && S9xRemasterWriteUniqueCapture(bytes, located, written);
					}
					NSString *savedPath = success ? [NSString stringWithUTF8String:written.c_str()] : nil;
					dispatch_async(dispatch_get_main_queue(), ^{
						if (session != remasterCaptureSession.load() || !running || !pauseEmulation || frzselecting) return;
						if (!savedPath) { NSBeep(); NSLog(@"Paused capture unavailable: allow one live frame after loading the profile."); return; }
						NSLog(@"Remaster frame capture written: %@", savedPath);
						if (select)
						{
							ClearRemasterDebugLightRedraw();
							NSString *error = [(S9xEngine *)s9xView.emulationDelegate openRemasterFrame:[NSURL fileURLWithPath:savedPath]];
							if (error.length) { NSBeep(); NSLog(@"%@", error); return; }
							[(S9xEngine *)s9xView.emulationDelegate selectRemasterPixelAtViewPoint:point extendingSelection:NO];
						}
					});
				}

                {
                    std::lock_guard<std::mutex> lock(remasterDebugLightRedrawMutex);
                    const uint32_t lightRedraw = remasterDebugLightRedraw.exchange(0);
                    if (lightRedraw && running && pauseEmulation && !frzselecting)
                    {
                        const RemasterFrame *frame = S9xRemasterCompletedFrame();
                        if (frame && frame->width == SNES_WIDTH)
                            DrawRemasterFrame(*frame, RemasterDebugMode::Original, nullptr, true,
                                static_cast<RemasterLightingView>(lightRedraw - 1));
                    }
                }

                usleep(Settings.FrameTime);
            }
        }
    }

    MacStopSound();

    if (macQTRecord)
    {
//        MacQTStopRecording();
//        macQTRecord = false;

        Settings.DisplayFrameRate = olddisplayframerate;
    }

    S9xMovieShutdown();
}
//
//static OSStatus MainEventHandler (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
//{
//    OSStatus    err, result = eventNotHandledErr;
//    Boolean        done = false;
//
//    if (frzselecting)
//        return (result);
//
//    switch (GetEventClass(inEvent))
//    {
//        case kEventClassCommand:
//            switch (GetEventKind(inEvent))
//            {
//                HICommand    cmd;
//
//                case kEventCommandUpdateStatus:
//                    err = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &cmd);
//                    if (err == noErr && cmd.commandID == 'clos')
//                    {
//                        UpdateMenuCommandStatus(false);
//                        result = noErr;
//                    }
//
//                    break;
//
//                case kEventCommandProcess:
//                    err = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &cmd);
//                    if (err == noErr)
//                    {
//                        UInt32    modifierkey;
//
//                        err = GetEventParameter(inEvent, kEventParamKeyModifiers, typeUInt32, NULL, sizeof(UInt32), NULL, &modifierkey);
//                        if (err == noErr)
//                        {
//                            if ((cmd.commandID == 'pref') && (modifierkey & optionKey))
//                                cmd.commandID = 'EXTR';
//
//                            result = HandleMenuChoice(cmd.commandID, &done);
//
//                            if (done)
//                                QuitApplicationEventLoop();
//                        }
//                    }
//
//                    break;
//            }
//
//            break;
//    }
//
//    return (result);
//}
//
//static OSStatus SubEventHandler (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
//{
//    OSStatus    err, result = eventNotHandledErr;
//
//    if (frzselecting)
//        return (result);
//
//    switch (GetEventClass(inEvent))
//    {
//        case kEventClassCommand:
//            switch (GetEventKind(inEvent))
//            {
//                HICommand    cmd;
//
//                case kEventCommandUpdateStatus:
//                    err = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &cmd);
//                    if (err == noErr && cmd.commandID == 'clos')
//                    {
//                        UpdateMenuCommandStatus(false);
//                        result = noErr;
//                    }
//
//                    break;
//
//                case kEventCommandProcess:
//                    err = GetEventParameter(inEvent, kEventParamDirectObject, typeHICommand, NULL, sizeof(HICommand), NULL, &cmd);
//                    if (err == noErr)
//                    {
//                        switch (cmd.commandID)
//                        {
//                            case 'Erun':    // Pause
//                            case 'SubQ':    // Queue from emulation thread
//                                running = false;
//                                while (s9xthreadrunning)
//                                    sleep(0);
//                                QuitApplicationEventLoop();
//                                result = noErr;
//                                break;
//
//                            case 'Ocif':    // Core Image Filter
//                                HiliteMenu(0);
//                                ConfigureCoreImageFilter();
//                                result = noErr;
//                                break;
//                        }
//                    }
//
//                    break;
//            }
//
//            break;
//
//        case kEventClassMouse:
//            if (fullscreen)
//            {
//                if ((macControllerOption == SNES_JOYPAD) || (macControllerOption == SNES_MULTIPLAYER5) || (macControllerOption == SNES_MULTIPLAYER5_2))
//                {
//                    if (!(Settings.NetPlay && !Settings.NetPlayServer))
//                    {
//                        switch (GetEventKind(inEvent))
//                        {
//                            case kEventMouseUp:
//                                HIPoint    hipt;
//
//                                err = GetEventParameter(inEvent, kEventParamMouseLocation, typeHIPoint, NULL, sizeof(HIPoint), NULL, &hipt);
//                                if (err == noErr)
//                                {
//                                    if (CGRectContainsPoint(glScreenBounds, hipt))
//                                    {
//                                        running = false;
//                                        while (s9xthreadrunning)
//                                            sleep(0);
//                                        QuitApplicationEventLoop();
//                                        result = noErr;
//                                    }
//                                }
//
//                                break;
//                        }
//                    }
//                }
//                else
//                if ((macControllerOption == SNES_MOUSE) || (macControllerOption == SNES_MOUSE_SWAPPED))
//                {
//                    switch (GetEventKind(inEvent))
//                    {
//                        case kEventMouseMoved:
//                        case kEventMouseDragged:
//                            HIPoint    hipt;
//
//                            err = GetEventParameter(inEvent, kEventParamMouseDelta, typeHIPoint, NULL, sizeof(HIPoint), NULL, &hipt);
//                            if (err == noErr)
//                            {
//                                unlimitedCursor.x += hipt.x;
//                                unlimitedCursor.y += hipt.y;
//                            }
//
//                            break;
//                    }
//                }
//            }
//
//            break;
//    }
//
//    return (result);
//}
//
//void PostQueueToSubEventLoop (void)
//{
//    OSStatus    err;
//    EventRef    event;
//
//    err = CreateEvent(kCFAllocatorDefault, kEventClassCommand, kEventCommandProcess, 0, kEventAttributeUserEvent, &event);
//    if (err == noErr)
//    {
//        HICommand    cmd;
//
//        cmd.commandID          = 'SubQ';
//        cmd.attributes         = kEventAttributeUserEvent;
//        cmd.menu.menuRef       = NULL;
//        cmd.menu.menuItemIndex = 0;
//
//        err = SetEventParameter(event, kEventParamDirectObject, typeHICommand, sizeof(HICommand), &cmd);
//        if (err == noErr)
//            err = PostEventToQueue(GetMainEventQueue(), event, kEventPriorityStandard);
//
//        ReleaseEvent(event);
//    }
//}
//
//void InitGameWindow (void)
//{
//    OSStatus            err;
//    IBNibRef            nibRef;
//    WindowAttributes    attr;
//    CFStringRef            ref;
//    HIViewRef            ctl;
//    HIViewID            cid = { 'Pict', 0 };
//    Rect                rct;
//    char                drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];
//    EventTypeSpec        wupaneEvents[] = { { kEventClassControl, kEventControlClick            },
//                                           { kEventClassControl, kEventControlDraw             } },
//                        windowEvents[] = { { kEventClassWindow,  kEventWindowDeactivated       },
//                                           { kEventClassWindow,  kEventWindowActivated         },
//                                           { kEventClassWindow,  kEventWindowBoundsChanging    },
//                                           { kEventClassWindow,  kEventWindowBoundsChanged     },
//                                           { kEventClassWindow,  kEventWindowZoom              },
//                                           { kEventClassWindow,  kEventWindowToolbarSwitchMode } };
//
//    if (gWindow)
//        return;
//
//    err = CreateNibReference(kMacS9XCFString, &nibRef);
//    if (err)
//        QuitWithFatalError(err, "os 02");
//
//    err = CreateWindowFromNib(nibRef, CFSTR("GameWindow"), &gWindow);
//    if (err)
//        QuitWithFatalError(err, "os 03");
//
//    DisposeNibReference(nibRef);
//
//    HIViewFindByID(HIViewGetRoot(gWindow), cid, &ctl);
//
//    gameWindowUPP = NewEventHandlerUPP(GameWindowEventHandler);
//    err = InstallWindowEventHandler(gWindow, gameWindowUPP, GetEventTypeCount(windowEvents), windowEvents, (void *) gWindow, &gameWindowEventRef);
//
//    gameWUPaneUPP = NewEventHandlerUPP(GameWindowUserPaneEventHandler);
//    err = InstallControlEventHandler(ctl, gameWUPaneUPP, GetEventTypeCount(wupaneEvents), wupaneEvents, (void *) gWindow, &gameWUPaneEventRef);
//
//    _splitpath(Memory.ROMFilename, drive, dir, fname, ext);
//    ref = CFStringCreateWithCString(kCFAllocatorDefault, fname, kCFStringEncodingUTF8);
//    if (ref)
//    {
//        SetWindowTitleWithCFString(gWindow, ref);
//        CFRelease(ref);
//    }
//
//    attr = kWindowFullZoomAttribute | kWindowResizableAttribute | kWindowLiveResizeAttribute;
//    err = ChangeWindowAttributes(gWindow, attr, kWindowNoAttributes);
//
//    attr = kWindowToolbarButtonAttribute;
//    if (!drawoverscan)
//        err = ChangeWindowAttributes(gWindow, attr, kWindowNoAttributes);
//    else
//        err = ChangeWindowAttributes(gWindow, kWindowNoAttributes, attr);
//
//    if (savewindowpos)
//    {
//        MoveWindow(gWindow, windowPos[kWindowScreen].h, windowPos[kWindowScreen].v, false);
//
//        if ((windowSize[kWindowScreen].width <= 0) || (windowSize[kWindowScreen].height <= 0))
//        {
//            windowExtend = true;
//            windowSize[kWindowScreen].width  = 512;
//            windowSize[kWindowScreen].height = kMacWindowHeight;
//        }
//
//        if (!lastoverscan && !windowExtend && drawoverscan)
//        {
//            windowExtend = true;
//            windowSize[kWindowScreen].height = (int) ((float) (windowSize[kWindowScreen].height + 0.5) * SNES_HEIGHT_EXTENDED / SNES_HEIGHT);
//        }
//
//        SizeWindow(gWindow, (short) windowSize[kWindowScreen].width, (short) windowSize[kWindowScreen].height, false);
//    }
//    else
//    {
//        if (drawoverscan)
//            windowExtend = true;
//
//        SizeWindow(gWindow, 512, (windowExtend ? kMacWindowHeight : (SNES_HEIGHT << 1)), false);
//        RepositionWindow(gWindow, NULL, kWindowCenterOnMainScreen);
//    }
//
//    windowZoomCount = 0;
//
//    GetWindowBounds(gWindow, kWindowContentRgn, &rct);
//    gWindowRect = CGRectMake((float) rct.left, (float) rct.top, (float) (rct.right - rct.left), (float) (rct.bottom - rct.top));
//
//    ActivateWindow(gWindow, true);
//}
//
//void UpdateGameWindow (void)
//{
//    OSStatus    err;
//    HIViewRef    ctl;
//    HIViewID    cid = { 'Pict', 0 };
//
//    if (!gWindow)
//        return;
//
//    HIViewFindByID(HIViewGetRoot(gWindow), cid, &ctl);
//    err = HIViewSetNeedsDisplay(ctl, true);
//}
//
//static void ResizeGameWindow (void)
//{
//    Rect    rct;
//    int        ww, wh;
//
//    if (!gWindow)
//        return;
//
//    GetWindowBounds(gWindow, kWindowContentRgn, &rct);
//
//    wh = (windowExtend ? SNES_HEIGHT_EXTENDED : SNES_HEIGHT) * ((windowZoomCount >> 1) + 1);
//
//    if (windowZoomCount % 2)
//        ww = SNES_NTSC_OUT_WIDTH(SNES_WIDTH) * ((windowZoomCount >> 1) + 1) / 2;
//    else
//        ww = SNES_WIDTH * ((windowZoomCount >> 1) + 1);
//
//    rct.right  = rct.left + ww;
//    rct.bottom = rct.top  + wh;
//
//    SetWindowBounds(gWindow, kWindowContentRgn, &rct);
//
//    printf("Window Size: %d, %d\n", ww, wh);
//
//    windowZoomCount++;
//    if (windowZoomCount == 8)
//        windowZoomCount = 0;
//}
//
//void DeinitGameWindow (void)
//{
//    OSStatus    err;
//
//    if (!gWindow)
//        return;
//
//    SaveWindowPosition(gWindow, kWindowScreen);
//    lastoverscan = drawoverscan;
//
//    err = RemoveEventHandler(gameWUPaneEventRef);
//    DisposeEventHandlerUPP(gameWUPaneUPP);
//
//    err = RemoveEventHandler(gameWindowEventRef);
//    DisposeEventHandlerUPP(gameWindowUPP);
//
//    CFRelease(gWindow);
//    gWindow = NULL;
//}
//
//static OSStatus GameWindowEventHandler (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
//{
//    OSStatus    err, result = eventNotHandledErr;
//    HIRect        rct;
//    Rect        r;
//    UInt32        attr;
//
//    switch (GetEventClass(inEvent))
//    {
//        case kEventClassWindow:
//            switch (GetEventKind(inEvent))
//            {
//                case kEventWindowDeactivated:
//                    if (running)
//                    {
//                        if (!(Settings.NetPlay && !Settings.NetPlayServer))
//                        {
//                            if (inactiveMode == 3)
//                            {
//                                running = false;
//                                while (s9xthreadrunning)
//                                    sleep(0);
//                                QuitApplicationEventLoop();
//                                result = noErr;
//                            }
//                            else
//                            if (inactiveMode == 2)
//                            {
//                                rejectinput = true;
//                                result = noErr;
//                            }
//                        }
//                    }
//
//                    break;
//
//                case kEventWindowActivated:
//                    if (running)
//                    {
//                        if (!(Settings.NetPlay && !Settings.NetPlayServer))
//                        {
//                            ForceChangingKeyScript();
//
//                            if (inactiveMode == 2)
//                            {
//                                rejectinput = false;
//                                result = noErr;
//                            }
//                        }
//                    }
//
//                    break;
//
//                case kEventWindowBoundsChanging:
//                    windowResizeCount = 0x7FFFFFFF;
//
//                    err = GetEventParameter(inEvent, kEventParamAttributes, typeUInt32, NULL, sizeof(UInt32), NULL, &attr);
//                    if ((err == noErr) && (attr & kWindowBoundsChangeSizeChanged))
//                    {
//                        err = GetEventParameter(inEvent, kEventParamCurrentBounds, typeHIRect, NULL, sizeof(HIRect), NULL, &rct);
//                        if (err == noErr)
//                        {
//                            if (GetCurrentEventKeyModifiers() & shiftKey)
//                            {
//                                HIRect    origRct;
//
//                                err = GetEventParameter(inEvent, kEventParamOriginalBounds, typeHIRect, NULL, sizeof(HIRect), NULL, &origRct);
//                                if (err == noErr)
//                                {
//                                    rct.size.width = (float) (int) (origRct.size.width * rct.size.height / origRct.size.height);
//                                    err = SetEventParameter(inEvent, kEventParamCurrentBounds, typeHIRect, sizeof(HIRect), &rct);
//                                }
//                            }
//
//                            gWindowRect = rct;
//                        }
//                    }
//
//                    result = noErr;
//                    break;
//
//                case kEventWindowBoundsChanged:
//                    windowResizeCount = 3;
//                    result = noErr;
//                    break;
//
//                case kEventWindowZoom:
//                    ResizeGameWindow();
//                    result = noErr;
//                    break;
//
//                case kEventWindowToolbarSwitchMode:
//                    windowExtend = !windowExtend;
//
//                    GetWindowBounds(gWindow, kWindowContentRgn, &r);
//
//                    if (windowExtend)
//                        r.bottom = r.top + (int) (((float) (r.bottom - r.top) + 0.5) * SNES_HEIGHT_EXTENDED / SNES_HEIGHT);
//                    else
//                        r.bottom = r.top + (int) (((float) (r.bottom - r.top) + 0.5) * SNES_HEIGHT / SNES_HEIGHT_EXTENDED);
//
//                    SetWindowBounds(gWindow, kWindowContentRgn, &r);
//
//                    result = noErr;
//                    break;
//            }
//
//            break;
//    }
//
//    return (result);
//}
//
//static OSStatus GameWindowUserPaneEventHandler (EventHandlerCallRef inHandlerCallRef, EventRef inEvent, void *inUserData)
//{
//    OSStatus    err, result = eventNotHandledErr;
//
//    switch (GetEventClass(inEvent))
//    {
//        case kEventClassControl:
//            switch (GetEventKind(inEvent))
//            {
//                case kEventControlClick:
//                    if (running)
//                    {
//                        if ((macControllerOption == SNES_JOYPAD) || (macControllerOption == SNES_MULTIPLAYER5) || (macControllerOption == SNES_MULTIPLAYER5_2))
//                        {
//                            if (!(Settings.NetPlay && !Settings.NetPlayServer))
//                            {
//                                if (!frzselecting)
//                                {
//                                    running = false;
//                                    while (s9xthreadrunning)
//                                        sleep(0);
//                                    QuitApplicationEventLoop();
//                                    result = noErr;
//                                }
//                            }
//                        }
//                    }
//                    else
//                    {
//                        UInt32    count;
//
//                        err = GetEventParameter(inEvent, kEventParamClickCount, typeUInt32, NULL, sizeof(UInt32), NULL, &count);
//                        if ((err == noErr) && (count == 2))
//                        {
//                            SNES9X_Go();
//                            QuitApplicationEventLoop();
//                            result = noErr;
//                        }
//                    }
//
//                    break;
//
//                case kEventControlDraw:
//                    CGContextRef    ctx;
//                    HIViewRef        view;
//                    HIRect            bounds;
//
//                    err = GetEventParameter(inEvent, kEventParamDirectObject, typeControlRef, NULL, sizeof(ControlRef), NULL, &view);
//                    if (err == noErr)
//                    {
//                        err = GetEventParameter(inEvent, kEventParamCGContextRef, typeCGContextRef, NULL, sizeof(CGContextRef), NULL, &ctx);
//                        if (err == noErr)
//                        {
//                            if (!running)
//                            {
//                                HIViewGetBounds(view, &bounds);
//                                CGContextTranslateCTM(ctx, 0, bounds.size.height);
//                                CGContextScaleCTM(ctx, 1.0f, -1.0f);
//                                DrawPauseScreen(ctx, bounds);
//                            }
//                        }
//                    }
//
//                    result = noErr;
//                    break;
//            }
//
//            break;
//    }
//
//    return (result);
//}
//
//
//static void InitRecentMenu (void)
//{
//    OSStatus    err;
//
//    err = CreateNewMenu(mRecentItem, 0, &recentMenu);
//    err = SetMenuItemHierarchicalMenu(GetMenuRef(mFile), iOpenRecent, recentMenu);
//}
//
//static void DeinitRecentMenu (void)
//{
//    CFRelease(recentMenu);
//}
//
//void BuildRecentMenu (void)
//{
//    OSStatus    err;
//    CFStringRef    str;
//
//    err = DeleteMenuItems(recentMenu, 1, CountMenuItems(recentMenu));
//
//    for (int i = 0; i < kRecentMenu_MAX; i++)
//    {
//        if (!recentItem[i])
//            break;
//
//        Boolean    r;
//        char    path[PATH_MAX + 1];
//
//        r = CFStringGetCString(recentItem[i], path, PATH_MAX, kCFStringEncodingUTF8);
//        if (r)
//        {
//            CFStringRef    nameRef;
//            char        drive[_MAX_DRIVE + 1], dir[_MAX_DIR + 1], fname[_MAX_FNAME + 1], ext[_MAX_EXT + 1];
//
//            _splitpath(path, drive, dir, fname, ext);
//            snprintf(path, PATH_MAX + 1, "%s%s", fname, ext);
//            nameRef = CFStringCreateWithCString(kCFAllocatorDefault, path, kCFStringEncodingUTF8);
//            if (nameRef)
//            {
//                err = AppendMenuItemTextWithCFString(recentMenu, nameRef, 0, 'FRe0' + i, NULL);
//                CFRelease(nameRef);
//            }
//        }
//    }
//
//    err = AppendMenuItemTextWithCFString(recentMenu, NULL, kMenuItemAttrSeparator, 'FR__', NULL);
//
//    str = CFCopyLocalizedString(CFSTR("ClearMenu"), "ClearMenu");
//    if (str)
//    {
//        err = AppendMenuItemTextWithCFString(recentMenu, str, 0, 'FRcr', NULL);
//        CFRelease(str);
//    }
//}
//
//void AdjustMenus (void)
//{
//    OSStatus    err;
//    MenuRef        menu;
//    CFStringRef    str;
//
//    if (running)
//    {
//        menu = GetMenuRef(mApple);
//        DisableMenuItem(menu, iAbout);
//        DisableMenuCommand(NULL, kHICommandPreferences);
//        DisableMenuCommand(NULL, kHICommandQuit);
//
//        menu = GetMenuRef(mFile);
//        DisableMenuItem(menu, iOpen);
//        DisableMenuItem(menu, iOpenMulti);
//        DisableMenuItem(menu, iOpenRecent);
//        DisableMenuItem(menu, iRomInfo);
//
//        menu = GetMenuRef(mControl);
//        DisableMenuItem(menu, iKeyboardLayout);
//        DisableMenuItem(menu, iISpLayout);
//        DisableMenuItem(menu, iAutoFire);
//        DisableMenuItem(menu, iISpPreset);
//
//        menu = GetMenuRef(mEmulation);
//        str = CFCopyLocalizedString(CFSTR("PauseMenu"), "pause");
//        err = SetMenuItemTextWithCFString(menu, iResume, str);
//        CFRelease(str);
//        DisableMenuItem(menu, iSoftReset);
//        DisableMenuItem(menu, iReset);
//        DisableMenuItem(menu, iDevice);
//
//        if (Settings.NetPlay)
//        {
//            if (Settings.NetPlayServer)
//                EnableMenuItem(menu, iResume);
//            else
//                DisableMenuItem(menu, iResume);
//        }
//        else
//            EnableMenuItem(menu, iResume);
//
//        menu = GetMenuRef(mCheat);
//        DisableMenuItem(menu, iApplyCheats);
//        DisableMenuItem(menu, iGameGenie);
//        DisableMenuItem(menu, iCheatFinder);
//
//        menu = GetMenuRef(mOption);
//        DisableMenuItem(menu, iFreeze);
//        DisableMenuItem(menu, iDefrost);
//        DisableMenuItem(menu, iFreezeTo);
//        DisableMenuItem(menu, iDefrostFrom);
//        DisableMenuItem(menu, iRecordMovie);
//        DisableMenuItem(menu, iPlayMovie);
//        DisableMenuItem(menu, iQTMovie);
//        DisableMenuItem(menu, iSaveSPC);
//        DisableMenuItem(menu, iSaveSRAM);
//        DisableMenuItem(menu, iMusicBox);
//        if (ciFilterEnable)
//            EnableMenuItem(menu, iCIFilter);
//        else
//            DisableMenuItem(menu, iCIFilter);
//
//        menu = GetMenuRef(mNetplay);
//        DisableMenuItem(menu, iServer);
//        DisableMenuItem(menu, iClient);
//    }
//    else
//    {
//        menu = GetMenuRef(mApple);
//        EnableMenuItem(menu, iAbout);
//        EnableMenuCommand(NULL, kHICommandPreferences);
//        EnableMenuCommand(NULL, kHICommandQuit);
//
//        menu = GetMenuRef(mFile);
//        EnableMenuItem(menu, iOpen);
//        EnableMenuItem(menu, iOpenMulti);
//        EnableMenuItem(menu, iOpenRecent);
//        if (cartOpen)
//            EnableMenuItem(menu, iRomInfo);
//        else
//            DisableMenuItem(menu, iRomInfo);
//
//        menu = GetMenuRef(mControl);
//        EnableMenuItem(menu, iKeyboardLayout);
//        EnableMenuItem(menu, iAutoFire);
//
//        menu = GetMenuRef(mEmulation);
//        str = CFCopyLocalizedString(CFSTR("RunMenu"), "run");
//        err = SetMenuItemTextWithCFString(menu, iResume, str);
//        CFRelease(str);
//        EnableMenuItem(menu, iDevice);
//        if (cartOpen)
//        {
//            EnableMenuItem(menu, iResume);
//            EnableMenuItem(menu, iSoftReset);
//            EnableMenuItem(menu, iReset);
//        }
//        else
//        {
//            DisableMenuItem(menu, iResume);
//            DisableMenuItem(menu, iSoftReset);
//            DisableMenuItem(menu, iReset);
//        }
//
//        menu = GetMenuRef(mCheat);
//        if (cartOpen)
//        {
//            EnableMenuItem(menu, iApplyCheats);
//            EnableMenuItem(menu, iGameGenie);
//            EnableMenuItem(menu, iCheatFinder);
//        }
//        else
//        {
//            DisableMenuItem(menu, iApplyCheats);
//            DisableMenuItem(menu, iGameGenie);
//            DisableMenuItem(menu, iCheatFinder);
//        }
//
//        menu = GetMenuRef(mOption);
//        DisableMenuItem(menu, iCIFilter);
//        if (cartOpen)
//        {
//            EnableMenuItem(menu, iFreeze);
//            EnableMenuItem(menu, iDefrost);
//            EnableMenuItem(menu, iFreezeTo);
//            EnableMenuItem(menu, iDefrostFrom);
//            EnableMenuItem(menu, iRecordMovie);
//            EnableMenuItem(menu, iPlayMovie);
//            EnableMenuItem(menu, iQTMovie);
//            EnableMenuItem(menu, iSaveSPC);
//            EnableMenuItem(menu, iSaveSRAM);
//            EnableMenuItem(menu, iMusicBox);
//        }
//        else
//        {
//            DisableMenuItem(menu, iFreeze);
//            DisableMenuItem(menu, iDefrost);
//            DisableMenuItem(menu, iFreezeTo);
//            DisableMenuItem(menu, iDefrostFrom);
//            DisableMenuItem(menu, iRecordMovie);
//            DisableMenuItem(menu, iPlayMovie);
//            DisableMenuItem(menu, iQTMovie);
//            DisableMenuItem(menu, iSaveSPC);
//            DisableMenuItem(menu, iSaveSRAM);
//            DisableMenuItem(menu, iMusicBox);
//        }
//
//        menu = GetMenuRef(mNetplay);
//        EnableMenuItem(menu, iClient);
//        if (cartOpen)
//            EnableMenuItem(menu, iServer);
//        else
//            DisableMenuItem(menu, iServer);
//    }
//
//    DrawMenuBar();
//}
//
//void UpdateMenuCommandStatus (Boolean closeMenu)
//{
//    if (closeMenu)
//        EnableMenuItem(GetMenuRef(mFile), iClose);
//    else
//        DisableMenuItem(GetMenuRef(mFile), iClose);
//}
//
//static OSStatus HandleMenuChoice (UInt32 command, Boolean *done)
//{
//    OSStatus    err, result = noErr;
//    MenuRef        mh;
//    int            item;
//    bool8        isok = true;
//
//    if ((command & 0xFFFFFF00) == 'FRe\0')
//    {
//        Boolean    r;
//        int        index;
//        char    path[PATH_MAX + 1];
//
//        index = (int) (command & 0x000000FF) - (int) '0';
//        r = CFStringGetCString(recentItem[index], path, PATH_MAX, kCFStringEncodingUTF8);
//        if (r)
//        {
//            FSRef    ref;
//
//            err = FSPathMakeRef((unsigned char *) path, &ref, NULL);
//            if (err == noErr)
//            {
//                if (SNES9X_OpenCart(&ref))
//                {
//                    SNES9X_Go();
//                    *done = true;
//                }
//                else
//                    AdjustMenus();
//            }
//        }
//    }
//    else
//    {
//        switch (command)
//        {
//            case 'abou':    // About SNES9X
//                StartCarbonModalDialog();
//                AboutDialog();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'pref':    // Preferences...
//                StartCarbonModalDialog();
//                ConfigurePreferences();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'EXTR':    // Extra Options...
//                StartCarbonModalDialog();
//                ConfigureExtraOptions();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'quit':    // Quit SNES9X
//                SNES9X_Quit();
//                *done = true;
//
//                break;
//
//            case 'open':    // Open ROM Image...
//                if (SNES9X_OpenCart(NULL))
//                {
//                    SNES9X_Go();
//                    *done = true;
//                }
//                else
//                    AdjustMenus();
//
//                break;
//
//            case 'Mult':    // Open Multiple ROM Images...
//                if (SNES9X_OpenMultiCart())
//                {
//                    SNES9X_Go();
//                    *done = true;
//                }
//                else
//                    AdjustMenus();
//
//                break;
//
//            case 'FRcr':    // Clear Menu
//                ClearRecentItems();
//                BuildRecentMenu();
//
//                break;
//
//            case 'Finf':    // ROM Information
//                StartCarbonModalDialog();
//                RomInfoDialog();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Ckey':    // Configure Keyboard...
//                StartCarbonModalDialog();
//                ConfigureKeyboard();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Cpad':    // Configure Controllers...
//                StartCarbonModalDialog();
//                ConfigureHID();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Caut':    // Automatic Fire...
//                StartCarbonModalDialog();
//                ConfigureAutofire();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Hapl':    // Apply Cheat Entries
//                mh = GetMenuRef(mCheat);
//                applycheat = !applycheat;
//                CheckMenuItem(mh, iApplyCheats, applycheat);
//                Settings.ApplyCheats = applycheat;
//
//                if (!Settings.ApplyCheats)
//                    S9xCheatsDisable();
//                else
//                    S9xCheatsEnable();
//
//                break;
//
//            case 'Hent':    // Cheat Entry...
//                StartCarbonModalDialog();
//                ConfigureCheat();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Hfnd':    // Cheat Finder...
//                StartCarbonModalDialog();
//                CheatFinder();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Erun':    // Run
//                SNES9X_Go();
//                *done = true;
//
//                break;
//
//            case 'Esrs':    // Software Reset
//                SNES9X_SoftReset();
//                SNES9X_Go();
//                *done = true;
//
//                break;
//
//            case 'Erst':    // Hardware Reset
//                SNES9X_Reset();
//                SNES9X_Go();
//                *done = true;
//
//                break;
//
//            case 'Ofrz':    // Freeze State
//                isok = SNES9X_Freeze();
//                *done = true;
//
//                break;
//
//            case 'Odfr':    // Defrost state
//                isok = SNES9X_Defrost();
//                *done = true;
//
//                break;
//
//            case 'Ofrd':    // Freeze State to...
//                StartCarbonModalDialog();
//                isok = SNES9X_FreezeTo();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Odfd':    // Defrost State From...
//                StartCarbonModalDialog();
//                isok = SNES9X_DefrostFrom();
//                if (gWindow)
//                    ActivateWindow(gWindow, true);
//                FinishCarbonModalDialog();
//                *done = true;
//
//                break;
//
//            case 'MVrc':    // Record Movie...
//                StartCarbonModalDialog();
//                isok = SNES9X_RecordMovie();
//                if (gWindow)
//                    ActivateWindow(gWindow, true);
//                FinishCarbonModalDialog();
//                *done = true;
//
//                break;
//
//            case 'MVpl':    // Play Movie...
//                StartCarbonModalDialog();
//                isok = SNES9X_PlayMovie();
//                if (isok && (macPlayFlag & 0x2))
//                {
//                    running = false;
//                    isok = SNES9X_QTMovieRecord();
//                    running = true;
//                }
//
//                if (gWindow)
//                    ActivateWindow(gWindow, true);
//                FinishCarbonModalDialog();
//                *done = true;
//
//                break;
//
//            case 'QTmv':    // Record QuickTime Movie...
//                StartCarbonModalDialog();
//                isok = SNES9X_QTMovieRecord();
//                if (gWindow)
//                    ActivateWindow(gWindow, true);
//                FinishCarbonModalDialog();
//                *done = true;
//
//                break;
//
//            case 'Ospc':    // Save SPC File at Next Note-on
//                S9xDumpSPCSnapshot();
//
//                break;
//
//            case 'Osrm':    // Save SRAM Now
//                SNES9X_SaveSRAM();
//
//                break;
//
//            case 'Ombx':    // Music Box
//                StartCarbonModalDialog();
//                MusicBoxDialog();
//                FinishCarbonModalDialog();
//
//                break;
//
//            case 'Nser':    // Server...
//                bool8    sr;
//
//                Settings.NetPlay = false;
//                Settings.NetPlayServer = false;
//
//                NPServerInit();
//
//                if (!NPServerStartServer(NP_PORT))
//                {
//                    NPServerStopServer();
//                    break;
//                }
//
//                StartCarbonModalDialog();
//                sr = NPServerDialog();
//                FinishCarbonModalDialog();
//
//                if (sr)
//                {
//                    SNES9X_Reset();
//                    SNES9X_Go();
//                    Settings.NetPlay = true;
//                    Settings.NetPlayServer = true;
//
//                    *done = true;
//                }
//                else
//                    NPServerStopServer();
//
//                break;
//
//            case 'Ncli':    // Client...
//                bool8    cr;
//
//                Settings.NetPlay = false;
//                Settings.NetPlayServer = false;
//
//                NPClientInit();
//
//                StartCarbonModalDialog();
//                cr = NPClientDialog();
//                FinishCarbonModalDialog();
//
//                if (cr)
//                {
//                    SNES9X_Go();
//                    Settings.NetPlay = true;
//                    Settings.NetPlayServer = false;
//
//                    *done = true;
//                }
//                else
//                    AdjustMenus();
//
//                break;
//
//            case 'CPr1':    // Controller Preset
//            case 'CPr2':
//            case 'CPr3':
//            case 'CPr4':
//            case 'CPr5':
//                item = (int) (command & 0x000000FF) - (int) '0';
//                err = GetMenuItemHierarchicalMenu(GetMenuRef(mControl), iISpPreset, &mh);
//                CheckMenuItem(mh, padSetting, false);
//                padSetting = item;
//                CheckMenuItem(mh, padSetting, true);
//                ClearPadSetting();
//                LoadControllerSettings();
//
//                break;
//
//            case 'EIp1':    // Input Device
//            case 'EIp2':
//            case 'EIp3':
//            case 'EIp4':
//            case 'EIp5':
//            case 'EIp6':
//            case 'EIp7':
//            case 'EIp8':
//                item = (int) (command & 0x000000FF) - (int) '0';
//                err = GetMenuItemHierarchicalMenu(GetMenuRef(mEmulation), iDevice, &mh);
//                CheckMenuItem(mh, deviceSetting, false);
//                deviceSetting = item;
//                deviceSettingMaster = deviceSetting;
//                CheckMenuItem(mh, deviceSetting, true);
//                ChangeInputDevice();
//
//                break;
//
//            default:
//                result = eventNotHandledErr;
//                break;
//        }
//    }
//
//    return (result);
//}
//
void ChangeInputDevice (void)
{
    switch (deviceSetting)
    {
        case Gamepads:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_JOYPAD,     1, 0, 0, 0);
            macControllerOption = SNES_JOYPAD;
			useMouse = false;
            break;

        case Mouse:
            S9xSetController(0, CTL_MOUSE,      0, 0, 0, 0);
            S9xSetController(1, CTL_JOYPAD,     1, 0, 0, 0);
            macControllerOption = SNES_MOUSE;
			useMouse = true;
            break;

        case Mouse2:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_MOUSE,      1, 0, 0, 0);
            macControllerOption = SNES_MOUSE_SWAPPED;
			useMouse = true;
            break;

        case SuperScope:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_SUPERSCOPE, 0, 0, 0, 0);
            macControllerOption = SNES_SUPERSCOPE;
			useMouse = true;
            break;

        case MultiTap:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_MP5,        1, 2, 3, 4);
            macControllerOption = SNES_MULTIPLAYER5;
			useMouse = false;
            break;

        case DoubleMultiTap:
            S9xSetController(0, CTL_MP5,        0, 1, 2, 3);
            S9xSetController(1, CTL_MP5,        4, 5, 6, 7);
            macControllerOption = SNES_MULTIPLAYER5_2;
			useMouse = false;
            break;

        case Justifier1:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_JUSTIFIER,  0, 0, 0, 0);
            macControllerOption = SNES_JUSTIFIER;
			useMouse = true;
            break;

        case Justifier2:
            S9xSetController(0, CTL_JOYPAD,     0, 0, 0, 0);
            S9xSetController(1, CTL_JUSTIFIER,  1, 0, 0, 0);
            macControllerOption = SNES_JUSTIFIER_2;
			useMouse = true;
            break;
    }

	[inputDelegate deviceSettingChanged:deviceSetting];
}

void ApplyNSRTHeaderControllers (void)
{
    uint32 valid = 0;
    deviceSetting = deviceSettingMaster;

    if (!strncmp((const char *) Memory.NSRTHeader + 24, "NSRT", 4))
    {
        switch (Memory.NSRTHeader[29])
        {
            case 0x00: // Everything goes
                deviceSetting = Gamepads;
                valid = (1 << Gamepads);
                break;

            case 0x10: // Mouse in Port 0
                deviceSetting = Mouse;
                valid = (1 << Mouse);
                break;

            case 0x01: // Mouse in Port 1
                deviceSetting = Mouse2;
                valid = (1 << Mouse2);
                break;

            case 0x03: // Super Scope in Port 1
                deviceSetting = SuperScope;
                valid = (1 << SuperScope);
                break;

            case 0x06: // Multitap in Port 1
                deviceSetting = MultiTap;
                valid = (1 << Gamepads) | (1 << MultiTap);
                break;

            case 0x66: // Multitap in Ports 0 and 1
                deviceSetting = DoubleMultiTap;
                valid = (1 << Gamepads) | (1 << MultiTap) | (1 << DoubleMultiTap);
                break;

            case 0x08: // Multitap in Port 1, Mouse in new Port 1
                deviceSetting = Mouse;
                valid = (1 << Gamepads) | (1 << Mouse2) | (1 << MultiTap);
                break;

            case 0x04: // Pad or Super Scope in Port 1
                deviceSetting = SuperScope;
                valid = (1 << Gamepads) | (1 << SuperScope);
                break;

            case 0x05: // Justifier - Must ask user...
                deviceSetting = Justifier1;
                valid = (1 << Justifier1) | (1 << Justifier2);
                break;

            case 0x20: // Pad or Mouse in Port 0
                deviceSetting = Mouse;
                valid = (1 << Gamepads) | (1 << Mouse);
                break;

            case 0x22: // Pad or Mouse in Port 0 & 1
                deviceSetting = Mouse;
                valid = (1 << Gamepads) | (1 << Mouse) | (1 << Mouse2);
                break;

            case 0x24: // Pad or Mouse in Port 0, Pad or Super Scope in Port 1
                deviceSetting = SuperScope;
                valid = (1 << Gamepads) | (1 << Mouse) | (1 << SuperScope);
                break;

            case 0x27: // Pad or Mouse in Port 0, Pad or Mouse or Super Scope in Port 1
                deviceSetting = SuperScope;
                valid = (1 << Gamepads) | (1 << Mouse) | (1 << Mouse2) | (1 << SuperScope);
                break;

            case 0x99: // Lasabirdie
                break;

            case 0x0A: // Barcode Battler
                break;

            default:
                break;
        }
    }

    ChangeInputDevice();
}

void DrawString(CGContextRef ctx, NSString *string, CGFloat size, CGFloat x, CGFloat y)
{
    NSAttributedString *astr = [[NSAttributedString alloc] initWithString:string attributes:@{NSFontAttributeName: [NSFont fontWithName:@"Helvetica" size:size], NSForegroundColorAttributeName: NSColor.whiteColor}];

    CTLineRef line = CTLineCreateWithAttributedString((__bridge CFAttributedStringRef)astr);
    CGFloat ascent = 0.0;
    CGFloat descent = 0.0;
    CGFloat leading = 0.0;
    CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

    // Draw the text in the new CoreGraphics Context
    CGContextSetTextPosition(ctx, x, y + descent);
    CTLineDraw(line, ctx);
    CFRelease(line);
}

int PromptFreezeDefrost (Boolean freezing)
{
    OSStatus            err;
    CGContextRef        ctx;
    CGColorSpaceRef     color;
    CGDataProviderRef   prov;
    CGImageRef          image;
    CGRect              rct;
    CFURLRef            url;
    FSCatalogInfo       info;
    bool8               keys[MAC_MAX_PLAYERS][kNumButtons];
    bool8               gamepadButtons[MAC_MAX_PLAYERS][kNumButtons];
    CFAbsoluteTime      newestDate, currentDate;
    int64               startTime;
    float               x, y;
    int                 result, newestIndex, current_selection, oldInactiveMode;
    char                dateC[256];
    uint8               *back, *draw;
	const bool8         wasPaused = pauseEmulation;

    const UInt32        repeatDelay = 200000;
    const int           w = SNES_WIDTH << 1, h = SNES_HEIGHT << 1;
    const char          letters[] = "123456789ABC", *filename;

    frzselecting = true;
	[s9xView updatePauseOverlay];
    oldInactiveMode = inactiveMode;
    if (inactiveMode == 3)
        inactiveMode = 2;

    S9xSetSoundMute(true);

    back = (uint8 *) malloc(w * h * 2);
    draw = (uint8 *) malloc(w * h * 2);
    if (!back || !draw)
        QuitWithFatalError(@"os 04");

    color = CGColorSpaceCreateDeviceRGB();
    if (!color)
        QuitWithFatalError(@"os 05");

    ctx = CGBitmapContextCreate(back, w, h, 5, w * 2, color, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder16Host);
    if (!ctx)
        QuitWithFatalError(@"os 06");

    rct = CGRectMake(0.0f, 0.0f, (float) w, (float) h);
    CGContextClearRect(ctx, rct);

    image = NULL;
    CFBundleRef bundle = CFBundleGetBundleWithIdentifier(CFSTR("com.snes9x.macos.snes9x-framework"));

    if (freezing)
        url = CFBundleCopyResourceURL(bundle, CFSTR("logo_freeze"),  CFSTR("png"), NULL);
    else
        url = CFBundleCopyResourceURL(bundle, CFSTR("logo_defrost"), CFSTR("png"), NULL);
    if (url)
    {
        prov = CGDataProviderCreateWithURL(url);
        if (prov)
        {
            image = CGImageCreateWithPNGDataProvider(prov, NULL, true, kCGRenderingIntentDefault);
            CGDataProviderRelease(prov);
        }

        CFRelease(url);
    }

    if (image)
    {
        rct = CGRectMake(0.0f, (float) h - 88.0f, w, 88.0f);
        CGContextDrawImage(ctx, rct, image);
        CGImageRelease(image);
    }

    newestDate  = 0;
    newestIndex = -1;

    CGContextSetLineJoin(ctx, kCGLineJoinRound);

    rct = CGRectMake(0.0f, (float) h - 208.0f, 128.0f, 120.0f);

    for (int count = 0; count < 12; count++)
    {
        url = nil;
        filename = S9xGetFreezeFilename(count);
        CFStringRef cfFilename = CFStringCreateWithCString(kCFAllocatorDefault, filename, kCFStringEncodingUTF8);

        if (cfFilename != NULL)
        {
            url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, cfFilename, kCFURLPOSIXPathStyle, false);
            CFRelease(cfFilename);
        }

        if (url != NULL)
        {
            CFDateRef date = NULL;
            if (CFURLCopyResourcePropertyForKey(url, kCFURLAttributeModificationDateKey, &date, NULL))
            {
                currentDate = CFDateGetAbsoluteTime(date);
                CFRelease(date);
            }
            else
            {
                currentDate = DBL_MIN;
            }

            if (currentDate > newestDate)
            {
                newestIndex = count;
                newestDate  = currentDate;
            }

            DrawThumbnailFromExtendedAttribute(filename, ctx, rct);

            CGContextSetShouldAntialias(ctx, false);
            CGContextSetLineWidth(ctx, 1.0f);

            CGContextSetRGBStrokeColor(ctx, 0.0f, 0.0f, 0.0f, 1.0f);
            x = rct.origin.x + 127.0f;
            y = rct.origin.y + 119.0f;
            CGContextBeginPath(ctx);
            CGContextMoveToPoint(ctx, x, y);
            CGContextAddLineToPoint(ctx, x,          y - 119.0f);
            CGContextAddLineToPoint(ctx, x - 127.0f, y - 119.0f);
            CGContextStrokePath(ctx);

            CGContextSetShouldAntialias(ctx, true);
            CGContextSetLineWidth(ctx, 3.0f);

            CGContextSetRGBFillColor(ctx, 1.0, 0.7, 0.7, 1.0);
            x = rct.origin.x +   5.0f;
            y = rct.origin.y + 102.0f;
            DrawString(ctx, [NSString stringWithFormat:@"%c", letters[count]], 12.0, x, y);

            if (showtimeinfrz)
            {
                CFAbsoluteTime        at;
                CFDateFormatterRef    format;
                CFLocaleRef            locale;
                CFStringRef            datstr;
                Boolean                r;

                err = UCConvertUTCDateTimeToCFAbsoluteTime(&(info.contentModDate), &at);
                locale = CFLocaleCopyCurrent();
                format = CFDateFormatterCreate(kCFAllocatorDefault, locale, kCFDateFormatterShortStyle, kCFDateFormatterMediumStyle);
                datstr = CFDateFormatterCreateStringWithAbsoluteTime(kCFAllocatorDefault, format, at);
                r = CFStringGetCString(datstr, dateC, sizeof(dateC), CFStringGetSystemEncoding());
                CFRelease(datstr);
                CFRelease(format);
                CFRelease(locale);

                x = rct.origin.x +  20.0f;
                y = rct.origin.y + 102.0f;
                DrawString(ctx, [NSString stringWithUTF8String:dateC], 10.0, x, y);
            }
        }
        else
        {
            x = rct.origin.x +   5.0f;
            y = rct.origin.y + 102.0f;
            DrawString(ctx, [NSString stringWithFormat:@"%c", letters[count]], 12.0, x, y);
        }

        if ((count % 4) == 3)
            rct = CGRectOffset(rct, -128.0f * 3.0f, -120.0f);
        else
            rct = CGRectOffset(rct, 128.0f, 0.0f);
    }

    if (newestIndex < 0)
        newestIndex = 0;

    CGContextRelease(ctx);

    image = NULL;

    prov = CGDataProviderCreateWithData(NULL, back, w * h * 2, NULL);
    if (prov)
    {
        image = CGImageCreate(w, h, 5, 16, w * 2, color, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder16Host, prov, NULL, 0, kCGRenderingIntentDefault);
        CGDataProviderRelease(prov);
    }

    if (!image)
        QuitWithFatalError(@"os 07");

    ctx = CGBitmapContextCreate(draw, w, h, 5, w * 2, color, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder16Host);
    if (!ctx)
        QuitWithFatalError(@"os 08");

    CGContextSetShouldAntialias(ctx, false);

    UpdateFreezeDefrostScreen(newestIndex, image, draw, ctx);

    CocoaPlayFreezeDefrostSound();

    result = -2;
    current_selection = newestIndex;

    do
    {
        if (!rejectinput)
        {
            CopyPressedKeys(keys, gamepadButtons);

            while (pressedRawKeyboardButtons[kVK_ANSI_1])
            {
                result = 0;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_2])
            {
                result = 1;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_3])
            {
                result = 2;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_4])
            {
                result = 3;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_5])
            {
                result = 4;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_6])
            {
                result = 5;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_7])
            {
                result = 6;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_8])
            {
                result = 7;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_9])
            {
                result = 8;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_A])
            {
                result = 9;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_B])
            {
                result = 10;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_ANSI_C])
            {
                result = 11;
                usleep(repeatDelay);
            }

            while (pressedRawKeyboardButtons[kVK_Return] || pressedRawKeyboardButtons[kVK_ANSI_KeypadEnter])
            {
                result = current_selection;
                usleep(repeatDelay);
            }

            while (KeyIsPressed(keys, gamepadButtons, 0, kRight))
            {
                startTime = mach_absolute_time();
                current_selection += 1;
                if (current_selection > 11)
                    current_selection -= 12;
                UpdateFreezeDefrostScreen(current_selection, image, draw, ctx);
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }

            while (KeyIsPressed(keys, gamepadButtons, 0, kLeft))
            {
                startTime = mach_absolute_time();
                current_selection -= 1;
                if (current_selection < 0)
                    current_selection += 12;
                UpdateFreezeDefrostScreen(current_selection, image, draw, ctx);
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }

            while (KeyIsPressed(keys, gamepadButtons, 0, kDown))
            {
                startTime = mach_absolute_time();
                current_selection += 4;
                if (current_selection > 11)
                    current_selection -= 12;
                UpdateFreezeDefrostScreen(current_selection, image, draw, ctx);
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }

            while (KeyIsPressed(keys, gamepadButtons, 0, kUp))
            {
                startTime = mach_absolute_time();
                current_selection -= 4;
                if (current_selection < 0)
                    current_selection += 12;
                UpdateFreezeDefrostScreen(current_selection, image, draw, ctx);
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }

            while (ISpKeyIsPressed(keys, gamepadButtons, kISpEsc))
            {
                result = -1;
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }

            while (KeyIsPressed(keys, gamepadButtons, 0, kA) ||
                   KeyIsPressed(keys, gamepadButtons, 1, kA) ||
                   KeyIsPressed(keys, gamepadButtons, 0, kB) ||
                   KeyIsPressed(keys, gamepadButtons, 1, kB) ||
                   KeyIsPressed(keys, gamepadButtons, 0, kX) ||
                   KeyIsPressed(keys, gamepadButtons, 1, kX) ||
                   KeyIsPressed(keys, gamepadButtons, 0, kY) ||
                   KeyIsPressed(keys, gamepadButtons, 1, kY))
            {
                result = current_selection;
                usleep(repeatDelay);
                CopyPressedKeys(keys, gamepadButtons);
            }
        }

        usleep(30000);

        UpdateFreezeDefrostScreen(current_selection, image, draw, ctx);
    } while (result == -2 && frzselecting);

    CocoaPlayFreezeDefrostSound();

    CGContextRelease(ctx);
    CGImageRelease(image);
    CGColorSpaceRelease(color);
    free(draw);
    free(back);

    S9xSetSoundMute(false);

    inactiveMode = oldInactiveMode;
    frzselecting = false;
	pauseEmulation = wasPaused;
	if (!freezing && result >= 0 && wasPaused)
		frameAdvance = true;

	[s9xView updatePauseOverlay];

    return (result);
}

static void UpdateFreezeDefrostScreen (int newIndex, CGImageRef image, uint8 *draw, CGContextRef ctx)
{
    if (newIndex >= 0 && newIndex < 12)
    {
        CGRect      rct;
        const int   w = SNES_WIDTH << 1, h = SNES_HEIGHT << 1;

        CGContextSetLineWidth(ctx, 1.0f);

        rct = CGRectMake(0.0f, 0.0f, (float) w, (float) h);
        CGContextDrawImage(ctx, rct, image);

        rct = CGRectMake(0.0f, (float) h - 208.0f, 128.0f, 120.0f);
        rct = CGRectOffset(rct, (float) (128 * (newIndex % 4)), (float) (-120 * (newIndex / 4)));
        rct.size.width  -= 1.0f;
        rct.size.height -= 1.0f;

        CGContextSetRGBStrokeColor(ctx, 1.0f, 1.0f, 0.0f, 1.0f);
        CGContextStrokeRect(ctx, rct);
        rct = CGRectInset(rct, 1.0f, 1.0f);
        CGContextSetRGBStrokeColor(ctx, 0.0f, 0.0f, 0.0f, 1.0f);
        CGContextStrokeRect(ctx, rct);
    }

    DrawFreezeDefrostScreen(draw);
}

static void ResumeEmulationFromInput (void)
{
	// Stay paused until the UI has cleared the retained replay. Do not block the
	// producer: ROM loading on the UI thread may be waiting for it to stop.
	const uint64_t session = remasterCaptureSession.load();
	dispatch_async(dispatch_get_main_queue(), ^{
		if (session != remasterCaptureSession.load() || !running || !pauseEmulation || frzselecting)
			return;
		[(S9xEngine *)s9xView.emulationDelegate resume];
	});
}

static void ProcessInput (void)
{
	if (RemasterBenchmarkActive())
	{
		benchmarkTag = {benchmarkCase, benchmarkFrame, benchmarkFrame >= benchmarkWarmupFrames};
		const bool moving = benchmarkFrame >= benchmarkWarmupFrames;
		const uint32_t pad = !moving ? 0 : [benchmarkMovement isEqualToString:@"left"] ? 0x0200 :
			[benchmarkMovement isEqualToString:@"right"] ? 0x0100 : 0;
		for (int i = 0; i < MAC_MAX_PLAYERS; i++) ControlPadFlagsToS9xReportButtons(i, i == 0 ? pad : 0);
		return;
	}
    bool8           keys[MAC_MAX_PLAYERS][kNumButtons];
    bool8           gamepadButtons[MAC_MAX_PLAYERS][kNumButtons];
    bool8           isok, fnbtn, altbtn, tcbtn;
    static bool8    toggleff = false, lastTimeTT = false, lastTimeFn = false, ffUp = false, ffDown = false;

    if (rejectinput)
        return;

    CopyPressedKeys(keys, gamepadButtons);

    fnbtn  = ISpKeyIsPressed(keys, gamepadButtons, kISpFunction);
    altbtn = ISpKeyIsPressed(keys, gamepadButtons, kISpAlt);

    if (fnbtn)
    {
        if (!lastTimeFn)
        {
            memset(heldFunctionButtons, 0, kNumFunctionButtons);
        }

        lastTimeFn = true;
        lastTimeTT = false;
        ffUp = ffDown = false;

        for (unsigned int i = 0; i < kNumFunctionButtons; i++)
        {
            if (pressedFunctionButtons[i])
            {
                if (!heldFunctionButtons[i])
                {
                    s9xcommand_t    s9xcmd;
                    static char     msg[64];

                    heldFunctionButtons[i] = true;

                    switch ((S9xFunctionButtonCommand) i)
                    {
                        case ToggleBG0:
                            s9xcmd = S9xGetCommandT("ToggleBG0");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case ToggleBG1:
                            s9xcmd = S9xGetCommandT("ToggleBG1");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case ToggleBG2:
                            s9xcmd = S9xGetCommandT("ToggleBG2");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case ToggleBG3:
                            s9xcmd = S9xGetCommandT("ToggleBG3");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case ToggleSprites:
                            s9xcmd = S9xGetCommandT("ToggleSprites");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SwapJoypads:
                            s9xcmd = S9xGetCommandT("SwapJoypads");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel0:
                            s9xcmd = S9xGetCommandT("SoundChannel0");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel1:
                            s9xcmd = S9xGetCommandT("SoundChannel1");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel2:
                            s9xcmd = S9xGetCommandT("SoundChannel2");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel3:
                            s9xcmd = S9xGetCommandT("SoundChannel3");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel4:
                            s9xcmd = S9xGetCommandT("SoundChannel4");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel5:
                            s9xcmd = S9xGetCommandT("SoundChannel5");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel6:
                            s9xcmd = S9xGetCommandT("SoundChannel6");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannel7:
                            s9xcmd = S9xGetCommandT("SoundChannel7");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case SoundChannelsOn:
                            s9xcmd = S9xGetCommandT("SoundChannelsOn");
                            S9xApplyCommand(s9xcmd, 1, 0);
                            break;

                        case ToggleDisplayPressedKeys:
                            Settings.DisplayPressedKeys = !Settings.DisplayPressedKeys;
                            break;

                        case ToggleDisplayMovieFrame:
                            if (S9xMovieActive())
                                Settings.DisplayMovieFrame = !Settings.DisplayMovieFrame;
                            break;

                        case IncreaseFrameAdvanceRate:
                            if (macFrameAdvanceRate < 5000000)
                                macFrameAdvanceRate += 100000;
                            sprintf(msg, "Emulation Speed: 100/%d", macFrameAdvanceRate / 10000);
                            S9xSetInfoString(msg);
                            break;

                        case DecreaseFrameAdvanceRate:
                            if (macFrameAdvanceRate > 500000)
                                macFrameAdvanceRate -= 100000;
                            sprintf(msg, "Emulation Speed: 100/%d", macFrameAdvanceRate / 10000);
                            S9xSetInfoString(msg);
                            break;

                        case ToggleEmulationPause:
							if (!pauseEmulation)
							{
								pauseEmulation = true;
								[s9xView.emulationDelegate emulationPaused];
							}
							else
							{
								ResumeEmulationFromInput();
							}
							
							[s9xView updatePauseOverlay];
                            break;

                        case AdvanceFrame:
                            frameAdvance = true;
                            break;

                        case kNumFunctionButtons:
                            break;

                    }
                }
            }
        }
    }
    else
    {
        lastTimeFn = false;

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpEsc))
        {
			if (!escKeyDown)
			{
				escKeyDown = true;

				if (!pauseEmulation)
				{
					pauseEmulation = true;
					[s9xView.emulationDelegate emulationPaused];
				}
				else
				{
					ResumeEmulationFromInput();
				}

				[s9xView updatePauseOverlay];

				dispatch_async(dispatch_get_main_queue(), ^
				{
					[s9xView setNeedsDisplay:YES];
				});
			}
        }
		else
		{
			escKeyDown = false;
		}

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpFreeze))
        {
            MacStopSound();
            while (ISpKeyIsPressed(keys, gamepadButtons, kISpFreeze))
                CopyPressedKeys(keys, gamepadButtons);

            isok = SNES9X_Freeze();
            return;
        }

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpDefrost))
        {
            MacStopSound();
            while (ISpKeyIsPressed(keys, gamepadButtons, kISpDefrost))
                CopyPressedKeys(keys, gamepadButtons);

            isok = SNES9X_Defrost();
            return;
        }

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpScreenshot))
        {
            Settings.TakeScreenshot = true;
            while (ISpKeyIsPressed(keys, gamepadButtons, kISpScreenshot))
                CopyPressedKeys(keys, gamepadButtons);
        }

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpSPC))
        {
            S9xDumpSPCSnapshot();
            while (ISpKeyIsPressed(keys, gamepadButtons, kISpSPC))
                CopyPressedKeys(keys, gamepadButtons);
        }

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpFFUp))
        {
            if (!ffUp)
            {
                ChangeTurboRate(+1);
                ffUp = true;
            }
        }
        else
            ffUp = false;

        if (ISpKeyIsPressed(keys, gamepadButtons, kISpFFDown))
        {
            if (!ffDown)
            {
                ChangeTurboRate(-1);
                ffDown = true;
            }
        }
        else
            ffDown = false;

        for (int i = 0; i < MAC_MAX_PLAYERS; ++i)
        {
            controlPad[i] = 0;
            if (KeyIsPressed(keys, gamepadButtons, i, kR     ))    controlPad[i] |= 0x0010;
            if (KeyIsPressed(keys, gamepadButtons, i, kL     ))    controlPad[i] |= 0x0020;
            if (KeyIsPressed(keys, gamepadButtons, i, kX     ))    controlPad[i] |= 0x0040;
            if (KeyIsPressed(keys, gamepadButtons, i, kA     ))    controlPad[i] |= 0x0080;
            if (KeyIsPressed(keys, gamepadButtons, i, kRight ))    controlPad[i] |= 0x0100;
            if (KeyIsPressed(keys, gamepadButtons, i, kLeft  ))    controlPad[i] |= 0x0200;
            if (KeyIsPressed(keys, gamepadButtons, i, kDown  ))    controlPad[i] |= 0x0400;
            if (KeyIsPressed(keys, gamepadButtons, i, kUp    ))    controlPad[i] |= 0x0800;
            if (KeyIsPressed(keys, gamepadButtons, i, kStart ))    controlPad[i] |= 0x1000;
            if (KeyIsPressed(keys, gamepadButtons, i, kSelect))    controlPad[i] |= 0x2000;
            if (KeyIsPressed(keys, gamepadButtons, i, kY     ))    controlPad[i] |= 0x4000;
            if (KeyIsPressed(keys, gamepadButtons, i, kB     ))    controlPad[i] |= 0x8000;
        }

        if (altbtn)
        {
            if (!lastTimeTT)
                changeAuto[0] = changeAuto[1] = 0;

            for (int i = 0; i < 2; i++)
            {
                for (int j = 0; j < 12; j++)
                {
                    uint16    mask = 0x0010 << j;

                    if (controlPad[i] & mask & autofireRec[i].toggleMask)
                    {
                        controlPad[i] &= ~mask;

                        if (!(changeAuto[i] & mask))
                        {
                            changeAuto[i] |= mask;
                            ChangeAutofireSettings(i, j);
                        }
                    }
                    else
                        changeAuto[i] &= ~mask;
                }
            }

            lastTimeTT = true;
        }
        else
            lastTimeTT = false;
    }

    if (enabletoggle)
    {
        if (ISpKeyIsPressed(keys, gamepadButtons, kISpFastForward) && !fnbtn)
        {
            if (!toggleff)
            {
                toggleff = true;
                Settings.TurboMode = !Settings.TurboMode;
                S9xSetInfoString(Settings.TurboMode ? "Turbo mode on" : "Turbo mode off");
                if (!Settings.TurboMode)
                    S9xClearSamples();
            }
        }
        else
            toggleff = false;
    }
    else
    {
        bool8    old = Settings.TurboMode;
        Settings.TurboMode = (ISpKeyIsPressed(keys, gamepadButtons, kISpFastForward) && !fnbtn) ? true : false;
        if (!Settings.TurboMode && old)
            S9xClearSamples();
    }

    for (int i = 0; i < 2; i++)
        controlPad[i] ^= autofireRec[i].invertMask;

    if (autofire)
    {
        long long    currentTime;
        uint16        changeMask;

        currentTime = GetMicroseconds();
        tcbtn = (ISpKeyIsPressed(keys, gamepadButtons, kISpTC));

        for (int i = 0; i < 2; i++)
        {
            changeMask = (lastTimeTT ? (~changeAuto[i]) : 0xFFFF);

            for (int j = 0; j < 12; j++)
            {
                uint16    mask = (0x0010 << j) & changeMask;

                if (autofireRec[i].tcMask & mask)
                {
                    if (!tcbtn)
                        continue;
                }

                if (autofireRec[i].buttonMask & mask)
                {
                    if (controlPad[i] & mask)
                    {
                        if (currentTime > autofireRec[i].nextTime[j])
                        {
                            if (Settings.TurboMode)
                                autofireRec[i].nextTime[j] = currentTime + (long long) ((1.0 / (float) autofireRec[i].frequency) * 1000000.0 / macFastForwardRate);
                            else
                                autofireRec[i].nextTime[j] = currentTime + (long long) ((1.0 / (float) autofireRec[i].frequency) * 1000000.0);
                        }
                        else
                            controlPad[i] &= ~mask;
                    }
                }
            }
        }
    }

	for (int i = 0; i < MAC_MAX_PLAYERS; ++i)
	{
		ControlPadFlagsToS9xReportButtons(i, controlPad[i]);
	}

    if (macControllerOption == SNES_JUSTIFIER_2)
	{
        ControlPadFlagsToS9xPseudoPointer(controlPad[1]);
	}
}

static void ChangeAutofireSettings (int player, int btn)
{
    static char    msg[64];
    uint16        mask, m;

    mask = 0x0010 << btn;
    autofireRec[player].buttonMask ^= mask;
    autofire = (autofireRec[0].buttonMask || autofireRec[1].buttonMask);

    m = autofireRec[player].buttonMask;
    if (m)
        snprintf(msg, sizeof(msg), "Autofire %d:%s%s%s%s%s%s%s%s%s%s%s%s%s", player + 1,
            (m & 0xC0F0 ?   " " : ""),
            (m & 0x0080 ?   "A" : ""),
            (m & 0x8000 ?   "B" : ""),
            (m & 0x0040 ?   "X" : ""),
            (m & 0x4000 ?   "Y" : ""),
            (m & 0x0020 ?   "L" : ""),
            (m & 0x0010 ?   "R" : ""),
            (m & 0x0800 ? " Up" : ""),
            (m & 0x0400 ? " Dn" : ""),
            (m & 0x0200 ? " Lf" : ""),
            (m & 0x0100 ? " Rt" : ""),
            (m & 0x1000 ? " St" : ""),
            (m & 0x2000 ? " Se" : ""));
    else
        snprintf(msg, sizeof(msg), "Autofire %d: Off", player + 1);

    S9xSetInfoString(msg);
}

static void ChangeTurboRate (int d)
{
    static char    msg[64];

    macFastForwardRate += d;
    if (macFastForwardRate < 1)
        macFastForwardRate = 1;
    else
    if (macFastForwardRate > 15)
        macFastForwardRate = 15;

    snprintf(msg, sizeof(msg), "Turbo Rate: %d", macFastForwardRate);
    S9xSetInfoString(msg);
}

static void Initialize (void)
{
	bzero(&Settings, sizeof(Settings));
	Settings.MouseMaster = true;
	Settings.SuperScopeMaster = true;
	Settings.JustifierMaster = true;
	Settings.MultiPlayer5Master = true;
	Settings.FrameTimePAL = 20000;
	Settings.FrameTimeNTSC = 16667;
	Settings.DisplayWatchedAddresses = true;
	Settings.SixteenBitSound = true;
	Settings.Stereo = true;
	Settings.SoundPlaybackRate = 32000;
	Settings.SoundInputRate = 31950;
	Settings.Transparency = true;
	Settings.AutoDisplayMessages = true;
	Settings.InitialInfoStringTimeout = 120;
	Settings.HDMATimingHack = 100;
	Settings.BlockInvalidVRAMAccessMaster = true;
	Settings.StopEmulation = true;
	Settings.WrongMovieStateProtection = true;
	Settings.DumpStreamsMaxFrames = -1;
	Settings.StretchScreenshots = 1;
	Settings.SnapshotScreenshots = true;
	Settings.SuperFXClockMultiplier = 100;
	Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN;
	Settings.MaxSpriteTilesPerLine = 34;
	Settings.OneClockCycle = 6;
	Settings.OneSlowClockCycle = 8;
	Settings.TwoClockCycles = 12;

    mach_timebase_info_data_t info;
    mach_timebase_info(&info);

    machTimeNumerator = info.numer;
    machTimeDenominator = info.denom * 1000;

	npServerIP[0] = 0;
	npName[0] = 0;

	saveFolderPath = NULL;

	CreateIconImages();

	InitKeyboard();
	InitAutofire();

	InitGraphics();
	InitMacSound();
	SetUpHID();

	autofire = (autofireRec[0].buttonMask || autofireRec[1].buttonMask) ? true : false;
	for (int a = 0; a < MAC_MAX_PLAYERS; a++)
		for (int b = 0; b < 12; b++)
			autofireRec[a].nextTime[b] = 0;

	S9xMovieInit();

	S9xUnmapAllControls();
	S9xSetupDefaultKeymap();
	ChangeInputDevice();

	if (!Memory.Init() || !S9xInitAPU() || !S9xGraphicsInit())
    {

    }

	frzselecting = false;
	[s9xView updatePauseOverlay];

	S9xSetControllerCrosshair(X_MOUSE1, 0, NULL, NULL);
	S9xSetControllerCrosshair(X_MOUSE2, 0, NULL, NULL);
}

static void Deinitialize (void)
{
	deviceSetting = deviceSettingMaster;

	ReleaseHID();
	DeinitGraphics();
	DeinitKeyboard();
	DeinitMacSound();
	ReleaseIconImages();

	S9xGraphicsDeinit();
	S9xDeinitAPU();
	Memory.Deinit();

	pthread_mutex_destroy(&keyLock);
}

uint64 GetMicroseconds(void)
{
    uint64 ms = mach_absolute_time();
    ms *= machTimeNumerator;
    ms /= machTimeDenominator;

    return ms;
}

static void InitAutofire (void)
{
	autofire = false;

	for (int i = 0; i < 2; i++)
	{
		for (int j = 0; j < 12; j++)
			autofireRec[i].nextTime[j] = 0;

		autofireRec[i].buttonMask = 0x0000;
		autofireRec[i].toggleMask = 0xFFF0;
		autofireRec[i].tcMask     = 0x0000;
		autofireRec[i].invertMask = 0x0000;
		autofireRec[i].frequency  = 10;
	}
}

void S9xSyncSpeed (void)
{
	long long	currentFrame, adjustment;
	const bool measurePerformance = S9xRemasterPerformanceMetricsEnabled();
	const auto pacingStarted = std::chrono::steady_clock::now();

	if (Settings.SoundSync)
	{
		while (!S9xSyncSound())
			usleep(0);
	}

	if (!macQTRecord)
	{
		if (macFrameSkip < 0)	// auto skip
		{
			skipFrames--;

			if (skipFrames <= 0)
			{
				adjustment = (Settings.TurboMode ? (macFrameAdvanceRate / macFastForwardRate) : macFrameAdvanceRate) / Memory.ROMFramesPerSecond;
				currentFrame = GetMicroseconds();

				skipFrames = (int32) ((currentFrame - lastFrame) / adjustment);
				lastFrame += frameCount * adjustment;

				if (skipFrames < 1)
					skipFrames = 1;
				else
				if (skipFrames > 7)
				{
					skipFrames = 7;
					lastFrame = GetMicroseconds();
				}

				frameCount = skipFrames;

				if (lastFrame > currentFrame)
					usleep((useconds_t) (lastFrame - currentFrame));

				IPPU.RenderThisFrame = true;
			}
			else
				IPPU.RenderThisFrame = false;
		}
		else					// constant
		{
			skipFrames--;

			if (skipFrames <= 0)
			{
				adjustment = macFrameAdvanceRate * macFrameSkip / Memory.ROMFramesPerSecond;
				currentFrame = GetMicroseconds();

				if (currentFrame - lastFrame < adjustment)
				{
					usleep((useconds_t) (adjustment + lastFrame - currentFrame));
					currentFrame = GetMicroseconds();
				}

				lastFrame = currentFrame;
				skipFrames = macFrameSkip;
				if (Settings.TurboMode)
					skipFrames *= macFastForwardRate;

				IPPU.RenderThisFrame = true;
			}
			else
				IPPU.RenderThisFrame = false;
		}
	}
	else
	{
		//MacQTRecordFrame(IPPU.RenderedScreenWidth, IPPU.RenderedScreenHeight);

		adjustment = macFrameAdvanceRate / Memory.ROMFramesPerSecond;
		currentFrame = GetMicroseconds();

		if (currentFrame - lastFrame < adjustment)
			usleep((useconds_t) (adjustment + lastFrame - currentFrame));

		lastFrame = currentFrame;

		IPPU.RenderThisFrame = true;
	}
	if (measurePerformance)
	{
		RemasterState &state = S9xRemasterState();
		std::lock_guard<std::mutex> metricsLock(state.performanceMetricsMutex);
		state.performanceMetrics.pacingWaitMs =
			std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - pacingStarted).count();
	}
}

void S9xAutoSaveSRAM (void)
{
    SNES9X_SaveSRAM();
}

void S9xMessage (int type, int number, const char *message)
{
	static char	mes[256];

	if (!onscreeninfo)
	{
		printf("%s\n", message);

//        if ((type == S9X_INFO) && (number == S9X_ROM_INFO))
//            if (strstr(message, "checksum ok") == NULL)
//                AppearanceAlert(kAlertCautionAlert, kS9xMacAlertkBadRom, kS9xMacAlertkBadRomHint);
	}
	else
	{
		strncpy(mes, message, 255);
		S9xSetInfoString(mes);
	}
}

const char * S9xStringInput (const char *s)
{
	return (NULL);
}

void S9xToggleSoundChannel (int c)
{
    static int	channel_enable = 255;

	if (c == 8)
		channel_enable = 255;
    else
		channel_enable ^= 1 << c;

	S9xSetSoundControl(channel_enable);
}

void S9xExit (void)
{
	NSBeep();

	running = false;
	cartOpen = false;
}

void QuitWithFatalError ( NSString *message)
{
    NSError *error = [NSError errorWithDomain:@"com.snes9x" code:0 userInfo:@{ NSLocalizedFailureReasonErrorKey: message }];
    NSAlert *alert = [NSAlert alertWithError:error];
    [alert runModal];
    [NSApp terminate:nil];
}

@interface S9xView ()
@property (nonatomic) BOOL remasterRightConsumed;
@property (nonatomic) BOOL remasterRightDown;
@property (nonatomic) BOOL remasterRightDragged;
@property (nonatomic) NSPoint remasterRightDownPoint;
@property (nonatomic) NSPoint remasterRightLastPoint;
- (void)cancelRemasterDebugLightGesture;
@end

@implementation S9xView

+ (void)initialize
{
	pthread_mutex_init(&keyLock, NULL);
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];

    if (self)
    {
        NSView *dimmedView = [[NSView alloc] initWithFrame:frameRect];
        dimmedView.wantsLayer = YES;
        dimmedView.layer.backgroundColor = NSColor.blackColor.CGColor;
        dimmedView.layer.opacity = 0.5;
        dimmedView.layer.zPosition = 100.0;
        dimmedView.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:dimmedView];

        [dimmedView.topAnchor constraintEqualToAnchor:self.topAnchor].active = YES;
        [dimmedView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor].active = YES;
        [dimmedView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor].active = YES;
        [dimmedView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor].active = YES;

        dimmedView.hidden = YES;

		remasterDebugOverlay = [[NSTextField alloc] initWithFrame:NSZeroRect];
		remasterDebugOverlay.editable = NO;
		remasterDebugOverlay.selectable = NO;
		remasterDebugOverlay.bezeled = NO;
		remasterDebugOverlay.drawsBackground = YES;
		remasterDebugOverlay.backgroundColor = [NSColor colorWithCalibratedWhite:0.0 alpha:0.65];
		remasterDebugOverlay.textColor = NSColor.whiteColor;
		remasterDebugOverlay.font = [NSFont userFixedPitchFontOfSize:10];
		remasterDebugOverlay.alignment = NSTextAlignmentCenter;
		remasterDebugOverlay.wantsLayer = YES;
		remasterDebugOverlay.layer.cornerRadius = 3.0;
		remasterDebugOverlay.layer.zPosition = 110.0;
		remasterDebugOverlay.translatesAutoresizingMaskIntoConstraints = NO;
		remasterDebugOverlay.hidden = YES;
		[self addSubview:remasterDebugOverlay];
		[remasterDebugOverlay.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:6.0].active = YES;
		[remasterDebugOverlay.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:6.0].active = YES;
		[remasterDebugOverlay.heightAnchor constraintEqualToConstant:18.0].active = YES;
		[remasterDebugOverlay.widthAnchor constraintGreaterThanOrEqualToConstant:120.0].active = YES;
    }

    return self;
}

- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
	newWindow.acceptsMouseMovedEvents = YES;
}

- (void)keyDown:(NSEvent *)event
{
    if (!NSApp.isActive)
    {
        return;
    }

    pthread_mutex_lock(&keyLock);
    S9xButton button = keyCodes[event.keyCode];
    if ( button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player <= MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = true;
    }

    for ( NSUInteger i = 0; i < kNumFunctionButtons; ++i )
    {
        if ( event.keyCode == functionButtons[i])
        {
            pressedFunctionButtons[i] = true;
            break;
        }
    }

    pressedRawKeyboardButtons[event.keyCode] = true;

    pthread_mutex_unlock(&keyLock);
}

- (void)keyUp:(NSEvent *)event
{
    if (!NSApp.isActive)
    {
        return;
    }

    pthread_mutex_lock(&keyLock);
    S9xButton button = keyCodes[event.keyCode];
    if ( button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player <= MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = false;
    }

    for ( NSUInteger i = 0; i < kNumFunctionButtons; ++i )
    {
        if ( event.keyCode == functionButtons[i])
        {
            pressedFunctionButtons[i] = false;
            heldFunctionButtons[i] = false;
            break;
        }
    }

    pressedRawKeyboardButtons[event.keyCode] = false;

    pthread_mutex_unlock(&keyLock);
}

- (void)flagsChanged:(NSEvent *)event
{
    if (!NSApp.isActive)
    {
        return;
    }

    pthread_mutex_lock(&keyLock);

    NSEventModifierFlags flags = event.modifierFlags;

    struct S9xButton button = keyCodes[kVK_Shift];
    if (button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player < MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = (flags & NSEventModifierFlagShift) != 0;
    }

    button = keyCodes[kVK_Command];
    if (button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player < MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = (flags & NSEventModifierFlagCommand) != 0;
    }

    button = keyCodes[kVK_Control];
    if (button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player < MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = (flags & NSEventModifierFlagControl) != 0;
    }

    button = keyCodes[kVK_Option];
    if (button.buttonCode >= 0 && button.buttonCode < kNumButtons && button.player >= 0 && button.player < MAC_MAX_PLAYERS)
    {
        pressedKeys[button.player][button.buttonCode] = (flags & NSEventModifierFlagOption) != 0;
    }

    pthread_mutex_unlock(&keyLock);
}

- (void)mouseDown:(NSEvent *)event
{
	if ([self.emulationDelegate respondsToSelector:@selector(selectRemasterPixelAtViewPoint:extendingSelection:)] &&
		[self.emulationDelegate selectRemasterPixelAtViewPoint:[self convertPoint:event.locationInWindow fromView:nil]
			extendingSelection:(event.modifierFlags & NSEventModifierFlagCommand) != 0])
		return;
	if ( useMouse )
	{
		switch (deviceSetting)
		{
			case Mouse:
			case SuperScope:
			case Justifier1:
				pressedKeys[0][kKeyMouseLeft] = true;
				break;

			case Mouse2:
			case Justifier2:
				pressedKeys[1][kKeyMouseLeft] = true;
				break;

			default:
				break;
		}
	}
	else
	{
		pauseEmulation = true;
		[self.emulationDelegate emulationPaused];

		[s9xView updatePauseOverlay];
	}
}

- (void)mouseUp:(NSEvent *)event
{
	if ( useMouse )
	{
		switch (deviceSetting)
		{
			case Mouse:
			case SuperScope:
			case Justifier1:
				pressedKeys[0][kKeyMouseLeft] = false;
				break;

			case Mouse2:
			case Justifier2:
				pressedKeys[1][kKeyMouseLeft] = false;
				break;

			default:
				break;
		}
	}
}

- (void)rightMouseDown:(NSEvent *)event
{
	self.remasterRightDownPoint = [self convertPoint:event.locationInWindow fromView:nil];
	self.remasterRightLastPoint = self.remasterRightDownPoint;
	self.remasterRightDragged = NO;
	self.remasterRightConsumed = !useMouse &&
		[self.emulationDelegate respondsToSelector:@selector(canBeginRemasterDebugLightAtViewPoint:)] &&
		[self.emulationDelegate respondsToSelector:@selector(updateRemasterDebugLightAtViewPoint:toggle:)] &&
		[self.emulationDelegate canBeginRemasterDebugLightAtViewPoint:self.remasterRightDownPoint];
	self.remasterRightDown = self.remasterRightConsumed;
	if (self.remasterRightConsumed)
		return;
	if ( useMouse )
	{
		switch (deviceSetting)
		{
			case Mouse:
			case SuperScope:
			case Justifier1:
				pressedKeys[0][kKeyMouseRight] = true;
				break;

			case Mouse2:
			case Justifier2:
				pressedKeys[1][kKeyMouseRight] = true;
				break;

			default:
				break;
		}
	}
}

- (void)rightMouseUp:(NSEvent *)event
{
	if (self.remasterRightConsumed)
	{
		[self rightMouseDragged:event];
		if (self.remasterRightDown && !self.remasterRightDragged && !useMouse &&
			[self.emulationDelegate respondsToSelector:@selector(updateRemasterDebugLightAtViewPoint:toggle:)])
			[self.emulationDelegate updateRemasterDebugLightAtViewPoint:self.remasterRightDownPoint toggle:YES];
		self.remasterRightConsumed = NO;
		[self cancelRemasterDebugLightGesture];
		return;
	}
	if ( useMouse )
	{
		switch (deviceSetting)
		{
			case Mouse:
			case SuperScope:
			case Justifier1:
				pressedKeys[0][kKeyMouseRight] = false;
				break;

			case Mouse2:
			case Justifier2:
				pressedKeys[1][kKeyMouseRight] = false;
				break;

			default:
				break;
		}
	}
}

- (void)mouseMoved:(NSEvent *)event
{
	if ( useMouse && running && !pauseEmulation )
	{
		rawMouseX += event.deltaX;
		rawMouseY += event.deltaY;
		CGRect bounds = self.bounds;

		if (rawMouseX < 0)
		{
			rawMouseX = 0;
		}
		else if (rawMouseX > bounds.size.width)
		{
			rawMouseX = bounds.size.width;
		}

		if (rawMouseY < 0)
		{
			rawMouseY = 0;
		}
		else if ( rawMouseY > bounds.size.height)
		{
			rawMouseY = bounds.size.height;
		}

		mouseX = (int16) (rawMouseX / ((float) bounds.size.width ) * (float) IPPU.RenderedScreenWidth);
		mouseY = (int16) (rawMouseY / ((float) bounds.size.height) * (float) IPPU.RenderedScreenHeight);
	}
}

- (void)mouseDragged:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)rightMouseDragged:(NSEvent *)event
{
	if (self.remasterRightConsumed)
	{
		if (!self.remasterRightDown || useMouse)
			return;
		const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
		if (std::hypot(point.x - self.remasterRightDownPoint.x, point.y - self.remasterRightDownPoint.y) >= 3.0)
			self.remasterRightDragged = YES;
		if (self.remasterRightDragged)
		{
			if (event.modifierFlags & NSEventModifierFlagCommand)
			{
				if ([self.emulationDelegate respondsToSelector:@selector(adjustRemasterDebugLightHeightByViewDelta:)])
					[self.emulationDelegate adjustRemasterDebugLightHeightByViewDelta:
						(point.y - self.remasterRightLastPoint.y) * (self.isFlipped ? -1 : 1)];
			}
			else if ([self.emulationDelegate respondsToSelector:@selector(updateRemasterDebugLightAtViewPoint:toggle:)])
				[self.emulationDelegate updateRemasterDebugLightAtViewPoint:point toggle:NO];
		}
		self.remasterRightLastPoint = point;
		return;
	}
	[self mouseMoved:event];
}

- (void)cancelRemasterDebugLightGesture
{
	// Still swallow the release of a consumed gesture after a session change.
	self.remasterRightDown = NO;
	self.remasterRightDragged = NO;
}

- (void)otherMouseDragged:(NSEvent *)event
{
	[self mouseMoved:event];
}

- (void)updatePauseOverlay
{
	dispatch_async(dispatch_get_main_queue(), ^{
		self.subviews[0].hidden = (frzselecting || remasterFramePresenting || !pauseEmulation);
		CGFloat scaleFactor = MAX(self.window.backingScaleFactor, 1.0);
		glScreenW = self.frame.size.width * scaleFactor;
		glScreenH = self.frame.size.height * scaleFactor;

		BOOL showMouse = !useMouse || !running || pauseEmulation;
		CGAssociateMouseAndMouseCursorPosition(showMouse);

		if (showMouse)
		{
			[NSCursor unhide];
		}
		else
		{
			CGRect frame = self.frame;
			CGPoint point = CGPointMake(frame.size.width / 2.0, frame.size.height / 2.0);
			point = [self convertPoint:point toView:nil];
			point = [self.window convertPointToScreen:point];
			point.y = self.window.screen.frame.size.height - point.y;
			CGWarpMouseCursorPosition(point);
			[NSCursor hide];
		}
	});
}

- (void)updateRemasterDebugOverlay
{
	dispatch_async(dispatch_get_main_queue(), ^{
		remasterDebugOverlay.stringValue = [NSString stringWithFormat:@"GI Debug: %@",
			RemasterLightingViewName(remasterLightingView)];
		remasterDebugOverlay.hidden = !remasterLightingEnabled;
	});
}

- (void)setFrame:(NSRect)frame
{
    if ( !NSEqualRects(frame, self.frame) )
    {
        [super setFrame:frame];
    }
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

- (BOOL)canBecomeKeyView
{
    return YES;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    return !( running && !pauseEmulation);
}

@end

@class S9xEngine;

@interface S9xRemasterTileView : NSImageView
@property (nonatomic, weak) S9xEngine *editor;
@property (nonatomic) BOOL showsNormalAxes;
@property (nonatomic) float normalX;
@property (nonatomic) float normalY;
@property (nonatomic) float normalZ;
@property (nonatomic) BOOL trackingNormal;
@property (nonatomic) BOOL areaDirection;
@end

@interface S9xEngine () <NSWindowDelegate, NSTextFieldDelegate>
@property (nonatomic, strong) NSPanel *areaReviewPanel;
@property (nonatomic, strong) NSPopUpButton *areaReviewPicker;
@property (nonatomic, strong) NSTextField *areaReviewStatus;
@property (nonatomic, strong) NSTextField *areaReviewDetails;
@property (nonatomic, strong) NSButton *areaReviewPrevious;
@property (nonatomic, strong) NSButton *areaReviewNext;
@property (nonatomic, strong) NSDictionary *areaReviewManifest;
@property (nonatomic, strong) NSURL *areaReviewURL;
@property (nonatomic) NSInteger areaReviewIndex;
@property (nonatomic) BOOL openingAreaReviewFrame;
- (void)readRemasterNpcLibrary:(NSURL *)url;
- (void)stopRemasterNpcPlayback;
- (void)configureRemasterNpcAnimationForPart:(NSUInteger)partIndex;
- (NSString *)loadRemasterProfileChecked:(NSURL *)fileURL;
- (BOOL)hasRemasterDebugLightContext;
- (void)showRemasterDebugLight;
- (void)refreshRemasterDebugLightControls;
- (void)changeRemasterDebugLight:(id)sender;
- (void)publishRemasterDebugLight:(const RemasterDebugLight &)light;
- (void)resetRemasterDebugLight;
- (void)showRemasterVariantAtIndex:(size_t)index;
- (void)previousRemasterVariant:(id)sender;
- (void)nextRemasterVariant:(id)sender;
- (void)openRemasterMetadataTool:(id)sender;
- (void)checkRemasterMetadataToolURL:(NSURL *)url attempts:(NSInteger)attempts;
- (BOOL)startRemasterMetadataToolInTerminal;
- (void)finishRemasterMetadataToolWithError:(NSString *)message;
- (void)changeRemasterLayer:(id)sender;
- (void)updateRemasterSemantics:(id)sender;
- (BOOL)remasterTilePixelAtPoint:(NSPoint)point pixel:(size_t *)pixel;
- (void)beginRemasterTileStrokeAtPoint:(NSPoint)point;
- (void)continueRemasterTileStrokeAtPoint:(NSPoint)point;
- (void)endRemasterTileStroke;
- (void)restoreRemasterAssetSnapshot:(S9xRemasterAssetUndoSnapshot *)snapshot;
- (void)changeRemasterPixelValue:(id)sender;
- (void)stepRemasterPixelValue:(id)sender;
- (void)fillRemasterOpaqueOcclusion:(id)sender;
- (void)changeRemasterHeightSampling:(id)sender;
- (void)fillRemasterTileHeight:(id)sender;
- (void)stepRemasterTileHeight:(NSButton *)sender;
- (void)applyRemasterHeightToAnimation:(id)sender;
- (void)changeRemasterDoorframe:(id)sender;
- (void)applyRemasterDoorframeToAnimation:(id)sender;
- (void)changeRemasterNormalPreset:(id)sender;
- (void)changeRemasterNormalValue:(id)sender;
- (void)fillRemasterTileNormal:(id)sender;
- (void)applyRemasterNormalToAnimation:(id)sender;
- (void)changeRemasterMetricsEnabled:(id)sender;
- (void)refreshRemasterMetrics;
- (void)setRemasterEmissionFromVisibleTile:(id)sender;
- (void)changeRemasterEmissionDepth:(id)sender;
- (void)changeRemasterOmnidirectionalEmission:(id)sender;
- (void)combineRemasterEmitters:(id)sender;
- (void)createRemasterObstacleGroup:(id)sender;
- (void)selectRemasterObstacleMember:(id)sender;
- (void)applyRemasterObstacleMember:(id)sender;
- (void)deleteRemasterObstacleGroup:(id)sender;
- (void)showRemasterObstacleGroup:(id)sender;
- (void)refreshRemasterObstacleControls;
- (BOOL)commitRemasterObstacleProfile:(const RemasterProfile &)candidate action:(NSString *)name;
- (void)setRemasterEmissionFromVisibleAnimation:(id)sender;
- (void)changeRemasterPreviewLayers:(id)sender;
- (void)resetRemasterLayer:(id)sender;
- (void)resetRemasterTile:(id)sender;
- (void)copyRemasterLayer:(id)sender;
- (void)pasteRemasterLayer:(id)sender;
- (void)flipRemasterLayerHorizontal:(id)sender;
- (void)flipRemasterLayerVertical:(id)sender;
- (void)flipRemasterLayer:(BOOL)horizontal;
- (void)shiftRemasterLayer:(id)sender;
- (void)copyRemasterTile:(id)sender;
- (void)pasteRemasterTile:(id)sender;
- (void)applyRemasterTileToVariants:(id)sender;
- (void)saveRemasterProfile:(id)sender;
- (void)refreshRemasterProfileSceneControls;
- (void)refreshRemasterAreaSettings;
- (BOOL)changeRemasterAreaSettings:(id)sender;
- (void)saveRemasterAreaSettings:(id)sender;
- (void)changeRemasterBounceCount:(id)sender;
- (void)changeRemasterHeightScale:(id)sender;
- (void)changeRemasterHeightPreviewMultiplier:(id)sender;
- (void)changeRemasterHeightPreviewRange:(id)sender;
- (BOOL)commitRemasterHeightScale;
- (void)changeRemasterCameraDirection:(id)sender;
- (BOOL)commitRemasterCameraDirection;
- (void)changeRemasterIndirectRoughness:(id)sender;
- (BOOL)commitRemasterIndirectRoughness;
- (void)changeRemasterReflectanceBoost:(id)sender;
- (BOOL)commitRemasterReflectanceBoost;
- (void)refreshRemasterEditingControls;
- (void)restoreRemasterProfileFromText:(NSString *)text;
- (void)restoreRemasterSceneSettings:(NSData *)snapshot;
- (void)finishRemasterSceneSettingsChangeFrom:(NSData *)snapshot actionName:(NSString *)name;
- (BOOL)writeRemasterProfile;
- (BOOL)confirmDiscardingRemasterChanges;
@end

@implementation S9xRemasterTileView

- (void)drawRect:(NSRect)dirtyRect
{
	[super drawRect:dirtyRect];
	if (!self.showsNormalAxes)
		return;

	const NSRect widget = NSInsetRect(self.bounds, 4.0, 4.0);
	[[NSColor colorWithCalibratedWhite:0.05 alpha:0.82] setFill];
	[[NSBezierPath bezierPathWithRoundedRect:widget xRadius:5.0 yRadius:5.0] fill];
	const NSPoint origin = NSMakePoint(NSMidX(widget), NSMidY(widget));
	[[NSColor colorWithCalibratedWhite:0.7 alpha:self.enabled ? 0.65 : 0.25] setStroke];
	[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(origin.x - 24.0, origin.y - 24.0, 48.0, 48.0)] stroke];
	auto drawAxis = ^(NSPoint end, NSColor *color, NSString *label)
	{
		[color setStroke];
		NSBezierPath *path = [NSBezierPath bezierPath];
		path.lineWidth = 2.0;
		[path moveToPoint:origin];
		[path lineToPoint:end];
		[path stroke];
		[label drawAtPoint:NSMakePoint(end.x + 2.0, end.y - 6.0) withAttributes:@{
			NSFontAttributeName: [NSFont boldSystemFontOfSize:9.0], NSForegroundColorAttributeName: color }];
	};
	drawAxis(NSMakePoint(origin.x + 22.0, origin.y), [NSColor colorWithCalibratedRed:1.0 green:0.25 blue:0.2 alpha:1.0], @"X");
	drawAxis(NSMakePoint(origin.x, origin.y - 22.0), [NSColor colorWithCalibratedRed:0.25 green:1.0 blue:0.3 alpha:1.0], @"Y");
	[[NSColor colorWithCalibratedRed:0.3 green:0.65 blue:1.0 alpha:1.0] setStroke];
	[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(origin.x - 5.0, origin.y - 5.0, 10.0, 10.0)] stroke];
	[@"Z" drawAtPoint:NSMakePoint(origin.x + 6.0, origin.y + 2.0) withAttributes:@{
		NSFontAttributeName: [NSFont boldSystemFontOfSize:9.0],
		NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:0.3 green:0.65 blue:1.0 alpha:1.0] }];

	const NSPoint normalEnd = NSMakePoint(origin.x + self.normalX * 24.0, origin.y - self.normalY * 24.0);
	[[NSColor colorWithCalibratedRed:1.0 green:0.85 blue:0.15 alpha:1.0] setStroke];
	NSBezierPath *normal = [NSBezierPath bezierPath];
	normal.lineWidth = 3.0;
	[normal moveToPoint:origin];
	[normal lineToPoint:normalEnd];
	[normal stroke];
	[[NSColor colorWithCalibratedRed:1.0 green:0.85 blue:0.15 alpha:1.0] setFill];
	[[NSBezierPath bezierPathWithOvalInRect:NSMakeRect(normalEnd.x - 3.0, normalEnd.y - 3.0, 6.0, 6.0)] fill];
	NSString *normalLabel = [NSString stringWithFormat:@"%@ z%+.2f", self.areaDirection ? @"Light" : @"N", self.normalZ];
	[normalLabel drawAtPoint:NSMakePoint(NSMinX(widget) + 4.0, NSMaxY(widget) - 14.0) withAttributes:@{
		NSFontAttributeName: [NSFont boldSystemFontOfSize:9.0],
		NSForegroundColorAttributeName: [NSColor colorWithCalibratedRed:1.0 green:0.85 blue:0.15 alpha:1.0] }];
}

- (void)mouseDown:(NSEvent *)event
{
	if (self.showsNormalAxes)
	{
		if (!self.enabled)
			return;
		const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
		if (std::hypot(point.x - NSMidX(self.bounds), point.y - NSMidY(self.bounds)) > 24.0)
			return;
		[self.window makeFirstResponder:nil];
		self.trackingNormal = YES;
		[self mouseDragged:event];
		return;
	}
	[self.editor beginRemasterTileStrokeAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseDragged:(NSEvent *)event
{
	if (self.showsNormalAxes)
	{
		if (!self.trackingNormal)
			return;
		const NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
		float x = (point.x - NSMidX(self.bounds)) / 24.0;
		float y = (NSMidY(self.bounds) - point.y) / 24.0;
		const float radius = std::hypot(x, y);
		if (radius > 1.0f)
		{
			x /= radius;
			y /= radius;
		}
		// A unit hemisphere supplies Z, so dragging selects all three components.
		const float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y)) *
			((self.areaDirection ? remasterAreaBack : remasterNormalBackButton).state == NSControlStateValueOn ? -1.0f : 1.0f);
		self.normalX = x;
		self.normalY = y;
		self.normalZ = z;
		if (self.areaDirection)
		{
			remasterAreaDirection[0].floatValue = x;
			remasterAreaDirection[1].floatValue = y;
			remasterAreaDirection[2].floatValue = z;
		}
		else
		{
			remasterNormalXInput.floatValue = x;
			remasterNormalYInput.floatValue = y;
			remasterNormalZInput.floatValue = z;
		}
		[self setNeedsDisplay:YES];
		return;
	}
	[self.editor continueRemasterTileStrokeAtPoint:[self convertPoint:event.locationInWindow fromView:nil]];
}

- (void)mouseUp:(NSEvent *)event
{
	if (self.showsNormalAxes)
	{
		if (self.trackingNormal)
		{
			[self mouseDragged:event];
			self.trackingNormal = NO;
			if (self.areaDirection) [self.editor changeRemasterAreaSettings:self];
			else [self.editor changeRemasterNormalValue:self];
		}
		return;
	}
	[self.editor endRemasterTileStroke];
}

@end

#include "remaster-npc-browser.h"
#include "remaster-overworld-review-methods.h"

@implementation S9xEngine
#include "remaster-npc-browser-methods.h"
#define S9X_AREA_REVIEW_METHODS
#include "remaster-overworld-review-methods.h"
#undef S9X_AREA_REVIEW_METHODS

- (instancetype)init
{
    if (self = [super init])
    {
		remasterUndoManager = [NSUndoManager new];
        Initialize();
		[self recreateS9xView];
    }

    return self;
}

- (void)dealloc
{
	[self stopRemasterNpcPlayback];
	remasterNpcPanel.delegate = nil;
	[self resetRemasterDebugLight];
	remasterDebugLightPanel.delegate = nil;
	remasterDebugLightEnabledButton.target = nil;
	remasterDebugLightColor.target = nil;
	for (NSTextField *input : remasterDebugLightInputs)
		input.target = nil;
	remasterDebugLightPanel = nil;
	remasterDebugLightEnabledButton = nil;
	remasterDebugLightColor = nil;
	for (size_t i = 0; i < 3; i++)
		remasterDebugLightInputs[i] = nil;
    Deinitialize();
}

- (void)recreateS9xView
{
	[s9xView removeFromSuperview];
	S9xDeinitDisplay();
	CGRect frame = NSMakeRect(0, 0, SNES_WIDTH * 2, SNES_HEIGHT * 2);
	s9xView = [[S9xView alloc] initWithFrame:frame];
	s9xView.translatesAutoresizingMaskIntoConstraints = NO;
	s9xView.autoresizingMask = NSViewWidthSizable|NSViewHeightSizable;
	[s9xView addConstraint:[NSLayoutConstraint constraintWithItem:s9xView attribute:NSLayoutAttributeHeight relatedBy:NSLayoutRelationEqual toItem:s9xView attribute:NSLayoutAttributeWidth multiplier:(CGFloat)SNES_HEIGHT/(CGFloat)SNES_WIDTH constant:0.0]];
	[s9xView addConstraint:[NSLayoutConstraint constraintWithItem:s9xView attribute:NSLayoutAttributeWidth relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1.0 constant:SNES_WIDTH * 2.0]];
	[s9xView addConstraint:[NSLayoutConstraint constraintWithItem:s9xView attribute:NSLayoutAttributeHeight relatedBy:NSLayoutRelationGreaterThanOrEqual toItem:nil attribute:NSLayoutAttributeNotAnAttribute multiplier:1.0 constant:SNES_HEIGHT * 2.0]];
	s9xView.device = MTLCreateSystemDefaultDevice();
	s9xView.emulationDelegate = self;
	S9xInitDisplay(NULL, NULL);
}

- (void)start
{
	// A prior loop can have set StopEmulation just before its thread returns.
	dispatch_group_wait(emulationThreadGroup, DISPATCH_TIME_FOREVER);
#ifdef DEBUGGER
	CPU.Flags |= DEBUG_MODE_FLAG;
	S9xDoDebug();
#endif

	lastFrame = GetMicroseconds();
	frameCount = 0;
	if (macFrameSkip < 0)
		skipFrames = 3;
	else
		skipFrames = macFrameSkip;

	S9xInitDisplay(NULL, NULL);

	emulationStopRequested.store(false);
	dispatch_group_enter(emulationThreadGroup);
	[NSThread detachNewThreadWithBlock:^
	{
		MacSnes9xThread(NULL);
		dispatch_group_leave(emulationThreadGroup);
	}];
}

- (void)stop
{
	ClearRemasterDebugLightRedraw();
	[s9xView cancelRemasterDebugLightGesture];
	// Stop the producer before saving SRAM or allowing shared C++ state to die.
	emulationStopRequested.store(true);
	running = false;
	dispatch_group_wait(emulationThreadGroup, DISPATCH_TIME_FOREVER);
	S9xDeinitDisplay(); // Drains queued presentation and Metal completion handlers.
	SNES9X_Quit();
    S9xExit();
}

- (void)softwareReset
{
	ClearRemasterDebugLightRedraw();
	SNES9X_SoftReset();
	SNES9X_Go();
	[self resume];
}


- (void)hardwareReset
{
	ClearRemasterDebugLightRedraw();
	SNES9X_Reset();
	SNES9X_Go();
	[self resume];
}

- (BOOL)isRunning
{
    return running;
}

- (BOOL)isPaused
{
    return running && pauseEmulation;
}

- (BOOL)isRemasterProfileLoaded
{
	return remasterEditingProfileLoaded;
}

- (BOOL)isPresentingRemasterFrame
{
	return remasterFramePresenting;
}

- (BOOL)canShowRemasterTileEditor
{
	return (remasterFramePresenting || remasterNpcInspectorActive) && remasterInspectorPanel != nil;
}

- (void)showRemasterTileEditor
{
	if (![self canShowRemasterTileEditor])
		return;
	[self refreshRemasterEditingControls];
	[remasterInspectorPanel makeKeyAndOrderFront:nil];
}

- (BOOL)isAudioMuted { return MacAudioMuted(); }

- (void)setAudioMuted:(BOOL)muted
{
	MacSetAudioMuted(muted);
	[NSUserDefaults.standardUserDefaults setBool:muted forKey:@"S9xAudioMuted"];
}

- (void)savePreviousWorkspace
{
	if (restoringWorkspace || !running || !workspaceGamePaths.count) return;
	NSMutableDictionary *saved = [@{@"games": workspaceGamePaths,
		@"paused": @(pauseEmulation), @"sceneControls": @(workspaceSceneOpen),
		@"npcPoses": @(workspaceNpcOpen)} mutableCopy];
	if (workspaceProfilePath.length) saved[@"profile"] = workspaceProfilePath;
	[NSUserDefaults.standardUserDefaults setObject:saved forKey:PreviousWorkspaceKey];
}

- (NSString *)restorePreviousWorkspace
{
	NSDictionary *saved = [NSUserDefaults.standardUserDefaults dictionaryForKey:PreviousWorkspaceKey];
	NSArray *paths = saved[@"games"];
	if (![paths isKindOfClass:NSArray.class] || !paths.count || paths.count > 2)
		return @"No previous game has been saved yet. Open a game first.";
	NSMutableArray<NSURL *> *urls = [NSMutableArray array];
	for (id path in paths)
	{
		BOOL directory = NO;
		if (![path isKindOfClass:NSString.class] || ![path length])
			return @"The saved game path is invalid. Open the game again to update it.";
		if (![NSFileManager.defaultManager fileExistsAtPath:path isDirectory:&directory] || directory)
			return [NSString stringWithFormat:@"The previous game could not be found:\n%@\n\nOpen the game from its current location to update the saved workspace.", path];
		[urls addObject:[NSURL fileURLWithPath:path]];
	}
	// Check pending edits before replacing the running game; cancellation is quiet.
	if (remasterEditingProfileLoaded && ![self confirmDiscardingRemasterChanges]) return @"";
	restoringWorkspace = YES;
	workspaceStartPaused.store([saved[@"paused"] boolValue]);
	BOOL opened = urls.count == 1 ? [self loadROM:urls.firstObject] : [self loadMultiple:urls];
	if (!opened)
	{
		restoringWorkspace = NO;
		workspaceStartPaused.store(false);
		return @"The previous game could not be opened. It may be unreadable or no longer a valid ROM.";
	}
	NSMutableArray<NSString *> *errors = [NSMutableArray array];
	NSString *profile = [saved[@"profile"] isKindOfClass:NSString.class] ? saved[@"profile"] : nil;
	if (profile.length)
	{
		NSString *error = [self loadRemasterProfile:[NSURL fileURLWithPath:profile]];
		if (error.length) [errors addObject:[NSString stringWithFormat:@"The game opened, but its profile could not be loaded:\n%@\n%@", profile, error]];
		if (error != nil) workspaceProfilePath = profile; // retain for a later retry
	}
	workspaceSceneOpen = [saved[@"sceneControls"] boolValue];
	workspaceNpcOpen = [saved[@"npcPoses"] boolValue];
	if (!workspaceSceneOpen) [remasterProfileSettingsPanel orderOut:nil];
	if (workspaceNpcOpen) [self showRemasterNpcBrowser];
	else [remasterNpcPanel orderOut:nil];
	if ([saved[@"paused"] boolValue]) [self pause];
	restoringWorkspace = NO;
	[self savePreviousWorkspace];
	return errors.count ? [errors componentsJoinedByString:@"\n\n"] : nil;
}

- (void)pause
{
    pauseEmulation = true;
	[self savePreviousWorkspace];
	[self.emulationDelegate emulationPaused];
    [s9xView updatePauseOverlay];
}

- (void)quit
{
	[self stop];
	[self pause];
}

- (void)resume
{
	// Inspector edits update the retained preview without publishing every stroke
	// to the core. Hand off all session edits before the first resumed frame;
	// saving the profile to disk is deliberately not required.
	if (running && remasterEditingProfileLoaded)
		S9xRemasterSetProfile(remasterEditingProfile);
	remasterCaptureSession.fetch_add(1);
	remasterPausedCapturePending.store(false);
	ClearRemasterDebugLightRedraw();
	remasterFramePresenting = false;
	remasterNpcInspectorActive = false;
	remasterNpcInspectorFrame = RemasterFrame();
	remasterSelectionValid = false;
	remasterHeightInstanceId = 0;
	remasterSelectedTiles.clear();
	remasterObstacleSelection.clear();
	remasterVariants.clear();
	remasterReplayFrame = RemasterFrame();
	remasterSelectedContextInstances.clear();
	[remasterInspectorPanel orderOut:nil];
	SetLiveRemasterPresentation(remasterLightingEnabled && remasterEditingProfileLoaded, remasterLightingView);
	pauseEmulation = false;
	[self savePreviousWorkspace];
	[self.emulationDelegate emulationResumed];
	[s9xView updatePauseOverlay];
}

- (NSArray<S9xJoypad *> *)listJoypads
{
    pthread_mutex_lock(&keyLock);
    NSMutableArray<S9xJoypad *> *joypads = [NSMutableArray new];
    for (auto joypadStruct : ListJoypads())
    {
        S9xJoypad *joypad = [S9xJoypad new];
        joypad.vendorID = joypadStruct.vendorID;
        joypad.productID = joypadStruct.productID;
        joypad.index = joypadStruct.index;
        joypad.name = [[NSString alloc] initWithUTF8String:NameForDevice(joypadStruct).c_str()];

        [joypads addObject:joypad];
    }

    [joypads sortUsingComparator:^NSComparisonResult(S9xJoypad *a, S9xJoypad *b)
    {
        NSComparisonResult result = [a.name compare:b.name];

        if ( result == NSOrderedSame )
        {
            result = [@(a.vendorID) compare:@(b.vendorID)];
        }

        if ( result == NSOrderedSame )
        {
            result = [@(a.productID) compare:@(b.productID)];
        }

        if ( result == NSOrderedSame )
        {
            result = [@(a.index) compare:@(b.index)];
        }

        return result;
    }];
    pthread_mutex_unlock(&keyLock);

    return joypads;
}

- (void)setPlayer:(int8)player forVendorID:(uint32)vendorID productID:(uint32)productID index:(uint32)index oldPlayer:(int8 *)oldPlayer
{
    pthread_mutex_lock(&keyLock);
    SetPlayerForJoypad(player, vendorID, productID, index, oldPlayer);
    pthread_mutex_unlock(&keyLock);
}

- (BOOL)setButton:(S9xButtonCode)button forVendorID:(uint32)vendorID productID:(uint32)productID index:(uint32)index cookie:(uint32)cookie value:(int32)value oldButton:(S9xButtonCode *)oldButton
{
    BOOL result = NO;
    pthread_mutex_lock(&keyLock);
    result = SetButtonCodeForJoypadControl(vendorID, productID, index, cookie, value, button, true, oldButton);
    pthread_mutex_unlock(&keyLock);
    return result;
}

- (void)clearJoypadForVendorID:(uint32)vendorID productID:(uint32)productID index:(uint32)index
{
    pthread_mutex_lock(&keyLock);
    ClearJoypad(vendorID, productID, index);
    pthread_mutex_unlock(&keyLock);
}

- (void)clearJoypadForVendorID:(uint32)vendorID productID:(uint32)productID index:(uint32)index buttonCode:(S9xButtonCode)buttonCode
{
    pthread_mutex_lock(&keyLock);
    ClearButtonCodeForJoypad(vendorID, productID, index, buttonCode);
    pthread_mutex_unlock(&keyLock);
}

- (NSArray<S9xJoypadInput *> *)getInputsForVendorID:(uint32)vendorID productID:(uint32)productID index:(uint32)index
{
    pthread_mutex_lock(&keyLock);
    NSMutableArray<S9xJoypadInput *> *inputs = [NSMutableArray new];
    std::unordered_map<struct JoypadInput, S9xButtonCode> buttonCodeMap = GetJoypadButtons(vendorID, productID, index);
    for (auto it = buttonCodeMap.begin(); it != buttonCodeMap.end(); ++it)
    {
        S9xJoypadInput *input = [S9xJoypadInput new];
        input.cookie = it->first.cookie.cookie;
        input.value = it->first.value;
        input.buttonCode = it->second;

        [inputs addObject:input];
    }
    pthread_mutex_unlock(&keyLock);

    return inputs;
}

- (NSString *)labelForVendorID:(uint32)vendorID productID:(uint32)productID cookie:(uint32)cookie value:(int32)value
{
    return [NSString stringWithUTF8String:LabelForInput(vendorID, productID, cookie, value).c_str()];
}

- (BOOL)setButton:(S9xButtonCode)button forKey:(int16)key player:(int8)player oldButton:(S9xButtonCode *)oldButton oldPlayer:(int8 *)oldPlayer oldKey:(int16 *)oldKey
{
    BOOL result = NO;
    pthread_mutex_lock(&keyLock);
    result = SetKeyCode(key, button, player, oldKey, oldButton, oldPlayer);
    pthread_mutex_unlock(&keyLock);
    return result;
}

- (void)clearButton:(S9xButtonCode)button forPlayer:(int8)player
{
    pthread_mutex_lock(&keyLock);
    ClearKeyCode(button, player);
    pthread_mutex_unlock(&keyLock);
}

- (BOOL)loadROM:(NSURL *)fileURL
{
	if (!restoringWorkspace && remasterEditingProfileLoaded && ![self confirmDiscardingRemasterChanges]) return NO;
	remasterCaptureSession.fetch_add(1);
	remasterPausedCapturePending.store(false);
	S9xRemasterState().requestedInspectableFrames.store(false);
	[self resetRemasterDebugLight];
	SetLiveRemasterPresentation(false, remasterLightingView);
	running = false;
	frzselecting = false;

	while (!Settings.StopEmulation)
	{
		usleep(Settings.FrameTime);
	}

    if ( SNES9X_OpenCart(fileURL) )
    {
		remasterEditingProfileLoaded = false;
		remasterEditingProfileDirty = false;
		remasterFramePresenting = false;
		remasterNpcInspectorActive = false;
		[remasterInspectorPanel orderOut:nil];
		[remasterProfileSettingsPanel orderOut:nil];
		workspaceSceneOpen = NO;
		[self.emulationDelegate gameLoaded];
		
        SNES9X_Go();
        s9xView.window.title = fileURL.lastPathComponent.stringByDeletingPathExtension;
        [s9xView.window makeKeyAndOrderFront:nil];

		dispatch_async(dispatch_get_main_queue(), ^
		{
			[s9xView.window makeFirstResponder:s9xView];
		});

        pauseEmulation = workspaceStartPaused.load();
        [self start];
		workspaceGamePaths = @[fileURL.path];
		workspaceProfilePath = nil;
		[self savePreviousWorkspace];
        return YES;
    }

    return NO;
}

- (void)startRemasterBenchmarkIfRequested
{
	NSArray<NSString *> *arguments = [NSProcessInfo processInfo].arguments;
	if (![arguments containsObject:@"--remaster-benchmark"]) return;
	NSMutableDictionary<NSString *, NSString *> *options = [NSMutableDictionary dictionary];
	NSSet *allowed = [NSSet setWithArray:@[@"--benchmark-rom", @"--benchmark-state", @"--benchmark-profile",
		@"--benchmark-output", @"--benchmark-seconds", @"--benchmark-warmup-seconds", @"--benchmark-move", @"--benchmark-connections",
		@"--benchmark-presentation"]];
	for (NSUInteger i = 1; i < arguments.count; i++)
	{
		NSString *argument = arguments[i];
		if ([argument isEqualToString:@"--remaster-benchmark"]) continue;
		if (![allowed containsObject:argument] || i + 1 >= arguments.count || options[argument])
			FailBenchmark([NSString stringWithFormat:@"Unknown, duplicate, or missing benchmark option: %@", argument]);
		options[argument] = arguments[++i];
	}
	for (NSString *key in @[@"--benchmark-rom", @"--benchmark-state", @"--benchmark-profile", @"--benchmark-output"])
		if (!options[key].length) FailBenchmark([NSString stringWithFormat:@"Required option: %@", key]);
	auto number = [&](NSString *key, double fallback, double minimum, double maximum) {
		if (!options[key]) return fallback;
		NSScanner *scanner = [NSScanner scannerWithString:options[key]];
		double value = 0;
		if (![scanner scanDouble:&value] || !scanner.isAtEnd || !std::isfinite(value) || value < minimum || value > maximum)
			FailBenchmark([NSString stringWithFormat:@"Invalid numeric option: %@", key]);
		return value;
	};
	const double seconds = number(@"--benchmark-seconds", 3, 0.1, 60);
	const double warmup = number(@"--benchmark-warmup-seconds", 0.5, 0, 10);
	const double connections = number(@"--benchmark-connections", 0, 1, 255);
	if (connections != std::floor(connections)) FailBenchmark(@"Connection count must be an integer");
	benchmarkMovement = options[@"--benchmark-move"] ?: @"none";
	NSString *presentation = options[@"--benchmark-presentation"] ?: @"deterministic";
	if (![@[@"deterministic", @"live"] containsObject:presentation]) FailBenchmark(@"Presentation must be deterministic or live");
	benchmarkDeterministicPresentation = [presentation isEqualToString:@"deterministic"];
	if (![@[@"none", @"left", @"right"] containsObject:benchmarkMovement]) FailBenchmark(@"Movement must be none, left, or right");
	NSString *rom = options[@"--benchmark-rom"].stringByExpandingTildeInPath.stringByStandardizingPath;
	NSString *profile = options[@"--benchmark-profile"].stringByExpandingTildeInPath.stringByStandardizingPath;
	benchmarkStatePath = options[@"--benchmark-state"].stringByExpandingTildeInPath.stringByStandardizingPath;
	benchmarkOutputPath = options[@"--benchmark-output"].stringByExpandingTildeInPath.stringByStandardizingPath;
	for (NSString *path in @[rom, profile, benchmarkStatePath])
		if (![[NSFileManager defaultManager] isReadableFileAtPath:path]) FailBenchmark([NSString stringWithFormat:@"Unreadable input: %@", path]);
	if ([[NSFileManager defaultManager] fileExistsAtPath:benchmarkOutputPath]) FailBenchmark(@"Benchmark output directory must not already exist");
	NSError *error = nil;
	if (![[NSFileManager defaultManager] createDirectoryAtPath:benchmarkOutputPath withIntermediateDirectories:YES attributes:nil error:&error])
		FailBenchmark(error.localizedDescription);
	if (getenv("S9X_REMASTER_GI_METRICS")) FailBenchmark(@"Disable S9X_REMASTER_GI_METRICS before benchmarking");
	benchmarkReport = [@{@"schema_version": @1, @"status": @"starting", @"cases": [NSMutableArray array],
		@"executable": [NSBundle mainBundle].executablePath, @"build": @(__DATE__ " " __TIME__),
		@"os": [NSProcessInfo processInfo].operatingSystemVersionString, @"gpu": s9xView.device.name ?: @"unavailable",
		@"rom_path": rom, @"state_path": benchmarkStatePath, @"profile_path": profile,
		@"rom_file_sha256": BenchmarkSha256([NSData dataWithContentsOfFile:rom]),
		@"state_sha256": BenchmarkSha256([NSData dataWithContentsOfFile:benchmarkStatePath]),
		@"profile_sha256": BenchmarkSha256([NSData dataWithContentsOfFile:profile]),
		@"movement": benchmarkMovement, @"presentation": presentation, @"requested_emulated_seconds": @(seconds), @"requested_warmup_seconds": @(warmup),
		@"debug_sphere_enabled": @NO, @"timing_note": @"GPU samples are per completed submitted frame. Input and duration use emulated frames; slow GPU cases take longer in wall time. Deterministic presentation waits for each GPU frame; live presentation retains bounded in-flight slots and drops. Refresh/pacing metrics are latest emulator samples, not guaranteed same-frame."} mutableCopy];
	WriteBenchmarkReport();
	benchmarkActive.store(true);
	benchmarkReport[@"transport_threads_override"] = @(getenv("S9X_REMASTER_TRANSPORT_THREADS") ?: "default 8x8");
	benchmarkReport[@"serial_direct_override"] = @(getenv("S9X_REMASTER_SERIAL_DIRECT") != nullptr);
	benchmarkReport[@"serial_indirect_override"] = @(getenv("S9X_REMASTER_SERIAL_INDIRECT") != nullptr);
	benchmarkReport[@"full_mesh_override"] = @(getenv("S9X_REMASTER_FULL_MESH") != nullptr);
	benchmarkReport[@"stage_counters_disabled"] = @(getenv("S9X_REMASTER_DISABLE_STAGE_COUNTERS") != nullptr);
	benchmarkReport[@"direct_samples_override"] = getenv("S9X_REMASTER_DIRECT_SAMPLES") ?
		[NSString stringWithUTF8String:getenv("S9X_REMASTER_DIRECT_SAMPLES")] : @"";
	benchmarkReport[@"full_sphere_samples_override"] = @(getenv("S9X_REMASTER_FULL_SPHERE_SAMPLES") != nullptr);
	benchmarkReport[@"full_depth_samples_override"] = @(getenv("S9X_REMASTER_FULL_DEPTH_SAMPLES") != nullptr);
	benchmarkReport[@"expanded_sampled_depth_override"] = @(getenv("S9X_REMASTER_EXPANDED_SAMPLED_DEPTH") != nullptr);
	benchmarkReport[@"direct_sampling_note"] = @"Valid override 1..4096 enables per-frame stratified Direct visibility sampling with exact unoccluded normalization; exhaustive fallback for visibility/diagnostic views or enabled debug sphere.";
	benchmarkReport[@"gpu_stage_timing_note"] = @"Benchmark-only GPU timestamp counters at compute encoder boundaries (nanoseconds converted to ms); repeated stages summed per frame. Empty when unsupported. Counter sampling can affect performance; stage sums exclude render/present and inter-encoder gaps.";
	if (running) FailBenchmark(@"Start benchmark in a fresh app process");
	if (!SNES9X_OpenCart([NSURL fileURLWithPath:rom])) FailBenchmark(@"Could not load benchmark ROM");
	// Load the existing profile pipeline without starting emulation until all
	// benchmark settings have been published.
	running = true;
	NSString *profileError = [self loadRemasterProfile:[NSURL fileURLWithPath:profile]];
	running = false;
	if (profileError) FailBenchmark(profileError);
	[remasterProfileSettingsPanel orderOut:nil];
	benchmarkSceneSettings = S9xRemasterGetSceneSettings(remasterEditingProfile);
	if (connections) benchmarkSceneSettings.samplesPerFrame = static_cast<uint8_t>(connections);
	if (const char *value = getenv("S9X_REMASTER_BENCHMARK_REPRESENTATIVES")) {
		char *end = nullptr; long count = strtol(value, &end, 10);
		if (end == value || *end || count < 0 || count > 8) FailBenchmark(@"Representative override must be0..8");
		benchmarkSceneSettings.representativeEmitters = static_cast<uint8_t>(count);
	}
	benchmarkWarmupFrames = static_cast<unsigned>(std::round(warmup * Memory.ROMFramesPerSecond));
	benchmarkMeasureFrames = std::max(1u, static_cast<unsigned>(std::round(seconds * Memory.ROMFramesPerSecond)));
	benchmarkReport[@"connections"] = @(benchmarkSceneSettings.samplesPerFrame);
	benchmarkReport[@"emitter_patch_size"] = @(benchmarkSceneSettings.emitterPatchSize);
	benchmarkReport[@"representative_emitters"] = @(benchmarkSceneSettings.representativeEmitters);
	benchmarkReport[@"audio_muted"] = @YES;
	benchmarkReport[@"warmup_frames"] = @(benchmarkWarmupFrames);
	benchmarkReport[@"measurement_frames"] = @(benchmarkMeasureFrames);
	benchmarkReport[@"rom_fps"] = @(Memory.ROMFramesPerSecond);
	benchmarkReport[@"lighting_coordinate_scale"] = @(benchmarkSceneSettings.lightingCoordinateScale);
	benchmarkReport[@"reflectance_boost"] = @(benchmarkSceneSettings.reflectanceBoost);
	benchmarkReport[@"original_scene_contribution"] = @(benchmarkSceneSettings.originalSceneContribution);
	benchmarkReport[@"sample_accumulation"] = @(benchmarkSceneSettings.sampleAccumulation);
	benchmarkReport[@"window_width"] = @(s9xView.window.frame.size.width);
	benchmarkReport[@"window_height"] = @(s9xView.window.frame.size.height);
	benchmarkReport[@"status"] = @"running";
	WriteBenchmarkReport();
	SetRemasterDebugLight(RemasterDebugLight{});
	Settings.DisplayFrameRate = false;
	Settings.TurboMode = false;
	Settings.SoundSync = false;
	Settings.Mute = true;
	Settings.AutoSaveDelay = 0;
	macFrameSkip = 1;
	benchmarkCase = getenv("S9X_REMASTER_BENCHMARK_COMPOSITE_ONLY") ? 4 : 0;
	S9xRemasterSetPerformanceMetricsEnabled(true);
	[self.emulationDelegate gameLoaded];
	[s9xView.window makeKeyAndOrderFront:nil];
	SNES9X_Go();
	[self start];
}

- (BOOL)loadMultiple:(NSArray<NSURL *> *)fileURLs
{
	if (!restoringWorkspace && remasterEditingProfileLoaded && ![self confirmDiscardingRemasterChanges]) return NO;
	remasterCaptureSession.fetch_add(1);
	remasterPausedCapturePending.store(false);
	S9xRemasterState().requestedInspectableFrames.store(false);
	if (fileURLs.count == 0)
	{
		return NO;
	}
	[self resetRemasterDebugLight];
	SetLiveRemasterPresentation(false, remasterLightingView);

	running = false;
	frzselecting = false;

	while (!Settings.StopEmulation)
	{
		usleep(Settings.FrameTime);
	}

	if (SNES9X_OpenMultiCart(fileURLs.firstObject, fileURLs.lastObject))
	{
		S9xRemasterClearProfile();
		remasterEditingProfileLoaded = false;
		remasterEditingProfileDirty = false;
		remasterFramePresenting = false;
		remasterNpcInspectorActive = false;
		[remasterInspectorPanel orderOut:nil];
		[remasterProfileSettingsPanel orderOut:nil];
		workspaceSceneOpen = NO;
		[self.emulationDelegate gameLoaded];

		SNES9X_Go();
		s9xView.window.title = fileURLs.firstObject.lastPathComponent.stringByDeletingPathExtension;
		[s9xView.window makeKeyAndOrderFront:nil];

		dispatch_async(dispatch_get_main_queue(), ^
		{
			[s9xView.window makeFirstResponder:s9xView];
		});

		pauseEmulation = workspaceStartPaused.load();
		[self start];
		workspaceGamePaths = [fileURLs valueForKey:@"path"];
		workspaceProfilePath = nil;
		[self savePreviousWorkspace];
		return YES;
	}

	return NO;
}

- (void)setShowFPS:(BOOL)showFPS
{
    Settings.DisplayFrameRate = showFPS;
}

- (NSString *)cycleRemasterDebugMode
{
	const RemasterDebugMode mode = S9xRemasterCycleDebugMode();
	remasterReplayDebugMode = mode;
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterSceneFrame, mode,
			remasterSelectionValid && !remasterNpcInspectorActive ? &remasterSelectedTiles : nullptr, remasterLightingEnabled,
			remasterLightingView);
	switch (mode)
	{
		case RemasterDebugMode::Overlay:
			return @"Surface Overlay";
		case RemasterDebugMode::SurfaceIds:
			return @"Surface IDs";
		case RemasterDebugMode::TileTypes:
			return @"Tile Types";
		case RemasterDebugMode::FloorWalls:
			return @"Floors / Wall Distances";
		case RemasterDebugMode::Original:
		default:
			return @"Original";
	}
}

- (BOOL)toggleRemasterLighting
{
	ClearRemasterDebugLightRedraw();
	[s9xView cancelRemasterDebugLightGesture];
	remasterLightingEnabled = !remasterLightingEnabled;
	SetLiveRemasterPresentation(running && remasterLightingEnabled && remasterEditingProfileLoaded,
		remasterLightingView);
	[s9xView updateRemasterDebugOverlay];
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterSceneFrame, remasterReplayDebugMode,
			remasterSelectionValid && !remasterNpcInspectorActive ? &remasterSelectedTiles : nullptr, remasterLightingEnabled,
			remasterLightingView);
	return remasterLightingEnabled;
}

- (NSString *)cycleRemasterLightingView
{
	ClearRemasterDebugLightRedraw();
	const uint32_t next = (static_cast<uint32_t>(remasterLightingView) + 1) %
		static_cast<uint32_t>(RemasterLightingView::Count);
	remasterLightingView = static_cast<RemasterLightingView>(next);
	remasterLightingEnabled = true;
	SetLiveRemasterPresentation(running && remasterEditingProfileLoaded, remasterLightingView);
	[s9xView updateRemasterDebugOverlay];
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterSceneFrame, remasterReplayDebugMode,
			remasterSelectionValid && !remasterNpcInspectorActive ? &remasterSelectedTiles : nullptr, true, remasterLightingView);
	return RemasterLightingViewName(remasterLightingView);
}

- (NSString *)captureRemasterTileInventory
{
	NSURL *applicationSupport = [[NSFileManager defaultManager] URLForDirectory:NSApplicationSupportDirectory
	                                                                    inDomain:NSUserDomainMask
	                                                           appropriateForURL:nil
	                                                                      create:YES
	                                                                       error:nil];
	NSURL *directory = [[applicationSupport URLByAppendingPathComponent:@"Snes9x" isDirectory:YES]
		URLByAppendingPathComponent:@"Remaster" isDirectory:YES];
	if (![[NSFileManager defaultManager] createDirectoryAtURL:directory
	                             withIntermediateDirectories:YES
	                                              attributes:nil
	                                                   error:nil])
		return nil;

	NSURL *file = [directory URLByAppendingPathComponent:@"tile-inventory.json"];
	S9xRemasterRequestTileInventory(file.path.UTF8String);
	return file.path;
}

- (NSString *)captureRemasterFrame
{
	if (!remasterEditingProfileLoaded)
		return nil;
	NSURL *directory = RemasterCaptureDirectory();
	if (!directory)
		return nil;

	NSURL *file = [directory URLByAppendingPathComponent:@"frame.s9xrmf"];
	if (running && pauseEmulation)
	{
		if (remasterFramePresenting)
		{
			std::vector<uint8_t> bytes; std::string written;
			// A replay may be from a different location than current game RAM.
			if (S9xSerializeRemasterFrame(remasterSceneFrame, bytes) &&
				S9xRemasterWriteUniqueCapture(bytes, [directory URLByAppendingPathComponent:@"frame_replay.s9xrmf"].path.fileSystemRepresentation, written))
				return [NSString stringWithUTF8String:written.c_str()];
			return nil;
		}
		std::lock_guard<std::mutex> lock(remasterPausedCaptureMutex);
		remasterPausedCapturePath = file.path;
		remasterPausedCaptureSelect = false;
		remasterPausedCapturePending.store(true);
		return directory.path;
	}
	// Two seconds covers slow ALTTP tile animation without creating an unbounded capture.
	S9xRemasterRequestFrameCapture(file.path.UTF8String, 120);
	return directory.path; // final location/index is chosen by the captured frame
}

- (NSString *)openRemasterFrame:(NSURL *)fileURL
{
	if (running && !pauseEmulation)
		return @"Pause emulation before opening a remaster frame.";
	if (!running && s9xthreadrunning)
		return @"Wait for emulation to stop before opening a remaster frame.";

	RemasterFrame frame;
	if (!S9xReadRemasterFrame(fileURL.path.UTF8String, frame))
		return @"The file is not a valid supported remaster frame capture.";
	if (frame.width > MAX_SNES_WIDTH || frame.height > MAX_SNES_HEIGHT)
		return @"The captured frame dimensions are not supported by this renderer.";
	if (!self.openingAreaReviewFrame && !remasterEditingProfileLoaded && !frame.profileRomSha256.empty())
	{
		NSString *lastPath = [[NSUserDefaults standardUserDefaults] stringForKey:RemasterLastProfilePathKey];
		if (lastPath.length)
		{
			RemasterProfile profile;
			std::vector<RemasterProfileDiagnostic> diagnostics;
			if (S9xRemasterLoadProfile(lastPath.fileSystemRepresentation, profile, diagnostics) &&
				profile.romSha256 == frame.profileRomSha256)
			{
				remasterEditingProfile = std::move(profile);
				remasterEditingProfileURL = [NSURL fileURLWithPath:lastPath];
				remasterProfileDiskSnapshot = [NSData dataWithContentsOfURL:remasterEditingProfileURL];
				remasterPendingProfileConflicts.clear();
				remasterEditingProfileLoaded = true;
				remasterEditingProfileDirty = false;
				remasterSavedSceneSettings = S9xRemasterGetSceneSettings(remasterEditingProfile);
				remasterSavedAreaLights = remasterEditingProfile.areaLights;
				remasterAreaNeedsRefresh = true;
				remasterNonSceneDirty = false;
				S9xRemasterSerializeProfile(remasterEditingProfile, remasterSavedProfileText, diagnostics, false);
				[remasterUndoManager removeAllActions];
			}
		}
	}
	const bool incompatibleProfile = !self.openingAreaReviewFrame && remasterEditingProfileLoaded &&
		frame.profileRomSha256 != remasterEditingProfile.romSha256;
	if (incompatibleProfile && ![self confirmDiscardingRemasterChanges])
		return @"";
	if (!incompatibleProfile && !self.openingAreaReviewFrame)
		SyncRemasterEditingMetadataToFrame(frame);
	const std::pair<int, int> heightRange = S9xRemasterFrameHeightRange(frame);
	remasterHeightPreviewMin = heightRange.first;
	remasterHeightPreviewMax = heightRange.second;
	SetRemasterHeightPreviewRange(static_cast<int32_t>(remasterHeightPreviewMin),
		static_cast<int32_t>(remasterHeightPreviewMax));
	if (remasterHeightPreviewMinInput)
	{
		remasterHeightPreviewMinInput.integerValue = remasterHeightPreviewMin;
		remasterHeightPreviewMaxInput.integerValue = remasterHeightPreviewMax;
	}
	const RemasterDebugLight previousLight = GetRemasterDebugLight();
	ClearRemasterDebugLightRedraw();
	if (!self.openingAreaReviewFrame)
		SetRemasterDebugLight(RemasterDebugLight());
	if (!DrawRemasterFrame(frame, RemasterDebugMode::Original, nullptr, remasterLightingEnabled, remasterLightingView))
	{
		SetRemasterDebugLight(previousLight);
		return @"The captured frame could not be presented.";
	}
	if (!self.openingAreaReviewFrame)
		[self resetRemasterDebugLight];
	if (incompatibleProfile)
	{
		S9xRemasterState().requestedInspectableFrames.store(false);
		remasterEditingProfile = RemasterProfile();
		remasterEditingProfileURL = nil;
		remasterEditingProfileLoaded = false;
		remasterSavedProfileText.clear();
		remasterNonSceneDirty = false;
		[remasterUndoManager removeAllActions];
		[remasterProfileSettingsPanel orderOut:nil];
	}

	remasterNpcInspectorActive = false;
	remasterNpcInspectorFrame = RemasterFrame();
	remasterSceneFrame = std::move(frame);
	remasterHeightInstanceId = 0;
	remasterMetadataNeedsSync = false;
	remasterReplayDebugMode = RemasterDebugMode::Original;
	remasterFramePresenting = true;
	remasterSelectionValid = false;
	remasterVariants.clear();
	[remasterInspectorPanel orderOut:nil];
	[s9xView updatePauseOverlay];
	return nil;
}

- (BOOL)hasRemasterDebugLightContext
{
	if (!remasterLightingEnabled || frzselecting)
		return NO;
	if (remasterFramePresenting)
		return (!running || pauseEmulation) && remasterSceneFrame.width && remasterSceneFrame.height &&
			!remasterSceneFrame.profileRomSha256.empty();
	if (!running || !remasterEditingProfileLoaded || remasterEditingProfile.romSha256.size() != 64)
		return NO;
	static const char hex[] = "0123456789abcdef";
	for (size_t i = 0; i < 32; i++)
		if (remasterEditingProfile.romSha256[i * 2] != hex[Memory.ROMSHA256[i] >> 4] ||
			remasterEditingProfile.romSha256[i * 2 + 1] != hex[Memory.ROMSHA256[i] & 15])
			return NO;
	return YES;
}

- (BOOL)canBeginRemasterDebugLightAtViewPoint:(NSPoint)point
{
	const NSRect bounds = s9xView.bounds;
	return !useMouse && [self hasRemasterDebugLightContext] &&
		std::isfinite(point.x) && std::isfinite(point.y) &&
		NSWidth(bounds) > 0 && NSHeight(bounds) > 0 && NSPointInRect(point, bounds);
}

- (void)updateRemasterDebugLightAtViewPoint:(NSPoint)point toggle:(BOOL)toggle
{
	if (useMouse || ![self hasRemasterDebugLightContext])
		return;
	const NSRect bounds = s9xView.bounds;
	if (!std::isfinite(point.x) || !std::isfinite(point.y) || NSWidth(bounds) <= 0 || NSHeight(bounds) <= 0)
		return;
	RemasterDebugLight light = GetRemasterDebugLight();
	light.x = std::max<CGFloat>(0, std::min<CGFloat>(1, (point.x - NSMinX(bounds)) / NSWidth(bounds)));
	const CGFloat y = s9xView.isFlipped ? point.y - NSMinY(bounds) : NSMaxY(bounds) - point.y;
	light.y = std::max<CGFloat>(0, std::min<CGFloat>(1, y / NSHeight(bounds)));
	if (toggle)
		light.enabled = !light.enabled;
	[self publishRemasterDebugLight:light];
	remasterDebugLightEnabledButton.state = light.enabled ? NSControlStateValueOn : NSControlStateValueOff;
	if (light.enabled && !remasterDebugLightPanel.visible)
		[self showRemasterDebugLight];
}

- (void)adjustRemasterDebugLightHeightByViewDelta:(CGFloat)delta
{
	if (useMouse || ![self hasRemasterDebugLightContext] || !std::isfinite(delta) || NSHeight(s9xView.bounds) <= 0)
		return;
	const unsigned sourceHeight = remasterFramePresenting ? remasterSceneFrame.height : IPPU.RenderedScreenHeight;
	RemasterDebugLight light = GetRemasterDebugLight();
	light.height = std::max<CGFloat>(0, std::min<CGFloat>(4096,
		light.height + delta * sourceHeight / NSHeight(s9xView.bounds)));
	[self publishRemasterDebugLight:light];
	remasterDebugLightInputs[1].stringValue = [NSString stringWithFormat:@"%.6g", light.height];
}

- (void)publishRemasterDebugLight:(const RemasterDebugLight &)light
{
	SetRemasterDebugLight(light);
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
			remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled, remasterLightingView);
	else if (running && pauseEmulation)
		remasterDebugLightRedraw.store(static_cast<uint32_t>(remasterLightingView) + 1);
	// Live emulation consumes the published light on its next presentation.
}

- (void)resetRemasterDebugLight
{
	ClearRemasterDebugLightRedraw();
	[s9xView cancelRemasterDebugLightGesture];
	for (NSTextField *input : remasterDebugLightInputs)
		[input abortEditing];
	[remasterDebugLightColor deactivate];
	[remasterDebugLightPanel orderOut:nil];
	SetRemasterDebugLight(RemasterDebugLight());
	[self refreshRemasterDebugLightControls];
}

- (void)refreshRemasterDebugLightControls
{
	const RemasterDebugLight light = GetRemasterDebugLight();
	remasterDebugLightEnabledButton.state = light.enabled ? NSControlStateValueOn : NSControlStateValueOff;
	const float values[] = { light.radius, light.height, light.intensity };
	for (size_t i = 0; i < 3; i++)
		remasterDebugLightInputs[i].stringValue = [NSString stringWithFormat:@"%.6g", values[i]];
	remasterDebugLightColor.color = [NSColor colorWithSRGBRed:light.red green:light.green blue:light.blue alpha:1];
}

- (void)showRemasterDebugLight
{
	if (!remasterDebugLightPanel)
	{
		remasterDebugLightPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 430, 335)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow
			backing:NSBackingStoreBuffered defer:NO];
		remasterDebugLightPanel.title = @"Remaster Debug Light (Session Only)";
		remasterDebugLightPanel.releasedWhenClosed = NO;
		remasterDebugLightPanel.floatingPanel = YES;
		remasterDebugLightPanel.hidesOnDeactivate = NO;
		remasterDebugLightPanel.delegate = self;
		NSView *content = remasterDebugLightPanel.contentView;
		remasterDebugLightEnabledButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 293, 180, 24)];
		remasterDebugLightEnabledButton.buttonType = NSButtonTypeSwitch;
		remasterDebugLightEnabledButton.title = @"Enabled";
		remasterDebugLightEnabledButton.target = self;
		remasterDebugLightEnabledButton.action = @selector(changeRemasterDebugLight:);
		[content addSubview:remasterDebugLightEnabledButton];
		NSArray<NSString *> *labels = @[ @"Sphere Radius (source pixels)", @"Center Height (source pixels)", @"Intensity (emission units)", @"Color (RGB)" ];
		for (NSInteger i = 0; i < 4; i++)
		{
			const CGFloat y = 252 - i * 36;
			NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(20, y, 280, 24)];
			label.stringValue = labels[i];
			label.editable = NO;
			label.bezeled = NO;
			label.drawsBackground = NO;
			[content addSubview:label];
			if (i < 3)
			{
				NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(310, y, 100, 24)];
				input.alignment = NSTextAlignmentRight;
				input.target = self;
				input.action = @selector(changeRemasterDebugLight:);
				[(NSTextFieldCell *)input.cell setSendsActionOnEndEditing:YES];
				input.toolTip = i == 2 ? @"Floating-point emission intensity: 0 to 10000." :
					@"Native source-pixel units: 0 to 4096. Independent of window size.";
				remasterDebugLightInputs[i] = input;
				[content addSubview:input];
			}
			else
			{
				remasterDebugLightColor = [[NSColorWell alloc] initWithFrame:NSMakeRect(310, y, 100, 26)];
				remasterDebugLightColor.target = self;
				remasterDebugLightColor.action = @selector(changeRemasterDebugLight:);
				[content addSubview:remasterDebugLightColor];
			}
		}
		NSTextField *hint = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 14, 390, 114)];
		hint.stringValue = @"Right-click the scene to toggle at that position.\nRight-drag to move; Cmd-right-drag up/down raises/lowers center height. Dragging keeps the enabled state.\nRadius is sphere extent; height is its center. Emits in all directions.\nRadius/height: 0-4096 source pixels. Intensity: 0-10000.\nSession only; not saved in profiles. Closing keeps the light.";
		hint.editable = NO;
		hint.bezeled = NO;
		hint.drawsBackground = NO;
		hint.font = [NSFont systemFontOfSize:11];
		[content addSubview:hint];
		S9xInitializeWindowPlacement(remasterDebugLightPanel, @"RemasterDebugLight");
	}
	[self refreshRemasterDebugLightControls];
	// Do not take keyboard focus away from gameplay when opening via a gesture.
	[remasterDebugLightPanel orderFront:nil];
}

- (void)changeRemasterDebugLight:(id)sender
{
	if (![self hasRemasterDebugLightContext])
	{
		[self refreshRemasterDebugLightControls];
		return;
	}
	RemasterDebugLight light = GetRemasterDebugLight();
	float *values[] = { &light.radius, &light.height, &light.intensity };
	for (size_t i = 0; i < 3; i++)
	{
		float value;
		NSScanner *scanner = [NSScanner scannerWithString:remasterDebugLightInputs[i].stringValue];
		if (![scanner scanFloat:&value] || !scanner.isAtEnd || !std::isfinite(value))
		{
			NSBeep();
			[self refreshRemasterDebugLightControls];
			return;
		}
		*values[i] = std::max(0.0f, std::min(i == 2 ? 10000.0f : 4096.0f, value));
	}
	NSColor *color = [remasterDebugLightColor.color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
	if (!color || !std::isfinite(color.redComponent) || !std::isfinite(color.greenComponent) ||
		!std::isfinite(color.blueComponent))
	{
		NSBeep();
		[self refreshRemasterDebugLightControls];
		return;
	}
	light.red = std::max<CGFloat>(0, std::min<CGFloat>(1, color.redComponent));
	light.green = std::max<CGFloat>(0, std::min<CGFloat>(1, color.greenComponent));
	light.blue = std::max<CGFloat>(0, std::min<CGFloat>(1, color.blueComponent));
	light.enabled = remasterDebugLightEnabledButton.state == NSControlStateValueOn;
	[self publishRemasterDebugLight:light];
	[self refreshRemasterDebugLightControls];
}

- (void)windowWillClose:(NSNotification *)notification
{
	if (notification.object == remasterNpcPanel) workspaceNpcOpen = NO;
	if (notification.object == remasterProfileSettingsPanel) workspaceSceneOpen = NO;
	[self savePreviousWorkspace];
	if (notification.object == remasterNpcPanel) [self stopRemasterNpcPlayback];
	if (notification.object == remasterDebugLightPanel)
		[remasterDebugLightColor deactivate];
}

- (BOOL)selectRemasterPixelAtViewPoint:(NSPoint)point extendingSelection:(BOOL)extendingSelection
{
	if (!remasterNpcPicking && remasterNpcInspectorActive)
	{
		remasterNpcInspectorActive = false;
		remasterSelectionValid = false;
		remasterSelectedTiles.clear();
		remasterVariants.clear();
		remasterMetadataNeedsSync = true;
		[remasterInspectorPanel orderOut:nil];
		SyncRemasterEditingMetadataToFrame();
	}
	if (!remasterNpcPicking && running && !pauseEmulation && !frzselecting && !useMouse && remasterEditingProfileLoaded)
		[self pause];
	if (!remasterNpcPicking && !remasterFramePresenting && running && pauseEmulation && !frzselecting && !useMouse && remasterEditingProfileLoaded)
	{
		if (!NSPointInRect(point, s9xView.bounds)) return YES;
		NSURL *directory = RemasterCaptureDirectory();
		if (!directory) { NSBeep(); return YES; }
		std::lock_guard<std::mutex> lock(remasterPausedCaptureMutex);
		remasterPausedCapturePath = [[directory URLByAppendingPathComponent:@"frame.s9xrmf"] path];
		remasterPausedCapturePoint = point;
		remasterPausedCaptureSelect = true;
		remasterPausedCapturePending.store(true);
		return YES;
	}
	if ((!remasterFramePresenting && !remasterNpcPicking) || remasterReplayFrame.width == 0 || remasterReplayFrame.height == 0)
		return NO;
	const NSRect bounds = remasterNpcPicking ? NSMakeRect(0, 0, remasterReplayFrame.width, remasterReplayFrame.height) : s9xView.bounds;
	if (NSWidth(bounds) <= 0 || NSHeight(bounds) <= 0 || !NSPointInRect(point, bounds))
		return YES;
	const CGFloat normalizedX = (point.x - NSMinX(bounds)) / NSWidth(bounds);
	const CGFloat normalizedY = ((remasterNpcPicking || s9xView.isFlipped) ? point.y - NSMinY(bounds) : NSMaxY(bounds) - point.y) / NSHeight(bounds);
	if (normalizedX < 0 || normalizedX >= 1 || normalizedY < 0 || normalizedY >= 1)
		return YES;
	const uint32_t x = static_cast<uint32_t>(normalizedX * remasterReplayFrame.width);
	const uint32_t y = static_cast<uint32_t>(normalizedY * remasterReplayFrame.height);
	const size_t offset = static_cast<size_t>(y) * remasterReplayFrame.width + x;
	const RemasterFramePixel &mainPixel = remasterReplayFrame.mainPixels[offset];
	const RemasterFramePixel &subPixel = remasterReplayFrame.subPixels[offset];
	bool instanceFromSubscreen = false;
	const RemasterFrameTileInstance *instance = S9xRemasterFrameVisibleInstanceAt(remasterReplayFrame, x, y,
		&instanceFromSubscreen);
	remasterHeightInstanceId = instance ? static_cast<uint32_t>(instance - remasterReplayFrame.tileInstances.data() + 1) : 0;
	const bool selectingObstacle = !remasterNpcPicking && remasterLayerSelector && remasterLayerSelector.indexOfSelectedItem == RemasterEditorMetadata;
	if (!extendingSelection) remasterObstacleSelection.clear();
	RemasterObstacleEditorCell obstacleCell;
	if (S9xRemasterObstacleEditorCellAt(remasterReplayFrame,x,y,obstacleCell))
	{
		auto selected = std::find_if(remasterObstacleSelection.begin(),remasterObstacleSelection.end(),
			[&](const auto &cell){ return cell.x==obstacleCell.x && cell.y==obstacleCell.y && cell.background==obstacleCell.background; });
		if (selected==remasterObstacleSelection.end()) remasterObstacleSelection.push_back(obstacleCell);
		else if (extendingSelection) remasterObstacleSelection.erase(selected);
	}

	if (remasterSelectLocations && !remasterNpcPicking)
	{
		for(auto selected=remasterSelectedContextInstances.begin();selected!=remasterSelectedContextInstances.end();)
			if(*selected>remasterReplayFrame.tileInstances.size() || !remasterReplayFrame.contextLocations.count(*selected))selected=remasterSelectedContextInstances.erase(selected);else ++selected;
		if (!extendingSelection) remasterSelectedContextInstances.clear();
		if (instance && remasterReplayFrame.contextLocations.count(remasterHeightInstanceId)) {
			if (extendingSelection && remasterSelectedContextInstances.count(remasterHeightInstanceId)) remasterSelectedContextInstances.erase(remasterHeightInstanceId);
			else remasterSelectedContextInstances.insert(remasterHeightInstanceId);
		}
		remasterSelectedTiles.clear();
		for (auto id:remasterSelectedContextInstances) {
			const auto tile=remasterReplayFrame.tileInstances[id-1].tileId;
			if(std::find(remasterSelectedTiles.begin(),remasterSelectedTiles.end(),tile)==remasterSelectedTiles.end())remasterSelectedTiles.push_back(tile);
		}
		remasterReplayFrame.selectedContextInstances=remasterSelectedContextInstances;
		remasterReplayFrame.contextLocationSelection=true;
		remasterTileContextName.clear();
		if(instance) {
			auto location=remasterReplayFrame.contextLocations.find(remasterHeightInstanceId);
			if(location!=remasterReplayFrame.contextLocations.end()) {
				auto binding=remasterEditingProfile.contextBindings.find(location->second);
				if(binding!=remasterEditingProfile.contextBindings.end() && binding->second.tile==instance->tileId)remasterTileContextName=binding->second.name;
			}
		}
	}
	else if (selectingObstacle)
	{
		remasterSelectedTiles.clear();
		for (const auto &cell : remasterObstacleSelection)
			if (std::find(remasterSelectedTiles.begin(),remasterSelectedTiles.end(),cell.tileId)==remasterSelectedTiles.end())
				remasterSelectedTiles.push_back(cell.tileId);
	}
	else if (instance)
	{
		RemasterToggleTileSelection(remasterSelectedTiles, instance->tileId, extendingSelection);
	}
	else if (!extendingSelection)
		remasterSelectedTiles.clear();
	remasterSelectionValid = !remasterSelectedTiles.empty();
	if (!remasterSelectionValid) remasterHeightInstanceId = 0;
	remasterTilePixelSelected = false;
	if (instance && std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), instance->tileId) != remasterSelectedTiles.end())
	{
		remasterGeometryFacing = 1 + S9xRemasterGeometryFlipIndex(instance->hFlip, instance->vFlip);
		const auto &pickedPixel = instanceFromSubscreen ? subPixel : mainPixel;
		if (pickedPixel.tilePixel < 64)
		{
			remasterTilePixelSelected = true;
			remasterSelectedTilePixel = pickedPixel.tilePixel; // rasterizer already canonicalizes H/V
		}
	}
	if (instance && std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), instance->tileId) != remasterSelectedTiles.end())
		remasterSelectedTile = instance->tileId;
	else if (remasterSelectionValid)
		remasterSelectedTile = remasterSelectedTiles.back();
	if (!instance || std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), instance->tileId) == remasterSelectedTiles.end())
	{
		instance = nullptr;
		if (remasterSelectionValid)
			for (const RemasterFrameTileInstance &candidate : remasterReplayFrame.tileInstances)
				if (candidate.tileId == remasterSelectedTile)
				{
					instance = &candidate;
					break;
				}
	}
	DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
		remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled,
		remasterLightingView);

	if (!remasterInspectorPanel)
	{
		remasterInspectorPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 760, 646)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskUtilityWindow
			backing:NSBackingStoreBuffered defer:NO];
		remasterInspectorPanel.title = @"Remaster Pixel Inspector";
		remasterInspectorPanel.releasedWhenClosed = NO;
		remasterInspectorPanel.hidesOnDeactivate = YES;
		NSView *contentView = remasterInspectorPanel.contentView;
		NSTextField *contextLabel = [NSTextField labelWithString:@"Context:"];
		contextLabel.frame = NSMakeRect(20, 570, 62, 22);
		[contentView addSubview:contextLabel];
		remasterTileContextInput = [[NSTextField alloc] initWithFrame:NSMakeRect(85, 568, 250, 24)];
		remasterTileContextInput.target = self;
		remasterTileContextInput.action = @selector(renameRemasterTileContext:);
		remasterTileContextInput.toolTip = @"Enter a name to add a context when None is selected, or rename the current context for all selected tile types. None is reserved.";
		[contentView addSubview:remasterTileContextInput];
		NSArray *contextTitles = @[@"Add Context", @"Remove Context", @"◀", @"▶"];
		SEL contextActions[] = {@selector(addRemasterTileContext:), @selector(removeRemasterTileContext:), @selector(previousRemasterTileContext:), @selector(nextRemasterTileContext:)};
		NSButton * __strong *contextButtons[] = {&remasterAddContextButton, &remasterRemoveContextButton, &remasterPreviousContextButton, &remasterNextContextButton};
		CGFloat contextX[] = {343, 457, 587, 624};
		CGFloat contextW[] = {110, 126, 34, 34};
		for (NSUInteger i=0;i<4;i++) {
			NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(contextX[i], 566, contextW[i], 28)];
			button.title = contextTitles[i]; button.bezelStyle = NSBezelStyleRounded;
			button.target = self; button.action = contextActions[i];
			*contextButtons[i] = button; [contentView addSubview:button];
		}
		remasterContextSelectionMode=[[NSPopUpButton alloc] initWithFrame:NSMakeRect(20,606,180,26) pullsDown:NO];
		[remasterContextSelectionMode addItemsWithTitles:@[@"Select Tile Types", @"Select Tile Locations"]];
		remasterContextSelectionMode.target=self;remasterContextSelectionMode.action=@selector(changeRemasterContextSelection:);
		[contentView addSubview:remasterContextSelectionMode];
		remasterAssignContextButton=[[NSButton alloc] initWithFrame:NSMakeRect(210,604,170,28)];
		remasterAssignContextButton.title=@"Assign Context";remasterAssignContextButton.bezelStyle=NSBezelStyleRounded;
		remasterAssignContextButton.target=self;remasterAssignContextButton.action=@selector(assignRemasterLocationContext:);
		remasterAssignContextButton.toolTip=@"Bind the previewed context to selected world/room locations. None clears bindings. Command-click to select multiple locations.";
		[contentView addSubview:remasterAssignContextButton];
		S9xRemasterTileView *tilePreview = [[S9xRemasterTileView alloc] initWithFrame:NSMakeRect(20, 405, 128, 128)];
		tilePreview.editor = self;
		remasterTilePreview = tilePreview;
		remasterTilePreview.imageFrameStyle = NSImageFrameGrayBezel;
		remasterTilePreview.imageScaling = NSImageScaleAxesIndependently;
		[contentView addSubview:remasterTilePreview];
		remasterArtworkPreview = [[S9xRemasterTileView alloc] initWithFrame:NSMakeRect(20, 265, 128, 128)];
		remasterArtworkPreview.editor = self;
		remasterArtworkPreview.imageFrameStyle = NSImageFrameGrayBezel;
		remasterArtworkPreview.imageScaling = NSImageScaleAxesIndependently;
		remasterArtworkPreview.toolTip = @"Full-brightness palette artwork reference for the tile being edited above.";
		[contentView addSubview:remasterArtworkPreview];
		remasterNormalAxesView = [[S9xRemasterTileView alloc] initWithFrame:NSMakeRect(156, 432, 74, 100)];
		remasterNormalAxesView.imageFrameStyle = NSImageFrameNone;
		remasterNormalAxesView.showsNormalAxes = YES;
		remasterNormalAxesView.editor = self;
		remasterNormalAxesView.hidden = YES;
		[contentView addSubview:remasterNormalAxesView];
		remasterNormalBackButton = [[NSButton alloc] initWithFrame:NSMakeRect(152, 406, 88, 24)];
		remasterNormalBackButton.title = @"Back (-Z)";
		remasterNormalBackButton.font = [NSFont systemFontOfSize:10];
		remasterNormalBackButton.buttonType = NSButtonTypeSwitch;
		remasterNormalBackButton.target = self;
		remasterNormalBackButton.action = @selector(changeRemasterNormalValue:);
		remasterNormalBackButton.toolTip = @"Flip the normal between the front (+Z) and back (-Z) hemispheres.";
		[contentView addSubview:remasterNormalBackButton];
		remasterVariantLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 165, 128, 90)];
		remasterVariantLabel.editable = NO;
		remasterVariantLabel.selectable = YES;
		remasterVariantLabel.bezeled = NO;
		remasterVariantLabel.drawsBackground = NO;
		remasterVariantLabel.alignment = NSTextAlignmentCenter;
		remasterVariantLabel.font = [NSFont systemFontOfSize:11];
		[contentView addSubview:remasterVariantLabel];
		remasterPreviousVariantButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 125, 60, 30)];
		remasterPreviousVariantButton.title = @"Previous";
		remasterPreviousVariantButton.bezelStyle = NSBezelStyleRounded;
		remasterPreviousVariantButton.target = self;
		remasterPreviousVariantButton.action = @selector(previousRemasterVariant:);
		[contentView addSubview:remasterPreviousVariantButton];
		remasterNextVariantButton = [[NSButton alloc] initWithFrame:NSMakeRect(88, 125, 60, 30)];
		remasterNextVariantButton.title = @"Next";
		remasterNextVariantButton.bezelStyle = NSBezelStyleRounded;
		remasterNextVariantButton.target = self;
		remasterNextVariantButton.action = @selector(nextRemasterVariant:);
		[contentView addSubview:remasterNextVariantButton];
		remasterLayerSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(20, 87, 128, 28) pullsDown:NO];
		[remasterLayerSelector addItemsWithTitles:@[@"Artwork", @"Material", @"Occlusion", @"Height", @"Normal", @"Emission", @"Metadata"]];
		remasterLayerSelector.target = self;
		remasterLayerSelector.action = @selector(changeRemasterLayer:);
		[contentView addSubview:remasterLayerSelector];
		remasterDoorframeBrush = [[NSButton alloc] initWithFrame:NSMakeRect(168, 377, 190, 28)];
		remasterDoorframeBrush.title = @"Doorframe pixel (true)";
		[remasterDoorframeBrush setButtonType:NSSwitchButton];
		remasterDoorframeBrush.state = NSControlStateValueOn;
		remasterDoorframeBrush.target = self;
		remasterDoorframeBrush.action = @selector(changeRemasterDoorframe:);
		[contentView addSubview:remasterDoorframeBrush];
		remasterDoorframeAnimationButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 342, 150, 28)];
		remasterDoorframeAnimationButton.title = @"Apply to Animation";
		remasterDoorframeAnimationButton.bezelStyle = NSBezelStyleRounded;
		remasterDoorframeAnimationButton.target = self;
		remasterDoorframeAnimationButton.action = @selector(applyRemasterDoorframeToAnimation:);
		[contentView addSubview:remasterDoorframeAnimationButton];
		remasterSemanticsView = [[NSView alloc] initWithFrame:NSMakeRect(168, 342, 570, 205)];
		[contentView addSubview:remasterSemanticsView];
		NSArray *titles = @[@"Tile role", @"Canonical wall facing", @"Second corner facing", @"Corner type"];
		NSPopUpButton * __strong *menus[] = { &remasterRoleSelector, &remasterFacingSelector, &remasterSecondFacingSelector, &remasterCornerSelector };
		for (NSUInteger i = 0; i < 4; i++)
		{
			NSTextField *label = [NSTextField labelWithString:titles[i]];
			label.frame = NSMakeRect((i % 2) * 280, 183 - (i / 2) * 55, 270, 18);
			[remasterSemanticsView addSubview:label];
			*menus[i] = [[NSPopUpButton alloc] initWithFrame:NSMakeRect((i % 2) * 280, 155 - (i / 2) * 55, 270, 26) pullsDown:NO];
			[remasterSemanticsView addSubview:*menus[i]];
		}
		for (unsigned i = 0; i < static_cast<unsigned>(RemasterTileRole::Count); i++)
			[remasterRoleSelector addItemWithTitle:[NSString stringWithUTF8String:S9xRemasterTileRoleName(static_cast<RemasterTileRole>(i))]];
		for (NSPopUpButton *menu in @[remasterFacingSelector, remasterSecondFacingSelector])
			[menu addItemsWithTitles:@[@"Choose facing…", @"north", @"east", @"south", @"west", @"north_east", @"south_east", @"south_west", @"north_west"]];
		[remasterCornerSelector addItemsWithTitles:@[@"Choose corner…", @"inside", @"outside"]];
		remasterContextDependentButton = [[NSButton alloc] initWithFrame:NSMakeRect(0, 70, 550, 24)];
		remasterContextDependentButton.buttonType = NSButtonTypeSwitch;
		remasterContextDependentButton.title = @"Context-dependent: artwork has different meanings in different placements";
		[remasterSemanticsView addSubview:remasterContextDependentButton];
		remasterSemanticNoteInput = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 38, 550, 26)];
		remasterSemanticNoteInput.placeholderString = @"Optional notes (single line)";
		[remasterSemanticsView addSubview:remasterSemanticNoteInput];
		NSButton *apply = [[NSButton alloc] initWithFrame:NSMakeRect(0, 3, 240, 28)];
		apply.title = @"Apply Metadata to Selected Tiles";
		apply.bezelStyle = NSBezelStyleRounded;
		apply.target = self;
		apply.action = @selector(updateRemasterSemantics:);
		[remasterSemanticsView addSubview:apply];
		remasterSemanticsView.hidden = YES;
		remasterObstacleView = [[NSView alloc] initWithFrame:NSMakeRect(168,20,570,207)];
		remasterObstacleView.bounds = NSMakeRect(0, 0, 570, 242);
		[contentView addSubview:remasterObstacleView];
		remasterObstacleName = [[NSTextField alloc] initWithFrame:NSMakeRect(0,210,240,26)];
		remasterObstacleName.placeholderString = @"Group name (e.g. stone_statue)";
		[remasterObstacleView addSubview:remasterObstacleName];
		remasterObstacleCreate = [[NSButton alloc] initWithFrame:NSMakeRect(245,210,185,28)];
		remasterObstacleCreate.title = @"Create Obstacle Group";
		remasterObstacleCreate.bezelStyle = NSBezelStyleRounded;
		remasterObstacleCreate.target = self;
		remasterObstacleCreate.action = @selector(createRemasterObstacleGroup:);
		[remasterObstacleView addSubview:remasterObstacleCreate];
		remasterObstacleDelete = [[NSButton alloc] initWithFrame:NSMakeRect(435,210,130,28)];
		remasterObstacleDelete.title = @"Delete Group";
		remasterObstacleDelete.bezelStyle = NSBezelStyleRounded;
		remasterObstacleDelete.target = self;
		remasterObstacleDelete.action = @selector(deleteRemasterObstacleGroup:);
		[remasterObstacleView addSubview:remasterObstacleDelete];
		remasterObstacleFootprint = [[NSView alloc] initWithFrame:NSMakeRect(0,45,155,155)];
		[remasterObstacleView addSubview:remasterObstacleFootprint];
		remasterObstacleMembers = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(165,172,395,28) pullsDown:NO];
		remasterObstacleMembers.target = self;
		remasterObstacleMembers.action = @selector(selectRemasterObstacleMember:);
		[remasterObstacleView addSubview:remasterObstacleMembers];
		NSTextField *zLabel = [NSTextField labelWithString:@"Member Z (tile units × 8):"];
		zLabel.frame = NSMakeRect(165,139,235,20);
		[remasterObstacleView addSubview:zLabel];
		remasterObstacleZ = [[NSTextField alloc] initWithFrame:NSMakeRect(405,135,80,26)];
		remasterObstacleZ.stringValue = @"0";
		[remasterObstacleView addSubview:remasterObstacleZ];
		remasterObstacleContact = [[NSButton alloc] initWithFrame:NSMakeRect(165,100,390,26)];
		remasterObstacleContact.buttonType = NSButtonTypeSwitch;
		remasterObstacleContact.title = @"Support contact (cell or south underside)";
		remasterObstacleContact.toolTip = @"This member establishes the group's supporting floor. Whole-group vertical mirrors use the north edge. Authored relief is preserved.";
		[remasterObstacleView addSubview:remasterObstacleContact];
		remasterObstacleApply = [[NSButton alloc] initWithFrame:NSMakeRect(165,65,210,28)];
		remasterObstacleApply.title = @"Apply Member / Group Name";
		remasterObstacleApply.bezelStyle = NSBezelStyleRounded;
		remasterObstacleApply.target = self;
		remasterObstacleApply.action = @selector(applyRemasterObstacleMember:);
		[remasterObstacleView addSubview:remasterObstacleApply];
		remasterObstacleShow = [[NSButton alloc] initWithFrame:NSMakeRect(380,65,180,28)];
		remasterObstacleShow.title = @"Show Entire Group";
		remasterObstacleShow.bezelStyle = NSBezelStyleRounded;
		remasterObstacleShow.target = self;
		remasterObstacleShow.action = @selector(showRemasterObstacleGroup:);
		[remasterObstacleView addSubview:remasterObstacleShow];
		remasterObstacleStatus = [NSTextField wrappingLabelWithString:@""];
		remasterObstacleStatus.frame = NSMakeRect(0,0,560,42);
		remasterObstacleStatus.font = [NSFont systemFontOfSize:11];
		[remasterObstacleView addSubview:remasterObstacleStatus];
		remasterObstacleView.hidden = YES;
		remasterMaterialBrush = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(168, 377, 190, 28) pullsDown:NO];
		[remasterMaterialBrush addItemWithTitle:@"Inherit"];
		if (remasterEditingProfileLoaded)
			for (const auto &entry : remasterEditingProfile.materials)
				[remasterMaterialBrush addItemWithTitle:[NSString stringWithUTF8String:entry.first.c_str()]];
		[contentView addSubview:remasterMaterialBrush];
		remasterValueDownButton = [[NSButton alloc] initWithFrame:NSMakeRect(342, 377, 30, 28)];
		remasterValueDownButton.title = @"↓";
		remasterValueDownButton.bezelStyle = NSBezelStyleRounded;
		remasterValueDownButton.tag = -1;
		remasterValueDownButton.target = self;
		remasterValueDownButton.action = @selector(stepRemasterPixelValue:);
		[contentView addSubview:remasterValueDownButton];
		remasterValueBrush = [[NSSlider alloc] initWithFrame:NSMakeRect(376, 380, 120, 22)];
		remasterValueBrush.minValue = 0;
		remasterValueBrush.maxValue = 255;
		remasterValueBrush.integerValue = 255;
		remasterValueBrush.continuous = NO;
		[contentView addSubview:remasterValueBrush];
		remasterValueUpButton = [[NSButton alloc] initWithFrame:NSMakeRect(500, 377, 30, 28)];
		remasterValueUpButton.title = @"↑";
		remasterValueUpButton.bezelStyle = NSBezelStyleRounded;
		remasterValueUpButton.tag = 1;
		remasterValueUpButton.target = self;
		remasterValueUpButton.action = @selector(stepRemasterPixelValue:);
		[contentView addSubview:remasterValueUpButton];
		remasterValueInput = [[NSTextField alloc] initWithFrame:NSMakeRect(416, 399, 40, 20)];
		remasterValueInput.alignment = NSTextAlignmentCenter;
		remasterValueInput.integerValue = 255;
		remasterValueInput.delegate = self;
		remasterValueInput.target = self;
		remasterValueInput.action = @selector(changeRemasterPixelValue:);
		[contentView addSubview:remasterValueInput];
		remasterValueLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(535, 379, 205, 24)];
		remasterValueLabel.editable = NO;
		remasterValueLabel.bezeled = NO;
		remasterValueLabel.drawsBackground = NO;
		remasterValueLabel.stringValue = @"Select a pixel";
		[remasterValueBrush setTarget:self];
		[remasterValueBrush setAction:@selector(changeRemasterPixelValue:)];
		[contentView addSubview:remasterValueLabel];
		remasterOcclusionFillOpaqueButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 342, 170, 28)];
		remasterOcclusionFillOpaqueButton.title = @"Fill Opaque Pixels";
		remasterOcclusionFillOpaqueButton.bezelStyle = NSBezelStyleRounded;
		remasterOcclusionFillOpaqueButton.target = self;
		remasterOcclusionFillOpaqueButton.action = @selector(fillRemasterOpaqueOcclusion:);
		remasterOcclusionFillOpaqueButton.toolTip = @"Apply the current value to every nontransparent pixel on all selected tile identities.";
		[contentView addSubview:remasterOcclusionFillOpaqueButton];
		remasterHeightSamplingSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(20, 10, 128, 28) pullsDown:NO];
		[remasterHeightSamplingSelector addItemsWithTitles:@[@"Nearest", @"Linear"]];
		remasterHeightSamplingSelector.target = self;
		remasterHeightSamplingSelector.action = @selector(changeRemasterHeightSampling:);
		[contentView addSubview:remasterHeightSamplingSelector];
		remasterHeightPreviewMode = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(168, 342, 156, 28) pullsDown:NO];
		[remasterHeightPreviewMode addItemsWithTitles:@[@"Raw Samples", @"Reconstructed", @"Derived Normals"]];
		remasterHeightPreviewMode.target = self;
		remasterHeightPreviewMode.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterHeightPreviewMode];
		remasterHeightDomainSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(168, 500, 210, 28) pullsDown:NO];
		[remasterHeightDomainSelector addItemsWithTitles:@[@"Authored / Profile", @"Effective Room (read only)"]];
		remasterHeightDomainSelector.target = self;
		remasterHeightDomainSelector.action = @selector(changeRemasterPreviewLayers:);
		remasterHeightDomainSelector.toolTip = @"Authored edits canonical local samples only. Effective shows the selected packet instance, including its signed base and placement override; it never seeds authoring.";
		[contentView addSubview:remasterHeightDomainSelector];
		remasterHeightOccurrenceSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(385, 500, 355, 28) pullsDown:NO];
		remasterHeightOccurrenceSelector.target = self;
		remasterHeightOccurrenceSelector.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterHeightOccurrenceSelector];
		remasterHeightReport = [[NSTextField alloc] initWithFrame:NSMakeRect(168, 425, 572, 70)];
		remasterHeightReport.editable = NO;
		remasterHeightReport.selectable = YES;
		remasterHeightReport.bezeled = NO;
		remasterHeightReport.drawsBackground = NO;
		remasterHeightReport.font = [NSFont systemFontOfSize:12];
		[contentView addSubview:remasterHeightReport];
		remasterHeightArtworkVisibleButton = [[NSButton alloc] initWithFrame:NSMakeRect(330, 342, 100, 28)];
		remasterHeightArtworkVisibleButton.title = @"Artwork";
		remasterHeightArtworkVisibleButton.buttonType = NSButtonTypeSwitch;
		remasterHeightArtworkVisibleButton.state = NSControlStateValueOn;
		remasterHeightArtworkVisibleButton.target = self;
		remasterHeightArtworkVisibleButton.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterHeightArtworkVisibleButton];
		remasterHeightDataVisibleButton = [[NSButton alloc] initWithFrame:NSMakeRect(430, 342, 90, 28)];
		remasterHeightDataVisibleButton.title = @"Height";
		remasterHeightDataVisibleButton.buttonType = NSButtonTypeSwitch;
		remasterHeightDataVisibleButton.state = NSControlStateValueOn;
		remasterHeightDataVisibleButton.target = self;
		remasterHeightDataVisibleButton.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterHeightDataVisibleButton];
		remasterHeightApplyAnimationButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 307, 150, 28)];
		remasterHeightApplyAnimationButton.title = @"Apply to Animation";
		remasterHeightApplyAnimationButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightApplyAnimationButton.target = self;
		remasterHeightApplyAnimationButton.action = @selector(applyRemasterHeightToAnimation:);
		[contentView addSubview:remasterHeightApplyAnimationButton];
		remasterHeightFillTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(325, 307, 130, 28)];
		remasterHeightFillTileButton.title = @"Fill Entire Tile";
		remasterHeightFillTileButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightFillTileButton.target = self;
		remasterHeightFillTileButton.action = @selector(fillRemasterTileHeight:);
		[contentView addSubview:remasterHeightFillTileButton];
		remasterHeightDecreaseTileLargeButton = [[NSButton alloc] initWithFrame:NSMakeRect(460, 307, 68, 28)];
		remasterHeightDecreaseTileLargeButton.title = @"Tile -8";
		remasterHeightDecreaseTileLargeButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightDecreaseTileLargeButton.tag = -8;
		remasterHeightDecreaseTileLargeButton.target = self;
		remasterHeightDecreaseTileLargeButton.action = @selector(stepRemasterTileHeight:);
		remasterHeightDecreaseTileLargeButton.toolTip = @"Decrease every height sample on this tile by eight, clamped at -32768.";
		[contentView addSubview:remasterHeightDecreaseTileLargeButton];
		remasterHeightDecreaseTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(532, 307, 64, 28)];
		remasterHeightDecreaseTileButton.title = @"Tile -1";
		remasterHeightDecreaseTileButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightDecreaseTileButton.tag = -1;
		remasterHeightDecreaseTileButton.target = self;
		remasterHeightDecreaseTileButton.action = @selector(stepRemasterTileHeight:);
		remasterHeightDecreaseTileButton.toolTip = @"Decrease every height sample on this tile by one, clamped at -32768.";
		[contentView addSubview:remasterHeightDecreaseTileButton];
		remasterHeightIncreaseTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(600, 307, 64, 28)];
		remasterHeightIncreaseTileButton.title = @"Tile +1";
		remasterHeightIncreaseTileButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightIncreaseTileButton.tag = 1;
		remasterHeightIncreaseTileButton.target = self;
		remasterHeightIncreaseTileButton.action = @selector(stepRemasterTileHeight:);
		remasterHeightIncreaseTileButton.toolTip = @"Increase every height sample on this tile by one, clamped at 32767.";
		[contentView addSubview:remasterHeightIncreaseTileButton];
		remasterHeightIncreaseTileLargeButton = [[NSButton alloc] initWithFrame:NSMakeRect(668, 307, 68, 28)];
		remasterHeightIncreaseTileLargeButton.title = @"Tile +8";
		remasterHeightIncreaseTileLargeButton.bezelStyle = NSBezelStyleRounded;
		remasterHeightIncreaseTileLargeButton.tag = 8;
		remasterHeightIncreaseTileLargeButton.target = self;
		remasterHeightIncreaseTileLargeButton.action = @selector(stepRemasterTileHeight:);
		remasterHeightIncreaseTileLargeButton.toolTip = @"Increase every height sample on this tile by eight, clamped at 32767.";
		[contentView addSubview:remasterHeightIncreaseTileLargeButton];
		remasterNormalPreset = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(244, 377, 90, 28) pullsDown:NO];
		[remasterNormalPreset addItemsWithTitles:@[@"+Z (Front)", @"-Z (Back)", @"+X (Right)", @"-X (Left)", @"+Y (Down)", @"-Y (Up)", @"Custom"]];
		remasterNormalPreset.target = self;
		remasterNormalPreset.action = @selector(changeRemasterNormalPreset:);
		[contentView addSubview:remasterNormalPreset];
		remasterNormalPixelScopeLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(244, 402, 100, 16)];
		remasterNormalPixelScopeLabel.editable = NO;
		remasterNormalPixelScopeLabel.bezeled = NO;
		remasterNormalPixelScopeLabel.drawsBackground = NO;
		remasterNormalPixelScopeLabel.font = [NSFont boldSystemFontOfSize:11];
		remasterNormalPixelScopeLabel.textColor = [NSColor secondaryLabelColor];
		remasterNormalPixelScopeLabel.stringValue = @"Per-pixel normal";
		[contentView addSubview:remasterNormalPixelScopeLabel];
		NSArray<NSTextField *> *normalInputs = @[
			[[NSTextField alloc] initWithFrame:NSMakeRect(340, 377, 70, 24)],
			[[NSTextField alloc] initWithFrame:NSMakeRect(420, 377, 70, 24)],
			[[NSTextField alloc] initWithFrame:NSMakeRect(500, 377, 70, 24)]
		];
		remasterNormalXInput = normalInputs[0];
		remasterNormalYInput = normalInputs[1];
		remasterNormalZInput = normalInputs[2];
		for (size_t component = 0; component < 3; component++)
		{
			NSTextField *input = normalInputs[component];
			input.placeholderString = @[@"X", @"Y", @"Z"][component];
			input.alignment = NSTextAlignmentCenter;
			input.toolTip = [NSString stringWithFormat:@"%@ component. Enter the complete vector, then click Update. Leave mixed components blank to preserve them.", @[@"X", @"Y", @"Z"][component]];
			[contentView addSubview:input];
		}
		remasterNormalXInput.floatValue = 0.0f;
		remasterNormalYInput.floatValue = 0.0f;
		remasterNormalZInput.floatValue = 1.0f;
		remasterNormalUpdateButton = [[NSButton alloc] initWithFrame:NSMakeRect(578, 375, 80, 28)];
		remasterNormalUpdateButton.title = @"Update";
		remasterNormalUpdateButton.bezelStyle = NSBezelStyleRounded;
		remasterNormalUpdateButton.target = self;
		remasterNormalUpdateButton.action = @selector(changeRemasterNormalValue:);
		remasterNormalUpdateButton.toolTip = @"Normalize and apply XYZ together to the selected pixel on every selected tile.";
		[contentView addSubview:remasterNormalUpdateButton];
		remasterNormalApplyAnimationButton = [[NSButton alloc] initWithFrame:NSMakeRect(244, 342, 150, 28)];
		remasterNormalApplyAnimationButton.title = @"Apply to Animation";
		remasterNormalApplyAnimationButton.bezelStyle = NSBezelStyleRounded;
		remasterNormalApplyAnimationButton.target = self;
		remasterNormalApplyAnimationButton.action = @selector(applyRemasterNormalToAnimation:);
		[contentView addSubview:remasterNormalApplyAnimationButton];
		remasterNormalFillTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(401, 342, 130, 28)];
		remasterNormalFillTileButton.title = @"Fill Entire Tile";
		remasterNormalFillTileButton.bezelStyle = NSBezelStyleRounded;
		remasterNormalFillTileButton.target = self;
		remasterNormalFillTileButton.action = @selector(fillRemasterTileNormal:);
		[contentView addSubview:remasterNormalFillTileButton];
		remasterOppositeFacingDirectButton = [[NSButton alloc] initWithFrame:NSMakeRect(540, 307, 200, 28)];
		remasterOppositeFacingDirectButton.title = @"Tile: Opposite-facing light";
		remasterOppositeFacingDirectButton.buttonType = NSButtonTypeSwitch;
		remasterOppositeFacingDirectButton.allowsMixedState = NO;
		remasterOppositeFacingDirectButton.target = self;
		remasterOppositeFacingDirectButton.action = @selector(changeRemasterOppositeFacingDirect:);
		remasterOppositeFacingDirectButton.toolTip = @"For direct light, also test the normal with X and Y reversed. Indirect lighting is unchanged.";
		[contentView addSubview:remasterOppositeFacingDirectButton];
		remasterNormalIgnoreTransformButton = [[NSButton alloc] initWithFrame:NSMakeRect(244, 307, 288, 28)];
		remasterNormalIgnoreTransformButton.title = @"Tile: Ignore flips/rotation for normals";
		remasterNormalIgnoreTransformButton.font = [NSFont systemFontOfSize:11];
		remasterNormalIgnoreTransformButton.buttonType = NSButtonTypeSwitch;
		remasterNormalIgnoreTransformButton.target = self;
		remasterNormalIgnoreTransformButton.action = @selector(changeRemasterNormalIgnoreTransform:);
		remasterNormalIgnoreTransformButton.toolTip = @"Keep authored normal directions fixed for every placement of this tile. Artwork and per-pixel locations still flip; H/V flips and normal yaw do not change the normal vector. Defaults to off.";
		[contentView addSubview:remasterNormalIgnoreTransformButton];
		remasterGeometryByFacingButton = [[NSButton alloc] initWithFrame:NSMakeRect(244, 500, 285, 28)];
		remasterGeometryByFacingButton.title = @"Tile: Geometry varies by tile flips";
		remasterGeometryByFacingButton.buttonType = NSButtonTypeSwitch;
		remasterGeometryByFacingButton.target = self;
		remasterGeometryByFacingButton.action = @selector(changeRemasterGeometryByFacing:);
		remasterGeometryByFacingButton.toolTip = @"Independent geometry for Normal, Flipped horz, Flipped vert, and Flipped both. Gameplay selects by the tile's H/V flags. Authored normals keep their chosen direction. Animation-wide operations skip this tile. Disabling preserves the maps.";
		[contentView addSubview:remasterGeometryByFacingButton];
		remasterGeometryFacingSelector = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(540, 500, 200, 28) pullsDown:NO];
		[remasterGeometryFacingSelector addItemsWithTitles:@[@"Default / fallback", @"Normal", @"Flipped horz", @"Flipped vert", @"Flipped both"]];
		remasterGeometryFacingSelector.target = self;
		remasterGeometryFacingSelector.action = @selector(changeRemasterGeometryFacing:);
		remasterGeometryFacingSelector.toolTip = @"Both previews match the selected flips. Paint on the displayed pixels; normals are gameplay-space directions (+Y stays +Y). Missing layers use Default in gameplay; editing starts a new layer.";
		[contentView addSubview:remasterGeometryFacingSelector];
		remasterEmissionColor = [[NSColorWell alloc] initWithFrame:NSMakeRect(168, 377, 80, 28)];
		remasterEmissionColor.color = [NSColor colorWithCalibratedRed:1.0 green:0.4 blue:0.1 alpha:1.0];
		[contentView addSubview:remasterEmissionColor];
		remasterEmissionPaintMode = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(252, 377, 120, 28) pullsDown:NO];
		[remasterEmissionPaintMode addItemsWithTitles:@[@"Intensity", @"RGB", @"RGB + Intensity"]];
		[contentView addSubview:remasterEmissionPaintMode];
		remasterEmissionColorScope = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(300, 307, 130, 28) pullsDown:NO];
		[remasterEmissionColorScope addItemsWithTitles:@[@"Selected Pixel", @"Entire Tile"]];
		[remasterEmissionColorScope selectItemAtIndex:1];
		remasterEmissionColorScope.target = self;
		remasterEmissionColorScope.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterEmissionColorScope];
		remasterEmissionFromTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 342, 170, 28)];
		remasterEmissionFromTileButton.title = @"From Visible Tile";
		remasterEmissionFromTileButton.bezelStyle = NSBezelStyleRounded;
		remasterEmissionFromTileButton.target = self;
		remasterEmissionFromTileButton.action = @selector(setRemasterEmissionFromVisibleTile:);
		[contentView addSubview:remasterEmissionFromTileButton];
		remasterEmissionFromAnimationButton = [[NSButton alloc] initWithFrame:NSMakeRect(345, 342, 180, 28)];
		remasterEmissionFromAnimationButton.title = @"From Visible Animation";
		remasterEmissionFromAnimationButton.bezelStyle = NSBezelStyleRounded;
		remasterEmissionFromAnimationButton.target = self;
		remasterEmissionFromAnimationButton.action = @selector(setRemasterEmissionFromVisibleAnimation:);
		[contentView addSubview:remasterEmissionFromAnimationButton];
		// Keep combination authoring on its own row above the paint controls;
		// the visible-color row already contains both reset buttons.
		remasterCombineEmittersButton = [[NSButton alloc] initWithFrame:NSMakeRect(540, 432, 200, 28)];
		remasterCombineEmittersButton.title = @"Combine Emitters";
		remasterCombineEmittersButton.bezelStyle = NSBezelStyleRounded;
		remasterCombineEmittersButton.target = self;
		remasterCombineEmittersButton.action = @selector(combineRemasterEmitters:);
		remasterCombineEmittersButton.toolTip = @"Cmd-click another tile type to select a connected shape. Combines all known animation frames; enable Representative Emitters in Scene Controls. Only cardinally adjacent, explicitly linked tiles combine at runtime.";
		[contentView addSubview:remasterCombineEmittersButton];
		remasterOmnidirectionalEmissionButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 432, 260, 28)];
		remasterOmnidirectionalEmissionButton.title = @"Omnidirectional Emission";
		remasterOmnidirectionalEmissionButton.buttonType = NSButtonTypeSwitch;
		remasterOmnidirectionalEmissionButton.allowsMixedState = YES;
		remasterOmnidirectionalEmissionButton.target = self;
		remasterOmnidirectionalEmissionButton.action = @selector(changeRemasterOmnidirectionalEmission:);
		remasterOmnidirectionalEmissionButton.toolTip = @"Flame-like emission in all directions. Shading normals, receiver response, depth extent, and shadows remain independent.";
		[contentView addSubview:remasterOmnidirectionalEmissionButton];
		remasterEmissionDepthLabel = [[NSTextField alloc] initWithFrame:NSMakeRect(450, 307, 135, 24)];
		remasterEmissionDepthLabel.stringValue = @"Emission Depth";
		remasterEmissionDepthLabel.editable = NO;
		remasterEmissionDepthLabel.bezeled = NO;
		remasterEmissionDepthLabel.drawsBackground = NO;
		[contentView addSubview:remasterEmissionDepthLabel];
		remasterEmissionDepthInput = [[NSTextField alloc] initWithFrame:NSMakeRect(590, 307, 60, 24)];
		remasterEmissionDepthInput.target = self;
		remasterEmissionDepthInput.action = @selector(changeRemasterEmissionDepth:);
		remasterEmissionDepthInput.toolTip = @"0–64 source pixels outward along the authored normal. Applies to selected tiles; zero keeps planar emission.";
		[contentView addSubview:remasterEmissionDepthInput];
		remasterArtworkVisibleButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 307, 120, 28)];
		remasterArtworkVisibleButton.title = @"Artwork";
		remasterArtworkVisibleButton.buttonType = NSButtonTypeSwitch;
		remasterArtworkVisibleButton.state = NSControlStateValueOn;
		remasterArtworkVisibleButton.target = self;
		remasterArtworkVisibleButton.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterArtworkVisibleButton];
		remasterEmissionVisibleButton = [[NSButton alloc] initWithFrame:NSMakeRect(168, 307, 120, 28)];
		remasterEmissionVisibleButton.title = @"Emission";
		remasterEmissionVisibleButton.buttonType = NSButtonTypeSwitch;
		remasterEmissionVisibleButton.state = NSControlStateValueOn;
		remasterEmissionVisibleButton.target = self;
		remasterEmissionVisibleButton.action = @selector(changeRemasterPreviewLayers:);
		[contentView addSubview:remasterEmissionVisibleButton];
		remasterResetLayerButton = [[NSButton alloc] initWithFrame:NSMakeRect(640, 267, 100, 28)];
		remasterResetLayerButton.title = @"Reset Layer";
		remasterResetLayerButton.bezelStyle = NSBezelStyleRounded;
		remasterResetLayerButton.target = self;
		remasterResetLayerButton.action = @selector(resetRemasterLayer:);
		[contentView addSubview:remasterResetLayerButton];
		remasterResetTileButton = [[NSButton alloc] initWithFrame:NSMakeRect(635, 232, 105, 28)];
		remasterResetTileButton.title = @"Reset Tile";
		remasterResetTileButton.bezelStyle = NSBezelStyleRounded;
		remasterResetTileButton.target = self;
		remasterResetTileButton.action = @selector(resetRemasterTile:);
		[contentView addSubview:remasterResetTileButton];
		NSArray<NSString *> *clipboardTitles = @[@"Copy Layer", @"Paste Layer", @"Copy Tile", @"Paste Tile", @"Flip Layer Horiz.", @"Flip Layer Vert."];
		SEL clipboardActions[] = { @selector(copyRemasterLayer:), @selector(pasteRemasterLayer:),
			@selector(copyRemasterTile:), @selector(pasteRemasterTile:), @selector(flipRemasterLayerHorizontal:), @selector(flipRemasterLayerVertical:) };
		const NSRect buttonFrames[] = {NSMakeRect(168, 267, 90, 28), NSMakeRect(264, 267, 90, 28),
			NSMakeRect(168, 232, 90, 28), NSMakeRect(264, 232, 90, 28), NSMakeRect(360, 267, 132, 28), NSMakeRect(498, 267, 132, 28)};
		for (size_t buttonIndex = 0; buttonIndex < 6; buttonIndex++)
		{
			NSButton *button = [[NSButton alloc] initWithFrame:buttonFrames[buttonIndex]];
			button.title = clipboardTitles[buttonIndex];
			button.font = [NSFont systemFontOfSize:10];
			button.bezelStyle = NSBezelStyleRounded;
			button.target = self;
			button.action = clipboardActions[buttonIndex];
			[contentView addSubview:button];
			if (buttonIndex == 0)
				remasterCopyLayerButton = button;
			else if (buttonIndex == 1)
				remasterPasteLayerButton = button;
			else if (buttonIndex == 2)
				remasterCopyTileButton = button;
			else if (buttonIndex == 3)
				remasterPasteTileButton = button;
			else if (buttonIndex == 4)
				remasterFlipLayerHorizontalButton = button;
			else
				remasterFlipLayerVerticalButton = button;
		}
		remasterFlipLayerHorizontalButton.toolTip = remasterFlipLayerVerticalButton.toolTip = @"Mirror the current 8×8 layer on each selected tile. Normals also reflect their X direction horizontally or Y direction vertically. Emission channels retain their values. Supports Undo.";
		NSMutableArray<NSButton *> *shiftButtons = [NSMutableArray new];
		NSArray<NSString *> *shiftTitles = @[@"Shift ↑", @"Shift ↓", @"Shift ←", @"Shift →"];
		for (NSUInteger direction=0;direction<4;direction++) {
			NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(360+direction*69, 197, 66, 28)];
			button.title = shiftTitles[direction]; button.tag = direction;
			button.font = [NSFont systemFontOfSize:10]; button.bezelStyle = NSBezelStyleRounded;
			button.target = self; button.action = @selector(shiftRemasterLayer:);
			button.toolTip = @"Shift the displayed layer by one pixel within each selected 8×8 tile; no wrapping. Exposed edges: height/emission/mask zero, normals +Z, occlusion 255, material default. Supports Undo.";
			[contentView addSubview:button]; [shiftButtons addObject:button];
		}
		remasterShiftLayerButtons = shiftButtons;
		remasterApplyTileToVariantsButton = [[NSButton alloc] initWithFrame:NSMakeRect(360, 232, 264, 28)];
		remasterApplyTileToVariantsButton.title = @"Apply Frame to Other Frames";
		remasterApplyTileToVariantsButton.font = [NSFont systemFontOfSize:10];
		remasterApplyTileToVariantsButton.bezelStyle = NSBezelStyleRounded;
		remasterApplyTileToVariantsButton.target = self;
		remasterApplyTileToVariantsButton.action = @selector(applyRemasterTileToVariants:);
		remasterApplyTileToVariantsButton.toolTip = @"Copy every metadata layer and tile option from the current frame to the other frames.";
		[contentView addSubview:remasterApplyTileToVariantsButton];
		remasterSaveProfileButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 45, 128, 30)];
		remasterSaveProfileButton.title = @"Save Profile";
		remasterSaveProfileButton.bezelStyle = NSBezelStyleRounded;
		remasterSaveProfileButton.target = self;
		remasterSaveProfileButton.action = @selector(saveRemasterProfile:);
		[contentView addSubview:remasterSaveProfileButton];
		remasterMetadataToolButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 8, 210, 30)];
		remasterMetadataToolButton.title = @"Open in HTML Metadata Tool";
		remasterMetadataToolButton.bezelStyle = NSBezelStyleRounded;
		remasterMetadataToolButton.target = self;
		remasterMetadataToolButton.action = @selector(openRemasterMetadataTool:);
		remasterMetadataToolButton.toolTip = @"Open the focused tile in the browser, starting the metadata tool in a visible Terminal window if needed. Control-C in Terminal stops the tool.";
		[contentView addSubview:remasterMetadataToolButton];
		NSScrollView *scrollView = [[NSScrollView alloc] initWithFrame:NSMakeRect(244, 20, 496, 170)];
		remasterInspectorScrollView = scrollView;
		scrollView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
		scrollView.hasVerticalScroller = YES;
		remasterInspectorText = [[NSTextView alloc] initWithFrame:scrollView.contentView.bounds];
		remasterInspectorText.editable = NO;
		remasterInspectorText.selectable = YES;
		remasterInspectorText.font = [NSFont userFixedPitchFontOfSize:12];
		remasterInspectorText.textContainerInset = NSMakeSize(10, 10);
		scrollView.documentView = remasterInspectorText;
		[contentView addSubview:scrollView];
		remasterInspectorPanel.delegate = self;
		S9xInitializeWindowPlacement(remasterInspectorPanel, @"RemasterInspector");
		// Placement restoration includes the old window size. Keep its location,
		// but migrate the fixed-size inspector so the new header is not clipped.
		[remasterInspectorPanel setContentSize:NSMakeSize(760, 646)];
		remasterInspectorPanel.contentMinSize = NSMakeSize(760, 646);
		// Restoring a saved window size can resize this flexible view even
		// though the inspector's controls use fixed coordinates. Restore its
		// reserved bounds last so it cannot cover the layer-shift row.
		remasterInspectorScrollView.frame = NSMakeRect(244, 20, 496, 170);
		[self refreshRemasterEditingControls];
	}

	std::ostringstream text;
	if (remasterNpcPicking)
		text << "NPC pose    " << [remasterNpcActor[@"name"] UTF8String] << " / "
			<< [remasterNpcPose[@"facing"] UTF8String] << " / frame " << [remasterNpcPose[@"step"] integerValue] + 1
			<< "\nDraw-only reference pose; no gameplay placement elevation\n";
	const uint16_t color = remasterReplayFrame.originalRgb555[offset];
	text << "Pixel       (" << x << ", " << y << ")\n";
	text << "RGB555      $" << std::hex << std::setfill('0') << std::setw(4) << color
		<< "  (" << std::dec << ((color >> 10) & 31) << ", " << ((color >> 5) & 31) << ", " << (color & 31) << ")\n";
	text << "Main owner  $" << std::hex << std::setw(8) << mainPixel.owner << std::dec
		<< "  instance " << mainPixel.instanceId << "\n";
	text << "Sub owner   $" << std::hex << std::setw(8) << subPixel.owner << std::dec
		<< "  instance " << subPixel.instanceId << "\n";
	text << "Tile path   " << (instanceFromSubscreen ? "Subscreen color-math contributor" : "Main screen") << "\n";
	if (instance)
	{
		const char *source = instance->source == RemasterSourceType::Background ? "Background" :
			instance->source == RemasterSourceType::Object ? "Object" : "Backdrop";
		const char *match = instance->matchStatus == RemasterProfileMatchStatus::Matched ? "Matched" :
			instance->matchStatus == RemasterProfileMatchStatus::Ambiguous ? "Ambiguous" : "No match";
		const std::vector<uint32_t> occurrences = S9xRemasterFrameOccurrences(remasterReplayFrame, instance->tileId,
			instanceFromSubscreen);
		std::set<uint32_t> occurrenceInstances;
		for (uint32_t occurrence : occurrences)
			occurrenceInstances.insert((instanceFromSubscreen ? remasterReplayFrame.subPixels :
				remasterReplayFrame.mainPixels)[occurrence].instanceId);
		text << "Tile hash   v" << unsigned(instance->tileId.hashVersion) << ':' << unsigned(instance->tileId.bitDepth)
			<< "bpp:" << std::hex << std::setw(16) << instance->tileId.hash << std::dec << "\n";
		text << "Source      " << source << " / " << unsigned(instance->sourceIndex) << "\n";
		text << "Tile        $" << std::hex << std::setw(4) << instance->tileNumber
			<< "  palette " << std::dec << unsigned(instance->palette) << "  VRAM $" << std::hex
			<< std::setw(4) << instance->vramAddress << std::dec << "\n";
		text << "Match       " << match << "  rule line " << instance->ruleLine << "\n";
		text << "Asset group " << (instance->assetGroup.empty() ? "-" : instance->assetGroup) << "\n";
		text << "Material    " << (instance->material.empty() ? "-" : instance->material) << "\n";
		text << "Occurrences " << occurrenceInstances.size() << " instances, " << occurrences.size() << " visible pixels\n";
	}
	else
	{
		text << "Tile        No tile selected\n";
		if (S9xRemasterFrameBackdropGroundAt(remasterReplayFrame, offset))
			text << "Surface     Outdoor backdrop through transparent tile pixels\n"
				<< "Height      Provisional ground at 0; not authored geometry\n"
				<< "Editing     No visible tile identity here; click opaque artwork to edit a tile\n";
	}
	remasterInspectorText.string = [NSString stringWithUTF8String:text.str().c_str()];
	remasterVariants.clear();
	if (instance)
	{
		if (remasterNpcPicking)
		{
			[self configureRemasterNpcAnimationForPart:instance - remasterReplayFrame.tileInstances.data()];
			[self showRemasterVariantAtIndex:remasterVariantIndex];
		}
		else
		{
			remasterVariants = S9xRemasterFrameAnimationVariants(remasterReplayFrame, instance->tileId, instance->assetGroup);
			auto selected = std::find(remasterVariants.begin(), remasterVariants.end(), instance->tileId);
			[self showRemasterVariantAtIndex:selected == remasterVariants.end() ? 0 :
				static_cast<size_t>(selected - remasterVariants.begin())];
		}
	}
	else
	{
		remasterTilePreview.image = nil;
		remasterArtworkPreview.image = nil;
		remasterVariantLabel.stringValue = @"No decoded tile";
		remasterPreviousVariantButton.enabled = NO;
		remasterNextVariantButton.enabled = NO;
	}
	[self refreshRemasterEditingControls];
	[remasterInspectorPanel orderFront:nil];
	return YES;
}

- (void)openRemasterMetadataTool:(id)sender
{
	if (!remasterSelectionValid || remasterMetadataToolOpening)
		return;
	RegisterRemasterSemanticProfile();
	NSString *tileId = [NSString stringWithFormat:@"v%u:%ubpp:%016llx",
		static_cast<unsigned>(remasterSelectedTile.hashVersion),
		static_cast<unsigned>(remasterSelectedTile.bitDepth),
		static_cast<unsigned long long>(remasterSelectedTile.hash)];
	NSURLComponents *url = [NSURLComponents componentsWithString:@"http://127.0.0.1:8765/"];
	url.queryItems = @[[NSURLQueryItem queryItemWithName:@"tile" value:tileId]];
	remasterMetadataToolOpening = true;
	remasterMetadataToolButton.title = @"Opening Metadata Tool…";
	[self refreshRemasterEditingControls];
	[self checkRemasterMetadataToolURL:url.URL attempts:-1];
}

- (void)finishRemasterMetadataToolWithError:(NSString *)message
{
	remasterMetadataToolOpening = false;
	remasterMetadataToolButton.title = @"Open in HTML Metadata Tool";
	[self refreshRemasterEditingControls];
	if (message)
	{
		NSAlert *alert = [NSAlert new];
		alert.messageText = @"Unable to Open Metadata Tool";
		alert.informativeText = message;
		[alert runModal];
	}
}

- (void)checkRemasterMetadataToolURL:(NSURL *)url attempts:(NSInteger)attempts
{
	NSMutableURLRequest *request = [NSMutableURLRequest requestWithURL:
		[NSURL URLWithString:@"http://127.0.0.1:8765/api/status"]
		cachePolicy:NSURLRequestReloadIgnoringLocalCacheData timeoutInterval:1];
	[[[NSURLSession sharedSession] dataTaskWithRequest:request completionHandler:
		^(NSData *data, NSURLResponse *response, NSError *error) {
			NSDictionary *status = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
			const BOOL ready = [status isKindOfClass:[NSDictionary class]] &&
				[status[@"tool"] isEqual:@"alttp-tile-semantics"] && [status[@"protocol_version"] integerValue] >= 2;
			dispatch_async(dispatch_get_main_queue(), ^{
				if (ready)
				{
					BOOL opened = [[NSWorkspace sharedWorkspace] openURL:url];
					[self finishRemasterMetadataToolWithError:opened ? nil : @"The browser could not open the tile URL."];
				}
				else if (!error && attempts == -1)
					[self finishRemasterMetadataToolWithError:@"Port 8765 is occupied by another service or an older metadata tool. Stop it in its Terminal window and try again."];
				else if (attempts == -1)
				{
					if ([self startRemasterMetadataToolInTerminal])
						[self checkRemasterMetadataToolURL:url attempts:60];
					else
						[self finishRemasterMetadataToolWithError:nil];
				}
				else if (attempts > 0)
					dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC / 2), dispatch_get_main_queue(), ^{
						[self checkRemasterMetadataToolURL:url attempts:attempts - 1];
					});
				else
					[self finishRemasterMetadataToolWithError:@"The server did not become ready. Check the visible Metadata Tool Terminal window for the startup error, then try again."];
			});
		}] resume];
}

- (BOOL)startRemasterMetadataToolInTerminal
{
	NSFileManager *files = [NSFileManager defaultManager];
	auto validRoot = [&] (NSString *root) {
		return root && [files fileExistsAtPath:[root stringByAppendingPathComponent:@"tools/ALttP/alttp_tile_semantic_atlas.py"]] &&
			[files fileExistsAtPath:[root stringByAppendingPathComponent:@"build/remaster-geometry/alttp-atlas.sqlite"]];
	};
	NSString *root = [[NSUserDefaults standardUserDefaults] stringForKey:@"RemasterMetadataToolProjectPath"];
	if (!validRoot(root))
		root = [[@(__FILE__) stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
	if (!validRoot(root))
	{
		NSOpenPanel *panel = [NSOpenPanel openPanel];
		panel.title = @"Locate Metadata Tool Project";
		panel.message = @"Choose the Snes9xGI folder containing tools/ALttP/alttp_tile_semantic_atlas.py and build/remaster-geometry/alttp-atlas.sqlite. This location will be remembered.";
		panel.canChooseDirectories = YES;
		panel.canChooseFiles = NO;
		panel.allowsMultipleSelection = NO;
		if ([panel runModal] != NSModalResponseOK)
			return NO;
		root = panel.URL.path;
	}
	if (!validRoot(root))
	{
		[self finishRemasterMetadataToolWithError:@"The selected folder does not contain the metadata tool and its generated atlas database."];
		return NO;
	}
	[[NSUserDefaults standardUserDefaults] setObject:root forKey:@"RemasterMetadataToolProjectPath"];
	NSString *python = @"/usr/bin/python3";
	for (NSString *candidate in @[@"/opt/homebrew/bin/python3", @"/usr/local/bin/python3"])
		if ([files isExecutableFileAtPath:candidate]) { python = candidate; break; }
	auto quote = [] (NSString *value) {
		return [NSString stringWithFormat:@"'%@'", [value stringByReplacingOccurrencesOfString:@"'" withString:@"'\\''"]];
	};
	NSString *script = [NSString stringWithFormat:
		@"#!/bin/zsh\n# Keep the server in the foreground so closing this Terminal session stops it.\n"
		"print -r -- 'Snes9x HTML Metadata Tool — Control-C or close this Terminal session to stop.'\n"
		"exec %@ -B %@ --atlas %@ --annotations %@ --room-index %@ --room-renderer %@ --room-images %@ --port 8765\n",
		quote(python), quote([root stringByAppendingPathComponent:@"tools/ALttP/alttp_tile_semantic_atlas.py"]),
		quote([root stringByAppendingPathComponent:@"build/remaster-geometry/alttp-atlas.sqlite"]),
		quote([root stringByAppendingPathComponent:@"tools/ALttP/alttp-tile-semantics-v1.json"]),
		quote([root stringByAppendingPathComponent:@"build/remaster-geometry/tile-room-occurrences.sqlite"]),
		quote([root stringByAppendingPathComponent:@"build/remaster-geometry/room-preview-engine-v21/alttp-room-preview"]),
		quote([root stringByAppendingPathComponent:@"build/remaster-geometry/room-gallery-room-local/images"])];
	NSURL *directory = [[files URLsForDirectory:NSApplicationSupportDirectory inDomains:NSUserDomainMask].firstObject
		URLByAppendingPathComponent:@"Snes9x/Remaster" isDirectory:YES];
	NSURL *command = [directory URLByAppendingPathComponent:@"HTML Metadata Tool.command"];
	NSError *error = nil;
	if (![files createDirectoryAtURL:directory withIntermediateDirectories:YES attributes:nil error:&error] ||
		![script writeToURL:command atomically:YES encoding:NSUTF8StringEncoding error:&error] ||
		![files setAttributes:@{NSFilePosixPermissions: @0700} ofItemAtPath:command.path error:&error])
	{
		[self finishRemasterMetadataToolWithError:error.localizedDescription];
		return NO;
	}
	if (![[NSWorkspace sharedWorkspace] openFile:command.path withApplication:@"Terminal"])
	{
		[self finishRemasterMetadataToolWithError:@"Terminal could not open the metadata-tool launcher."];
		return NO;
	}
	return YES;
}

- (void)showRemasterVariantAtIndex:(size_t)index
{
	if (index >= remasterVariants.size())
		return;
	if (remasterNpcInspectorActive && index < remasterNpcVariantFrames.size())
	{
		remasterNpcInspectorFrame = remasterNpcVariantFrames[index];
		// Preserve canonical artwork/palettes for off-phase animation edits.
		for (const auto &phase : remasterNpcVariantFrames)
		{
			for (const auto &asset : phase.assets)
				if (!S9xRemasterFrameAssetForTile(remasterNpcInspectorFrame, asset.tileId))
					remasterNpcInspectorFrame.assets.push_back(asset);
			remasterNpcInspectorFrame.tileInstances.insert(remasterNpcInspectorFrame.tileInstances.end(),
				phase.tileInstances.begin(), phase.tileInstances.end());
		}
		remasterMetadataNeedsSync = true;
		remasterNpcPose = remasterNpcVariantPoses[index];
		const NSUInteger poseIndex = [remasterNpcFilteredPoses indexOfObjectIdenticalTo:remasterNpcPose];
		if (poseIndex != NSNotFound) [remasterNpcPoseSelector selectItemAtIndex:poseIndex];
		remasterNpcPreviewFrame = remasterNpcVariantFrames[index];
		[remasterNpcPoseView setNeedsDisplay:YES];
	}
	const std::vector<RemasterTileContentId> previousSelection = remasterSelectedTiles;
	if (!(remasterSelectedTile == remasterVariants[index]))
		remasterTilePixelSelected = false;
	remasterVariantIndex = index;
	const RemasterTileContentId previousFocusedTile = remasterSelectedTile;
	remasterSelectedTile = remasterVariants[index];
	remasterSelectionValid = true;
	auto selected = std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), previousFocusedTile);
	if (selected != remasterSelectedTiles.end())
	{
		auto replacement = std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), remasterSelectedTile);
		if (replacement != remasterSelectedTiles.end() && replacement != selected)
			remasterSelectedTiles.erase(selected);
		else
			*selected = remasterSelectedTile;
	}
	else if (std::find(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), remasterSelectedTile) == remasterSelectedTiles.end())
		remasterSelectedTiles.push_back(remasterSelectedTile);
	const bool metadataChanged = remasterStrokeDragging ? false : SyncRemasterEditingMetadataToFrame();
	if (!remasterStrokeDragging && (metadataChanged || previousSelection != remasterSelectedTiles))
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode, &remasterSelectedTiles,
			remasterLightingEnabled, remasterLightingView);

	const RemasterFrameAsset *asset = S9xRemasterFrameAssetForTile(remasterReplayFrame, remasterSelectedTile);
	const RemasterFrameAssetMetadata *capturedMetadata = S9xRemasterFrameMetadataForTile(remasterReplayFrame, remasterSelectedTile);
	const RemasterAssetMetadata *editableMetadata = nullptr;
	if (remasterEditingProfileLoaded)
	{
		auto found = remasterEditingProfile.assets.find(remasterSelectedTile);
		if (found != remasterEditingProfile.assets.end())
			editableMetadata = &found->second;
	}
	const auto doorMetadata = EffectiveRemasterMetadata(remasterSelectedTile);
	if (doorMetadata.tileRole == RemasterTileRole::Door)
	{
		if (remasterLayerSelector.numberOfItems == RemasterEditorDoorframe)
			[remasterLayerSelector addItemWithTitle:@"Doorframe"];
	}
	else if (remasterLayerSelector.numberOfItems > RemasterEditorDoorframe)
	{
		if (remasterLayerSelector.indexOfSelectedItem == RemasterEditorDoorframe)
			[remasterLayerSelector selectItemAtIndex:RemasterEditorMetadata];
		[remasterLayerSelector removeItemAtIndex:RemasterEditorDoorframe];
	}
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	const bool hasMaterials = (editableMetadata && editableMetadata->hasMaterialSelectors) ||
		(capturedMetadata && capturedMetadata->hasMaterialSelectors);
	const bool hasOcclusion = (editableMetadata && editableMetadata->hasOcclusion) ||
		(capturedMetadata && capturedMetadata->hasOcclusion);
	RemasterHeightSampling sampling;
	const auto *height = RemasterAuthoredHeight(remasterSelectedTile, sampling);
	const bool hasHeight = height != nullptr;
	const auto localPreviewRange = height ? S9xRemasterInspectorLocalPreviewRange(*height) :
		RemasterInspectorHeightPreviewRange{};
	const uint32_t heightInstance = S9xRemasterInspectorHeightInstance(remasterReplayFrame,
		remasterSelectedTile, remasterHeightInstanceId, remasterHeightOccurrences);
	const auto *placement = heightInstance ? &remasterReplayFrame.tileInstances[heightInstance - 1] : nullptr;
	const auto effectiveHeight = S9xRemasterInspectorEffectiveHeight(placement, height);
	const bool effectivePreview = RemasterEffectiveHeightPreview();
	[remasterHeightOccurrenceSelector removeAllItems];
	[remasterHeightOccurrenceSelector addItemWithTitle:remasterHeightOccurrences.empty() ?
		@"No occurrence in this frame" : @"Choose occurrence (ambiguous)"];
	for (uint32_t id : remasterHeightOccurrences)
	{
		const auto &entry = remasterReplayFrame.tileInstances[id - 1];
		[remasterHeightOccurrenceSelector addItemWithTitle:[NSString stringWithFormat:@"Instance %u · %@%u · base %+d · %@%@", id,
			entry.source == RemasterSourceType::Background ? @"BG" : @"OBJ", entry.sourceIndex + 1,
			entry.heightOffset, entry.hFlip ? @"H" : @"", entry.vFlip ? @"V" : @""]];
		if (id == heightInstance) [remasterHeightOccurrenceSelector selectItemAtIndex:remasterHeightOccurrenceSelector.numberOfItems - 1];
	}
	const bool facingSpecific = editableMetadata && editableMetadata->geometryByFlip;
	const auto *editableGeometry = editableMetadata ? &RemasterEditingGeometry(*editableMetadata) : nullptr;
	const bool hasNormals = (editableGeometry && editableGeometry->hasNormals) || (!facingSpecific &&
		(capturedMetadata && capturedMetadata->hasNormals));
	const bool hasEmission = (editableMetadata && editableMetadata->hasEmission) ||
		(capturedMetadata && capturedMetadata->hasEmission);
	const std::array<uint8_t, 192> *normals = hasNormals ?
		(editableGeometry && editableGeometry->hasNormals ? &editableGeometry->normalXyz : &capturedMetadata->normalXyz) : nullptr;
	const bool canRender = asset != nullptr || capturedMetadata != nullptr || remasterEditingProfileLoaded || effectiveHeight.available;
	std::array<uint8_t, 64 * 3> visibleColors = {};
	std::array<uint32_t, 64> visibleColorCounts = {};
	std::array<std::array<uint8_t, 3>, 64> artworkColors = {};
	std::array<bool, 64> artworkVisible = RemasterVisibleTileColors(remasterReplayFrame,
		remasterSelectedTile, artworkColors);
	RemasterFrameArtworkColors paletteArtwork;
	if (S9xRemasterFramePaletteArtwork(remasterReplayFrame, remasterSelectedTile,
		remasterHeightInstanceId, paletteArtwork))
		for (size_t source = 0; source < 64; source++)
		{
			artworkVisible[source] = (paletteArtwork.visiblePixels & (UINT64_C(1) << source)) != 0;
			const uint16_t color = paletteArtwork.rgb555[source];
			artworkColors[source] = {{ static_cast<uint8_t>((((color >> 10) & 31) * 527 + 23) >> 6),
				static_cast<uint8_t>((((color >> 5) & 31) * 527 + 23) >> 6),
				static_cast<uint8_t>(((color & 31) * 527 + 23) >> 6) }};
		}
	for (size_t source = 0; source < 64; source++)
		if (artworkVisible[source])
		{
			visibleColorCounts[source] = 1;
			for (size_t component = 0; component < 3; component++)
				visibleColors[source * 3 + component] = artworkColors[source][component];
		}
	if (canRender)
	{
		const NSInteger size = 128;
		NSBitmapImageRep *artworkBitmap = [[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:nil pixelsWide:size pixelsHigh:size bitsPerSample:8 samplesPerPixel:4
			hasAlpha:YES isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:size * 4 bitsPerPixel:32];
		uint8_t *artworkPixels = artworkBitmap.bitmapData;
		NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
			initWithBitmapDataPlanes:nil pixelsWide:size pixelsHigh:size bitsPerSample:8 samplesPerPixel:4
			hasAlpha:YES isPlanar:NO colorSpaceName:NSCalibratedRGBColorSpace bytesPerRow:size * 4 bitsPerPixel:32];
		uint8_t *pixels = bitmap.bitmapData;
		const unsigned maximum = (1u << std::min<unsigned>(remasterSelectedTile.bitDepth, 8)) - 1;
		const unsigned previewFlip = RemasterInspectorPreviewFlip();
		for (NSInteger y = 0; y < size; y++)
		{
			for (NSInteger x = 0; x < size; x++)
			{
				const size_t source = S9xRemasterGeometryCanonicalPixel(static_cast<unsigned>((y / 16) * 8 + x / 16), previewFlip);
				const uint8_t paletteIndex = asset ? asset->indices[source] : 0;
				const uint8_t decodedArtwork = paletteIndex ?
					static_cast<uint8_t>(48 + paletteIndex * 207 / maximum) :
					(((x / 8) + (y / 8)) & 1 ? 28 : 42);
				uint8_t *artworkPixel = artworkPixels + (y * size + x) * 4;
				for (size_t component = 0; component < 3; component++)
					artworkPixel[component] = visibleColorCounts[source] ?
						visibleColors[source * 3 + component] : decodedArtwork;
				if (remasterTilePixelSelected && source == remasterSelectedTilePixel)
				{
					const NSInteger cellX = x % 16;
					const NSInteger cellY = y % 16;
					const bool outerEdge = cellX == 0 || cellX == 15 || cellY == 0 || cellY == 15;
					const bool innerEdge = cellX == 1 || cellX == 14 || cellY == 1 || cellY == 14;
					if (outerEdge)
						artworkPixel[0] = artworkPixel[1] = artworkPixel[2] = 8;
					else if (innerEdge)
					{
						artworkPixel[0] = 32;
						artworkPixel[1] = 220;
						artworkPixel[2] = 255;
					}
					else
					{
						artworkPixel[0] = static_cast<uint8_t>((artworkPixel[0] * 3 + 32) / 4);
						artworkPixel[1] = static_cast<uint8_t>((artworkPixel[1] * 3 + 220) / 4);
						artworkPixel[2] = static_cast<uint8_t>((artworkPixel[2] * 3 + 255) / 4);
					}
				}
				artworkPixel[3] = 255;
				uint8_t *pixel = pixels + (y * size + x) * 4;
				if (layer == RemasterEditorDoorframe)
				{
					const bool frame = S9xRemasterDoorframePixel(doorMetadata.doorframeMask,
						static_cast<unsigned>(source % 8), static_cast<unsigned>(source / 8));
					for (size_t c = 0; c < 3; c++) pixel[c] = artworkPixel[c];
					if (!doorMetadata.hasDoorframeMask) { pixel[0] = 80; pixel[1] = 20; pixel[2] = 90; }
					else if (frame) { pixel[0] = 240; pixel[1] = 205; pixel[2] = 70; }
				}
				else if (layer == RemasterEditorMaterial && hasMaterials)
				{
					const std::string &name = editableMetadata && editableMetadata->hasMaterialSelectors ? editableMetadata->materialSelectors[source] :
						capturedMetadata->materialSelectors[source];
					if (name.empty())
					{
						pixel[0] = pixel[1] = pixel[2] = ((x / 8) + (y / 8)) & 1 ? 45 : 65;
					}
					else
					{
						uint32_t hash = 2166136261u;
						for (uint8_t byte : name)
							hash = (hash ^ byte) * 16777619u;
						pixel[0] = 64 + (hash & 127);
						pixel[1] = 64 + ((hash >> 8) & 127);
						pixel[2] = 64 + ((hash >> 16) & 127);
					}
				}
				else if (layer == RemasterEditorMaterial)
				{
					pixel[0] = ((x / 8) + (y / 8)) & 1 ? 48 : 72;
					pixel[1] = 28;
					pixel[2] = 28;
				}
				else if (layer == RemasterEditorOcclusion)
				{
					const auto color = S9xRemasterOcclusionDebugColor(editableMetadata && editableMetadata->hasOcclusion ?
						editableMetadata->occlusion[source] : capturedMetadata && capturedMetadata->hasOcclusion ?
						capturedMetadata->occlusion[source] : 255);
					std::copy(color.begin(), color.end(), pixel);
				}
				else if (layer == RemasterEditorHeight)
				{
					const bool showHeight = remasterHeightDataVisibleButton.state == NSControlStateValueOn &&
						(effectivePreview ? effectiveHeight.available : hasHeight);
					uint8_t overlay[3] = {};
					if (showHeight)
					{
						const float sampleX = ((previewFlip & 1 ? size - 1 - x : x) + 0.5f) / 16.0f - 0.5f;
						const float sampleY = ((previewFlip & 2 ? size - 1 - y : y) + 0.5f) / 16.0f - 0.5f;
						const NSInteger preview = remasterHeightPreviewMode.indexOfSelectedItem;
						if (effectivePreview)
						{
							// Canonical orientation matches the artwork/editor; packet placement
							// arrays and tilePixel are already canonical (do not flip twice).
							const float value = (effectiveHeight.at(source) - remasterHeightPreviewMin) /
								static_cast<float>(std::max<NSInteger>(1, remasterHeightPreviewMax - remasterHeightPreviewMin));
							const uint8_t debug = static_cast<uint8_t>(std::lround(std::max(0.0f, std::min(1.0f, value)) * 255));
							overlay[0] = overlay[1] = overlay[2] = debug;
						}
						else if (preview == RemasterHeightPreviewNormals)
						{
							const float dx = SampleRemasterHeight(*height, sampling, sampleX + 0.5f, sampleY) -
								SampleRemasterHeight(*height, sampling, sampleX - 0.5f, sampleY);
							const float dy = SampleRemasterHeight(*height, sampling, sampleX, sampleY + 0.5f) -
								SampleRemasterHeight(*height, sampling, sampleX, sampleY - 0.5f);
							const float length = std::sqrt(dx * dx + dy * dy + 1.0f);
							overlay[0] = static_cast<uint8_t>(std::lround(((previewFlip & 1 ? dx : -dx) / length * 0.5f + 0.5f) * 255.0f));
							overlay[1] = static_cast<uint8_t>(std::lround(((previewFlip & 2 ? dy : -dy) / length * 0.5f + 0.5f) * 255.0f));
							overlay[2] = static_cast<uint8_t>(std::lround((1.0f / length * 0.5f + 0.5f) * 255.0f));
						}
						else
						{
							const float sample = preview == RemasterHeightPreviewRaw ? (*height)[source] :
								SampleRemasterHeight(*height, sampling, sampleX, sampleY) * 255.0f;
							const uint8_t debugValue = localPreviewRange.grayscale(sample);
							overlay[0] = overlay[1] = overlay[2] = debugValue;
						}
					}
					for (size_t component = 0; component < 3; component++)
						pixel[component] = showHeight ? overlay[component] : 0;
					if (!showHeight)
					{
						pixel[0] = 56;
						pixel[1] = 0;
						pixel[2] = 71;
					}
				}
				else if (layer == RemasterEditorNormal)
				{
					if (normals)
					{
						float nx, ny, nz;
						DecodeRemasterNormal(normals->data() + source * 3, nx, ny, nz);
						pixel[0] = static_cast<uint8_t>(std::lround((nx * 0.5f + 0.5f) * 255.0f));
						pixel[1] = static_cast<uint8_t>(std::lround((ny * 0.5f + 0.5f) * 255.0f));
						pixel[2] = static_cast<uint8_t>(std::lround((nz * 0.5f + 0.5f) * 255.0f));
					}
					else
					{
						float nx = 0.0f;
						float ny = 0.0f;
						float nz = 1.0f;
						if (height)
						{
							const size_t sourceX = source % 8;
							const size_t sourceY = source / 8;
							const size_t left = sourceY * 8 + (sourceX ? sourceX - 1 : sourceX);
							const size_t right = sourceY * 8 + std::min<size_t>(7, sourceX + 1);
							const size_t top = (sourceY ? sourceY - 1 : sourceY) * 8 + sourceX;
							const size_t bottom = std::min<size_t>(7, sourceY + 1) * 8 + sourceX;
							const float dx = ((*height)[right] - (*height)[left]) / 255.0f *
								remasterReplayFrame.lightingCoordinateScale;
							const float dy = ((*height)[bottom] - (*height)[top]) / 255.0f *
								remasterReplayFrame.lightingCoordinateScale;
							const float length = std::sqrt(dx * dx + dy * dy + 4.0f);
							nx = (previewFlip & 1 ? dx : -dx) / length;
							ny = (previewFlip & 2 ? dy : -dy) / length;
							nz = 2.0f / length;
						}
						pixel[0] = static_cast<uint8_t>(std::lround((nx * 0.5f + 0.5f) * 255.0f));
						pixel[1] = static_cast<uint8_t>(std::lround((ny * 0.5f + 0.5f) * 255.0f));
						pixel[2] = static_cast<uint8_t>(std::lround((nz * 0.5f + 0.5f) * 255.0f));
					}
				}
				else if (layer == RemasterEditorEmission)
				{
					const bool showEmission = remasterEmissionVisibleButton.state == NSControlStateValueOn && hasEmission;
					const std::array<uint8_t, 256> *emission = showEmission ?
						(editableMetadata && editableMetadata->hasEmission ? &editableMetadata->emissionRgba : &capturedMetadata->emissionRgba) : nullptr;
					const size_t emissionOffset = source * 4;
					const auto preview = emission ? S9xRemasterEmissionPreview(emission->data() + emissionOffset) :
						std::array<uint8_t, 3>{};
					for (size_t component = 0; component < 3; component++)
					{
						pixel[component] = preview[component];
					}
				}
				else if (layer == RemasterEditorArtwork)
				{
					for (size_t component = 0; component < 3; component++)
						pixel[component] = visibleColorCounts[source] ? visibleColors[source * 3 + component] : decodedArtwork;
				}
				else
				{
					pixel[0] = ((x / 8) + (y / 8)) & 1 ? 48 : 72;
					pixel[1] = 28;
					pixel[2] = 28;
				}
				if (remasterTilePixelSelected && source == remasterSelectedTilePixel)
				{
					const NSInteger cellX = x % 16;
					const NSInteger cellY = y % 16;
					const bool outerEdge = cellX == 0 || cellX == 15 || cellY == 0 || cellY == 15;
					const bool innerEdge = cellX == 1 || cellX == 14 || cellY == 1 || cellY == 14;
					if (outerEdge)
						pixel[0] = pixel[1] = pixel[2] = 8;
					else if (innerEdge)
					{
						pixel[0] = 32;
						pixel[1] = 220;
						pixel[2] = 255;
					}
					else
					{
						pixel[0] = static_cast<uint8_t>((pixel[0] * 3 + 32) / 4);
						pixel[1] = static_cast<uint8_t>((pixel[1] * 3 + 220) / 4);
						pixel[2] = static_cast<uint8_t>((pixel[2] * 3 + 255) / 4);
					}
				}
				pixel[3] = 255;
			}
		}
		NSImage *image = [[NSImage alloc] initWithSize:NSMakeSize(size, size)];
		[image addRepresentation:bitmap];
		remasterTilePreview.image = image;
		NSImage *artworkImage = [[NSImage alloc] initWithSize:NSMakeSize(size, size)];
		[artworkImage addRepresentation:artworkBitmap];
		remasterArtworkPreview.image = artworkImage;
	}
	else
	{
		remasterTilePreview.image = nil;
		remasterArtworkPreview.image = nil;
	}

	const std::vector<uint32_t> occurrences = S9xRemasterFrameOccurrences(remasterReplayFrame, remasterSelectedTile);
	if (layer == RemasterEditorHeight)
		[remasterHeightSamplingSelector selectItemAtIndex:sampling == RemasterHeightSampling::Linear ? 1 : 0];
	const bool oppositeFacingDirect = !remasterSelectedTiles.empty() &&
		EffectiveRemasterOppositeFacing(remasterSelectedTiles.front());
	bool oppositeFacingMixed = false;
	if (!remasterSelectedTiles.empty())
	{
		const bool first = EffectiveRemasterOppositeFacing(remasterSelectedTiles.front());
		for (const RemasterTileContentId &tileId : remasterSelectedTiles)
			oppositeFacingMixed |= EffectiveRemasterOppositeFacing(tileId) != first;
	}
	remasterOppositeFacingDirectButton.state = oppositeFacingMixed ? NSControlStateValueMixed :
		(oppositeFacingDirect ? NSControlStateValueOn : NSControlStateValueOff);
	bool fixedDirection = false, fixedMixed = false, firstFixed = true;
	for (const auto &tile : remasterSelectedTiles)
	{
		const bool value = EffectiveRemasterMetadata(tile).normalIgnoreTileTransform;
		if (firstFixed) { fixedDirection = value; firstFixed = false; }
		else fixedMixed |= value != fixedDirection;
	}
	remasterNormalIgnoreTransformButton.state = fixedMixed ? NSControlStateValueMixed :
		(fixedDirection ? NSControlStateValueOn : NSControlStateValueOff);
	NSString *layerStatus = [NSString stringWithFormat:@"metadata: material %@, occlusion %@, height %@ (%@), normal %@, emission %@%@%@",
		hasMaterials ? @"yes" : @"no", hasOcclusion ? @"yes" : @"default 255", hasHeight ? @"yes" : @"no",
		sampling == RemasterHeightSampling::Linear ? @"linear" : @"nearest", hasNormals ? @"yes" : @"no",
		hasEmission ? @"yes" : @"no",
		remasterEditingProfileDirty ? @", unsaved" : @"",
		remasterEditingProfileLoaded ? @"" : @"\nRead only; load profile to edit"];
	remasterVariantLabel.stringValue = [NSString stringWithFormat:@"Frame %lu of %lu\nv%u:%ubpp:%016llx\n%@, %lu visible pixels%@\n%@",
		static_cast<unsigned long>(index + 1), static_cast<unsigned long>(remasterVariants.size()),
		static_cast<unsigned>(remasterSelectedTile.hashVersion), static_cast<unsigned>(remasterSelectedTile.bitDepth),
		static_cast<unsigned long long>(remasterSelectedTile.hash),
		asset ? @"decoded indices" : @"not decoded in capture", static_cast<unsigned long>(occurrences.size()),
		remasterSelectedTiles.size() > 1 ? [NSString stringWithFormat:@", %lu selected types",
			static_cast<unsigned long>(remasterSelectedTiles.size())] : @"", layerStatus];
	remasterPreviousVariantButton.enabled = remasterVariants.size() > 1;
	remasterNextVariantButton.enabled = remasterVariants.size() > 1;
	[self refreshRemasterEditingControls];
}

- (void)changeRemasterLayer:(id)sender
{
	if (remasterLayerSelector.indexOfSelectedItem == RemasterEditorEmission && !remasterTilePixelSelected)
	{
		remasterValueBrush.integerValue = 25;
		remasterValueInput.integerValue = 25;
	}
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)updateRemasterSemantics:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	RemasterAssetMetadata annotation;
	annotation.hasTileSemantics = true;
	annotation.tileRole = static_cast<RemasterTileRole>(remasterRoleSelector.indexOfSelectedItem);
	const size_t expected = annotation.tileRole == RemasterTileRole::Wall ? 1 :
		(annotation.tileRole == RemasterTileRole::WallCorner ? 2 : 0);
	if (expected)
	{
		if (remasterFacingSelector.indexOfSelectedItem <= 0) { NSBeep(); return; }
		annotation.tileSemanticNormals.push_back(remasterFacingSelector.titleOfSelectedItem.UTF8String);
	}
	if (expected == 2)
	{
		if (remasterSecondFacingSelector.indexOfSelectedItem <= 0 || remasterCornerSelector.indexOfSelectedItem <= 0 ||
			remasterFacingSelector.indexOfSelectedItem > 4 || remasterSecondFacingSelector.indexOfSelectedItem > 4 ||
			remasterFacingSelector.indexOfSelectedItem == remasterSecondFacingSelector.indexOfSelectedItem) { NSBeep(); return; }
		annotation.tileSemanticNormals.push_back(remasterSecondFacingSelector.titleOfSelectedItem.UTF8String);
		annotation.tileSemanticCorner = remasterCornerSelector.titleOfSelectedItem.UTF8String;
	}
	annotation.tileSemanticContextDependent = remasterContextDependentButton.state == NSControlStateValueOn;
	annotation.tileSemanticNote = remasterSemanticNoteInput.stringValue.UTF8String;
	if (annotation.tileSemanticNote.size() > 4000) { NSBeep(); return; }
	std::set<RemasterTileContentId> touched(remasterSelectedTiles.begin(), remasterSelectedTiles.end());
	auto snapshot = CaptureRemasterAssetSnapshot(touched);
	for (const auto &id : touched)
	{
		auto &metadata = remasterEditingProfile.assets[id];
		metadata.tileId = id;
		CopyRemasterMetadataLayer(annotation, metadata, RemasterEditorMetadata);
	}
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, RemasterMetadataSchemaVersion(annotation));
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
	[remasterUndoManager setActionName:@"Edit Tile Metadata"];
	remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterPreviewLayers:(id)sender
{
	if (sender == remasterHeightOccurrenceSelector)
	{
		const NSInteger index = remasterHeightOccurrenceSelector.indexOfSelectedItem;
		remasterHeightInstanceId = index > 0 && static_cast<size_t>(index) <= remasterHeightOccurrences.size() ?
			remasterHeightOccurrences[index - 1] : 0;
	}
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)resetRemasterLayer:(id)sender
{
	if (!remasterTileContextName.empty() && (remasterLayerSelector.indexOfSelectedItem == RemasterEditorHeight || remasterLayerSelector.indexOfSelectedItem == RemasterEditorNormal)) {
		std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
		for (const auto &tile : remasterSelectedTiles) if (auto *c = RemasterSelectedContext(tile)) {
			if (remasterLayerSelector.indexOfSelectedItem == RemasterEditorHeight) { c->hasHeight = false; c->height = {}; c->heightSampling = RemasterHeightSampling::Nearest; }
			else { c->hasNormals = false; c->normalXyz = {}; }
		}
		[self finishRemasterContextEdit:@(before.c_str()) action:@"Reset Context Layer"];
		return;
	}
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty())
		return;
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (layer == RemasterEditorMetadata)
	{
		[remasterRoleSelector selectItemAtIndex:0];
		[remasterFacingSelector selectItemAtIndex:0];
		[remasterSecondFacingSelector selectItemAtIndex:0];
		remasterContextDependentButton.state = NSControlStateValueOff;
		remasterSemanticNoteInput.stringValue = @"";
		[self updateRemasterSemantics:sender];
		return;
	}
	if (layer == RemasterEditorHeight || layer == RemasterEditorNormal)
	{
		std::set<RemasterTileContentId> touched(remasterSelectedTiles.begin(), remasterSelectedTiles.end());
		auto snapshot = CaptureRemasterAssetSnapshot(touched);
		bool changed = false;
		for (const auto &tile : touched)
		{
			auto found = remasterEditingProfile.assets.find(tile);
			if (found == remasterEditingProfile.assets.end()) continue;
			auto &g = RemasterEditingGeometry(found->second);
			if (layer == RemasterEditorHeight) { changed |= g.hasHeight; g.hasHeight = false; g.heightSampling = RemasterHeightSampling::Nearest; }
			else { changed |= g.hasNormals; g.hasNormals = false; }
			if (!RemasterMetadataHasData(found->second)) remasterEditingProfile.assets.erase(found);
		}
		if (!changed) return;
		[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
		[remasterUndoManager setActionName:@"Reset Geometry Layer"];
		remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
		SyncRemasterEditingMetadataToFrame();
		[self showRemasterVariantAtIndex:remasterVariantIndex];
		[remasterNpcPoseView setNeedsDisplay:YES];
		return;
	}
	if (layer == RemasterEditorNormal && remasterSelectedTiles.size() > 1)
	{
		std::string before;
		std::vector<RemasterProfileDiagnostic> diagnostics;
		S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
		bool changed = false;
		for (const RemasterTileContentId &tileId : remasterSelectedTiles)
		{
			auto selected = remasterEditingProfile.assets.find(tileId);
			if (selected == remasterEditingProfile.assets.end() || !selected->second.hasNormals)
				continue;
			changed = true;
			selected->second.hasNormals = false;
			if (!selected->second.hasMaterialSelectors && !selected->second.hasOcclusion && !selected->second.hasHeight &&
				!selected->second.hasEmission && selected->second.emissionDepth == 0 && !selected->second.emissionOmnidirectional && selected->second.combineEmitters.empty() && !selected->second.directLightingOppositeFacing && !selected->second.normalIgnoreTileTransform && !selected->second.hasTileSemantics && selected->second.tileRole == RemasterTileRole::Unknown)
				remasterEditingProfile.assets.erase(selected);
		}
		if (!changed)
			return;
		[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
			object:[NSString stringWithUTF8String:before.c_str()]];
		[remasterUndoManager setActionName:@"Reset Remaster Normal Layers"];
		std::string current;
		if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
			UpdateRemasterEditingProfileDirty(current);
		remasterTilePixelSelected = false;
		[self showRemasterVariantAtIndex:remasterVariantIndex];
		return;
	}
	auto found = remasterEditingProfile.assets.find(remasterSelectedTile);
	if (found == remasterEditingProfile.assets.end())
		return;
	bool present = layer == RemasterEditorMaterial ? found->second.hasMaterialSelectors :
		layer == RemasterEditorDoorframe ? found->second.hasDoorframeMask :
		layer == RemasterEditorOcclusion ? found->second.hasOcclusion :
		layer == RemasterEditorHeight ? found->second.hasHeight :
		layer == RemasterEditorNormal ? found->second.hasNormals :
		layer == RemasterEditorEmission ? (found->second.hasEmission || found->second.emissionDepth > 0 ||
			found->second.emissionOmnidirectional) : false;
	if (!present)
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	if (layer == RemasterEditorDoorframe)
	{
		found->second.hasDoorframeMask = false;
		found->second.doorframeMask = 0;
	}
	else if (layer == RemasterEditorMaterial)
		found->second.hasMaterialSelectors = false;
	else if (layer == RemasterEditorOcclusion)
	{
		found->second.hasOcclusion = false;
		found->second.occlusion.fill(255);
	}
	else if (layer == RemasterEditorHeight)
	{
		found->second.hasHeight = false;
		found->second.heightSampling = RemasterHeightSampling::Nearest;
	}
	else if (layer == RemasterEditorNormal)
		found->second.hasNormals = false;
	else if (layer == RemasterEditorEmission)
	{
		found->second.hasEmission = false;
		found->second.emissionDepth = 0;
		found->second.emissionOmnidirectional = false;
	}
	if (!found->second.hasMaterialSelectors && !found->second.hasOcclusion &&
		!found->second.hasHeight && !found->second.hasNormals && !found->second.hasEmission && found->second.emissionDepth == 0 && !found->second.emissionOmnidirectional &&
		found->second.combineEmitters.empty() && !found->second.directLightingOppositeFacing && !found->second.normalIgnoreTileTransform && !found->second.hasTileSemantics && found->second.tileRole == RemasterTileRole::Unknown)
		remasterEditingProfile.assets.erase(found);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Reset Remaster Layer"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	remasterTilePixelSelected = false;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)resetRemasterTile:(id)sender
{
	if (!remasterTileContextName.empty()) {
		std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
		for (const auto &tile : remasterSelectedTiles) if (auto *c = RemasterSelectedContext(tile)) *c = RemasterTileContext{};
		[self finishRemasterContextEdit:@(before.c_str()) action:@"Reset Tile Context"];
		return;
	}
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty() ||
		!remasterEditingProfile.assets.count(remasterSelectedTile))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	const bool hadSemantics = remasterEditingProfile.assets.at(remasterSelectedTile).hasTileSemantics;
	remasterEditingProfile.assets.erase(remasterSelectedTile);
	if (hadSemantics)
	{
		auto &metadata = remasterEditingProfile.assets[remasterSelectedTile];
		metadata.tileId = remasterSelectedTile;
		metadata.hasTileSemantics = true; // Explicit reset supersedes the HTML sidecar.
	}
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Reset Remaster Tile"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	remasterTilePixelSelected = false;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)copyRemasterLayer:(id)sender
{
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (remasterVariants.empty() || layer == RemasterEditorArtwork)
		return;
	remasterClipboardMetadata = EffectiveRemasterMetadata(remasterSelectedTile);
	remasterClipboardLayer = layer;
	remasterClipboardGeometryFacing = remasterGeometryFacing;
	remasterClipboardHasData = true;
	remasterClipboardToken = [NSUUID UUID].UUIDString;
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:remasterClipboardToken forType:RemasterTileClipboardType];
	[self refreshRemasterEditingControls];
}

- (void)copyRemasterTile:(id)sender
{
	if (remasterVariants.empty())
		return;
	remasterClipboardMetadata = EffectiveRemasterMetadata(remasterSelectedTile);
	remasterClipboardLayer = -1;
	remasterClipboardHasData = true;
	remasterClipboardToken = [NSUUID UUID].UUIDString;
	NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
	[pasteboard clearContents];
	[pasteboard setString:remasterClipboardToken forType:RemasterTileClipboardType];
	[self refreshRemasterEditingControls];
}

- (void)pasteRemasterLayer:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty() || !RemasterClipboardAvailable(layer))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		RemasterAssetMetadata &destination = remasterEditingProfile.assets[tileId];
		destination.tileId = tileId;
		if (layer == RemasterEditorHeight || layer == RemasterEditorNormal)
		{
			const auto &s = S9xRemasterFlipGeometryForEditing(remasterClipboardMetadata, remasterClipboardGeometryFacing);
			auto &d = RemasterEditingGeometry(destination);
			if (layer == RemasterEditorHeight) { d.height = s.height; d.hasHeight = s.hasHeight; d.heightSampling = s.heightSampling; }
			else { d.normalXyz = s.normalXyz; d.hasNormals = s.hasNormals;
				destination.normalIgnoreTileTransform = remasterClipboardMetadata.normalIgnoreTileTransform;
				destination.directLightingOppositeFacing = remasterClipboardMetadata.directLightingOppositeFacing; }
		}
		else CopyRemasterMetadataLayer(remasterClipboardMetadata, destination, layer);
		if (!RemasterMetadataHasData(destination))
			remasterEditingProfile.assets.erase(tileId);
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion,
		RemasterMetadataSchemaVersion(remasterClipboardMetadata));
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics) || current == before)
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Paste Remaster Layer"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)finishRemasterContextEdit:(NSString *)before action:(NSString *)action
{
	std::string current; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics)) return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:) object:before];
	[remasterUndoManager setActionName:action];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterContextSelection:(id)sender
{
	remasterSelectLocations=remasterContextSelectionMode.indexOfSelectedItem==1;
	remasterReplayFrame.contextLocationSelection=remasterSelectLocations;
	remasterSelectedContextInstances.clear();remasterReplayFrame.selectedContextInstances.clear();
	remasterSelectedTiles.clear();remasterSelectionValid=false;
	remasterTileContextName.clear();remasterMetadataNeedsSync=true;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)assignRemasterLocationContext:(id)sender
{
	if(!remasterSelectLocations || remasterSelectedContextInstances.empty() || !remasterEditingProfileLoaded)return;
	// Validate every selected location first; single-context identities need no binding.
	for(auto id:remasterSelectedContextInstances) {
		if(id>remasterReplayFrame.tileInstances.size() || !remasterReplayFrame.contextLocations.count(id)){NSBeep();return;}
		const auto tile=remasterReplayFrame.tileInstances[id-1].tileId;
		auto contexts=remasterEditingProfile.tileContexts.find(tile);
		if(!remasterTileContextName.empty() && (contexts==remasterEditingProfile.tileContexts.end() || !contexts->second.count(remasterTileContextName))){NSBeep();return;}
	}
	std::string before;std::vector<RemasterProfileDiagnostic> diagnostics;
	if(!S9xRemasterSerializeProfile(remasterEditingProfile,before,diagnostics))return;
	for(auto id:remasterSelectedContextInstances) {
		const auto &at=remasterReplayFrame.contextLocations.at(id);const auto tile=remasterReplayFrame.tileInstances[id-1].tileId;
		auto contexts=remasterEditingProfile.tileContexts.find(tile);
		if(remasterTileContextName.empty() || contexts==remasterEditingProfile.tileContexts.end() || contexts->second.size()<2)remasterEditingProfile.contextBindings.erase(at);
		else remasterEditingProfile.contextBindings[at]={tile,remasterTileContextName};
	}
	[self finishRemasterContextEdit:@(before.c_str()) action:@"Assign Location Context"];
}

- (void)renameRemasterTileContext:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	NSString *input = [remasterTileContextInput.stringValue stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
	const std::string name = input.UTF8String;
	if (!S9xRemasterValidContextName(name)) { NSBeep(); [self refreshRemasterEditingControls]; return; }
	if (name == remasterTileContextName) return;
	// Refuse collisions for the whole selection before making any changes.
	for (const auto &tile : remasterSelectedTiles) {
		auto found = remasterEditingProfile.tileContexts.find(tile);
		if (found != remasterEditingProfile.tileContexts.end() && found->second.count(name)) {
			NSBeep(); [self refreshRemasterEditingControls]; return;
		}
	}
	std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	for (const auto &tile : remasterSelectedTiles) {
		const auto existing = EffectiveRemasterMetadata(tile);
		auto &contexts = remasterEditingProfile.tileContexts[tile];
		auto old = contexts.find(remasterTileContextName);
		RemasterTileContext geometry = S9xRemasterNewTileContext(existing, contexts.empty());
		if (old != contexts.end()) { geometry = old->second; contexts.erase(old); }
		for(auto &binding:remasterEditingProfile.contextBindings)
			if(binding.second.tile==tile && binding.second.name==remasterTileContextName)binding.second.name=name;
		contexts.emplace(name, geometry);
		// Geometry tools expect an asset identity even when only contexts exist.
		auto &asset = remasterEditingProfile.assets[tile]; asset.tileId = tile;
	}
	remasterTileContextName = name;
	remasterGeometryFacing = 0;
	[self finishRemasterContextEdit:@(before.c_str()) action:@"Add/Rename Tile Context"];
}

- (void)addRemasterTileContext:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	NSAlert *alert = [[NSAlert alloc] init];
	alert.messageText = @"Add Tile Context";
	alert.informativeText = @"The first context preserves existing geometry. Additional contexts start empty. None is reserved.";
	NSTextField *input = [[NSTextField alloc] initWithFrame:NSMakeRect(0, 0, 300, 24)];
	alert.accessoryView = input;
	[alert addButtonWithTitle:@"Add"]; [alert addButtonWithTitle:@"Cancel"];
	[alert.window makeFirstResponder:input];
	if ([alert runModal] != NSAlertFirstButtonReturn) return;
	const auto previous = remasterTileContextName;
	remasterTileContextName.clear(); remasterTileContextInput.stringValue = input.stringValue;
	[self renameRemasterTileContext:sender];
	if (remasterTileContextName.empty()) remasterTileContextName = previous;
	[self refreshRemasterEditingControls];
}

- (void)removeRemasterTileContext:(id)sender
{
	if (remasterTileContextName.empty() || !remasterEditingProfileLoaded) return;
	std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	for (const auto &tile : remasterSelectedTiles) {
		auto found = remasterEditingProfile.tileContexts.find(tile);
		if (found == remasterEditingProfile.tileContexts.end()) continue;
		found->second.erase(remasterTileContextName);
		for(auto b=remasterEditingProfile.contextBindings.begin();b!=remasterEditingProfile.contextBindings.end();) {
			if(b->second.tile==tile && (b->second.name==remasterTileContextName || found->second.size()<2))b=remasterEditingProfile.contextBindings.erase(b);else ++b;
		}
		if (found->second.empty()) remasterEditingProfile.tileContexts.erase(found);
		auto asset = remasterEditingProfile.assets.find(tile);
		if (asset != remasterEditingProfile.assets.end() && !RemasterMetadataHasData(asset->second)) remasterEditingProfile.assets.erase(asset);
	}
	remasterTileContextName.clear();
	[self finishRemasterContextEdit:@(before.c_str()) action:@"Remove Tile Context"];
}

- (void)cycleRemasterTileContext:(NSInteger)direction
{
	std::vector<std::string> names = {""};
	auto found = remasterEditingProfile.tileContexts.find(remasterSelectedTile);
	if (found != remasterEditingProfile.tileContexts.end()) for (const auto &c : found->second) names.push_back(c.first);
	auto current = std::find(names.begin(), names.end(), remasterTileContextName);
	NSInteger index = current == names.end() ? 0 : current - names.begin();
	remasterTileContextName = names[(index + direction + names.size()) % names.size()];
	if (remasterSelectLocations) {
		// Keep the tile-type authoring scope even when this context has no visible
		// assignments, so further cycling remains available. Only locations change.
		remasterSelectedContextInstances = S9xRemasterContextSelection(remasterEditingProfile,
			remasterReplayFrame, remasterSelectedTiles, remasterTileContextName);
		remasterReplayFrame.contextLocationSelection = true;
		remasterReplayFrame.selectedContextInstances = remasterSelectedContextInstances;
	}
	remasterGeometryFacing = 0;
	remasterMetadataNeedsSync = true;
	remasterTilePixelSelected = false;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}
- (void)previousRemasterTileContext:(id)sender { [self cycleRemasterTileContext:-1]; }
- (void)nextRemasterTileContext:(id)sender { [self cycleRemasterTileContext:1]; }

- (void)flipRemasterLayerHorizontal:(id)sender
{
	[self flipRemasterLayer:YES];
}

- (void)flipRemasterLayerVertical:(id)sender
{
	[self flipRemasterLayer:NO];
}

- (void)flipRemasterLayer:(BOOL)horizontal
{
	if (RemasterEffectiveHeightPreview() || !remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	for (const auto &tileId : remasterSelectedTiles)
	{
		auto mirrored = EffectiveRemasterMetadata(tileId);
		if (!remasterTileContextName.empty() && !RemasterSelectedContext(tileId)) continue;
		if (S9xRemasterHasGeometryVariants(mirrored) && (layer == RemasterEditorHeight || layer == RemasterEditorNormal))
		{
			auto &g = RemasterEditingGeometry(mirrored);
			if (layer == RemasterEditorHeight) { if (!g.hasHeight) continue; S9xRemasterFlipLayerPixels(g.height, horizontal); }
			else { if (!g.hasNormals) continue; S9xRemasterFlipNormalLayer(g.normalXyz, horizontal); }
			auto &destination = remasterEditingProfile.assets[tileId]; destination.tileId = tileId;
			CopyRemasterMetadataLayer(mirrored, destination, layer);
			continue;
		}
		if (!S9xRemasterFlipMetadataLayer(mirrored, static_cast<int>(layer), horizontal)) continue;
		auto &destination = remasterEditingProfile.assets[tileId];
		destination.tileId = tileId;
		CopyRemasterMetadataLayer(mirrored, destination, layer);
		remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, RemasterMetadataSchemaVersion(destination));
	}
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics) || current == before) return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:horizontal ? @"Flip Layer Horizontally" : @"Flip Layer Vertically"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)shiftRemasterLayer:(id)sender
{
	if (RemasterEffectiveHeightPreview() || !remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	const NSInteger direction = [sender tag], layer = remasterLayerSelector.indexOfSelectedItem;
	if (direction < 0 || direction > 3) return;
	const int dx[] = {0,0,-1,1}, dy[] = {-1,1,0,0};
	std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	for (const auto &tileId : remasterSelectedTiles) {
		auto shifted = EffectiveRemasterMetadata(tileId);
		if (!remasterTileContextName.empty() && !RemasterSelectedContext(tileId)) continue;
		// Geometry variant previews show flipped artwork; convert the displayed
		// translation back to canonical pixel coordinates exactly once.
		const bool variant = S9xRemasterHasGeometryVariants(shifted) && (layer == RemasterEditorHeight || layer == RemasterEditorNormal);
		const unsigned previewFlip = RemasterInspectorPreviewFlip();
		const int x = previewFlip & 1 ? -dx[direction] : dx[direction];
		const int y = previewFlip & 2 ? -dy[direction] : dy[direction];
		if (variant) {
			auto &g = RemasterEditingGeometry(shifted);
			if (layer == RemasterEditorHeight) { if (!g.hasHeight) continue; S9xRemasterShiftLayerPixels(g.height,x,y); }
			else { if (!g.hasNormals) continue; S9xRemasterShiftLayerPixels(g.normalXyz,x,y,std::array<uint8_t,3>{{128,128,255}}); }
		} else if (!S9xRemasterShiftMetadataLayer(shifted,static_cast<int>(layer),x,y)) continue;
		auto &destination = remasterEditingProfile.assets[tileId]; destination.tileId = tileId;
		CopyRemasterMetadataLayer(shifted,destination,layer);
	}
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile,current,diagnostics) || current == before) return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Shift Layer One Pixel"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)pasteRemasterTile:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty() || !RemasterClipboardAvailable(-1))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		if (!remasterTileContextName.empty()) {
			if (auto *context = RemasterSelectedContext(tileId)) {
				static_cast<RemasterGeometryLayers &>(*context) = static_cast<const RemasterGeometryLayers &>(remasterClipboardMetadata);
				context->normalIgnoreTileTransform = remasterClipboardMetadata.normalIgnoreTileTransform;
				context->directLightingOppositeFacing = remasterClipboardMetadata.directLightingOppositeFacing;
			}
			continue;
		}
		if (RemasterMetadataHasData(remasterClipboardMetadata))
		{
			RemasterAssetMetadata destination = remasterClipboardMetadata;
			destination.tileId = tileId;
			// Combination links describe identities, not paintable tile layers.
			destination.combineEmitters = EffectiveRemasterMetadata(tileId).combineEmitters;
			remasterEditingProfile.assets[tileId] = std::move(destination);
		}
		else
			remasterEditingProfile.assets.erase(tileId);
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion,
		RemasterMetadataSchemaVersion(remasterClipboardMetadata));
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics) || current == before)
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Paste Remaster Tile"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)applyRemasterTileToVariants:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.size() < 2)
		return;
	const RemasterAssetMetadata sourceMetadata = EffectiveRemasterMetadata(remasterSelectedTile);
	if (!RemasterMetadataHasData(sourceMetadata))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	std::array<std::array<uint8_t, 3>, 64> sourceArtwork = {};
	const std::array<bool, 64> sourceArtworkVisible = RemasterVisibleTileColors(remasterReplayFrame,
		remasterSelectedTile, sourceArtwork);
	for (const RemasterTileContentId &tileId : remasterVariants)
	{
		RemasterAssetMetadata destination = sourceMetadata;
		destination.tileId = tileId;
		const auto existing = EffectiveRemasterMetadata(tileId);
		destination.combineEmitters = existing.combineEmitters;
		if (S9xRemasterHasGeometryVariants(sourceMetadata) || S9xRemasterHasGeometryVariants(existing))
		{
			static_cast<RemasterGeometryLayers &>(destination) = static_cast<const RemasterGeometryLayers &>(existing);
			destination.geometryByFacing = existing.geometryByFacing;
			destination.facingGeometry = existing.facingGeometry;
			destination.geometryByFlip = existing.geometryByFlip;
			destination.flipGeometry = existing.flipGeometry;
			destination.normalIgnoreTileTransform = existing.normalIgnoreTileTransform;
			destination.directLightingOppositeFacing = existing.directLightingOppositeFacing;
		}
		if (sourceMetadata.hasEmission && !(tileId == remasterSelectedTile))
		{
			std::array<std::array<uint8_t, 3>, 64> destinationArtwork = {};
			const std::array<bool, 64> destinationArtworkVisible = RemasterVisibleTileColors(remasterReplayFrame,
				tileId, destinationArtwork);
			for (size_t pixel = 0; pixel < 64; pixel++)
			{
				const size_t offset = pixel * 4;
				if (sourceArtworkVisible[pixel] && destinationArtworkVisible[pixel] &&
					sourceMetadata.emissionRgba[offset] == sourceArtwork[pixel][0] &&
					sourceMetadata.emissionRgba[offset + 1] == sourceArtwork[pixel][1] &&
					sourceMetadata.emissionRgba[offset + 2] == sourceArtwork[pixel][2])
				{
					// Artwork-colored emission tracks each frame's artwork; only its intensity is shared.
					destination.emissionRgba[offset] = destinationArtwork[pixel][0];
					destination.emissionRgba[offset + 1] = destinationArtwork[pixel][1];
					destination.emissionRgba[offset + 2] = destinationArtwork[pixel][2];
					destination.emissionRgba[offset + 3] = sourceMetadata.emissionRgba[offset + 3];
				}
			}
		}
		remasterEditingProfile.assets[tileId] = std::move(destination);
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion,
		RemasterMetadataSchemaVersion(sourceMetadata));
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics) || current == before)
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Apply Frame to Other Frames"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)setRemasterEmissionFromVisibleTile:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterVariants.empty() ||
		(remasterEmissionColorScope.indexOfSelectedItem == 0 && !remasterTilePixelSelected))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	const int selectedPixel = remasterEmissionColorScope.indexOfSelectedItem == 0 ?
		static_cast<int>(remasterSelectedTilePixel) : -1;
	if (!SetRemasterEmissionFromVisibleColors(remasterEditingProfile, remasterReplayFrame,
		remasterSelectedTile, selectedPixel, static_cast<uint8_t>(std::max<NSInteger>(0, std::min<NSInteger>(255, remasterValueBrush.integerValue)))))
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Set Emission From Visible Tile"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)setRemasterEmissionFromVisibleAnimation:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterVariants.empty() ||
		(remasterEmissionColorScope.indexOfSelectedItem == 0 && !remasterTilePixelSelected))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	const int selectedPixel = remasterEmissionColorScope.indexOfSelectedItem == 0 ?
		static_cast<int>(remasterSelectedTilePixel) : -1;
	bool changed = false;
	for (const RemasterTileContentId &variant : remasterVariants)
		changed |= SetRemasterEmissionFromVisibleColors(remasterEditingProfile, remasterReplayFrame, variant, selectedPixel,
			static_cast<uint8_t>(std::max<NSInteger>(0, std::min<NSInteger>(255, remasterValueBrush.integerValue))));
	if (!changed)
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Set Animation Emission RGB From Visible Colors"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterOmnidirectionalEmission:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	const bool enabled = remasterOmnidirectionalEmissionButton.state != NSControlStateValueOff;
	for (const auto &tile : remasterSelectedTiles)
	{
		auto metadata = EffectiveRemasterMetadata(tile);
		metadata.emissionOmnidirectional = enabled;
		remasterEditingProfile.assets[tile] = metadata;
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 19u);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Change Emission Direction"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics)) UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterEmissionDepth:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	float depth;
	NSScanner *scanner = [NSScanner scannerWithString:remasterEmissionDepthInput.stringValue];
	if (![scanner scanFloat:&depth] || !scanner.isAtEnd || !std::isfinite(depth) || depth < 0 || depth > 64)
	{
		NSBeep(); [self refreshRemasterEditingControls]; return;
	}
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	for (const auto &tile : remasterSelectedTiles)
	{
		auto metadata = EffectiveRemasterMetadata(tile);
		metadata.emissionDepth = depth;
		remasterEditingProfile.assets[tile] = metadata;
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 14u);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Change Emission Depth"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics)) UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)combineRemasterEmitters:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.size() < 2) return;
	if (!S9xRemasterEmitterSelectionConnected(remasterReplayFrame, remasterSelectedTiles)) {
		NSAlert *alert = [[NSAlert alloc] init];
		alert.messageText = @"Selected tiles are not adjacent";
		alert.informativeText = @"All selected tile types must appear together in one shape connected by shared tile edges (up, down, left or right). Diagonal contact does not count. No emitters were combined.";
		[alert runModal]; return;
	}
	std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
	const auto combinedTiles = S9xRemasterEmitterSelectionAnimationVariants(remasterReplayFrame, remasterSelectedTiles);
	for (const auto &tile : combinedTiles) {
		auto metadata = EffectiveRemasterMetadata(tile);
		for (const auto &peer : combinedTiles) if (!(peer == tile) &&
			std::find(metadata.combineEmitters.begin(),metadata.combineEmitters.end(),peer) == metadata.combineEmitters.end())
			metadata.combineEmitters.push_back(peer);
		remasterEditingProfile.assets[tile] = metadata;
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion,17u);
	std::string current;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile,current,diagnostics) || current == before) return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Combine Emitters"];
	UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (BOOL)commitRemasterObstacleProfile:(const RemasterProfile &)candidate action:(NSString *)name
{
	std::string before, after;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile,before,diagnostics)) return NO;
	if (!S9xRemasterSerializeProfile(candidate,after,diagnostics))
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Cannot save obstacle group";
		alert.informativeText = diagnostics.empty() ? @"Invalid group definition." :
			[NSString stringWithUTF8String:diagnostics.front().message.c_str()];
		[alert runModal]; return NO;
	}
	if (before == after) return YES;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:name];
	remasterEditingProfile = candidate;
	UpdateRemasterEditingProfileDirty(after);
	SyncRemasterEditingMetadataToFrame();
	if (running) S9xRemasterSetProfile(remasterEditingProfile);
	[self refreshRemasterEditingControls];
	return YES;
}

- (void)createRemasterObstacleGroup:(id)sender
{
	if (!remasterEditingProfileLoaded) return;
	RemasterObstacleGroup group; std::string error;
	if (!S9xRemasterObstacleEditorBuild(remasterObstacleSelection,remasterEditingProfile.obstacleGroups,
		remasterObstacleName.stringValue.UTF8String ?: "",group,error))
	{
		NSAlert *alert = [NSAlert new]; alert.messageText = @"Cannot create obstacle group";
		alert.informativeText = [NSString stringWithUTF8String:error.c_str()]; [alert runModal]; return;
	}
	auto candidate = remasterEditingProfile;
	candidate.schemaVersion = std::max(candidate.schemaVersion,22u);
	candidate.obstacleGroups.push_back(group);
	remasterObstacleFocusedGroup = group.name;
	remasterObstacleMemberIndex = 0;
	[self commitRemasterObstacleProfile:candidate action:@"Create Obstacle Group"];
}

- (void)selectRemasterObstacleMember:(id)sender
{
	remasterObstacleMemberIndex = [sender isKindOfClass:[NSButton class]] ? [sender tag] : remasterObstacleMembers.indexOfSelectedItem;
	if (remasterObstacleGroupIndex < 0 || size_t(remasterObstacleGroupIndex)>=remasterEditingProfile.obstacleGroups.size()) return;
	const auto &group = remasterEditingProfile.obstacleGroups[remasterObstacleGroupIndex];
	if (remasterObstacleMemberIndex < 0 || size_t(remasterObstacleMemberIndex)>=group.members.size()) return;
	const auto &member = group.members[remasterObstacleMemberIndex];
	remasterSelectedTile = member.tileId;
	remasterVariants = S9xRemasterFrameAnimationVariants(remasterReplayFrame,member.tileId,"");
	auto found = std::find(remasterVariants.begin(),remasterVariants.end(),member.tileId);
	if (found != remasterVariants.end()) [self showRemasterVariantAtIndex:size_t(found-remasterVariants.begin())];
	[self refreshRemasterObstacleControls];
}

- (void)applyRemasterObstacleMember:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterObstacleGroupIndex<0 || remasterObstacleMemberIndex<0) return;
	NSInteger z = 0; NSScanner *scanner = [NSScanner scannerWithString:remasterObstacleZ.stringValue];
	if (![scanner scanInteger:&z] || !scanner.isAtEnd || z < -4095 || z > 4095)
	{ NSBeep(); return; }
	auto candidate = remasterEditingProfile;
	if (size_t(remasterObstacleGroupIndex)>=candidate.obstacleGroups.size()) return;
	auto &group = candidate.obstacleGroups[remasterObstacleGroupIndex];
	if (size_t(remasterObstacleMemberIndex)>=group.members.size()) return;
	group.name = remasterObstacleName.stringValue.UTF8String ?: "";
	group.members[remasterObstacleMemberIndex].z = int(z);
	group.members[remasterObstacleMemberIndex].supportContact = remasterObstacleContact.state == NSControlStateValueOn;
	const std::string focus = group.name;
	if ([self commitRemasterObstacleProfile:candidate action:@"Edit Obstacle Member"])
	{ remasterObstacleFocusedGroup = focus; [self refreshRemasterObstacleControls]; }
}

- (void)deleteRemasterObstacleGroup:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterObstacleGroupIndex<0 || size_t(remasterObstacleGroupIndex)>=remasterEditingProfile.obstacleGroups.size()) return;
	auto candidate = remasterEditingProfile;
	candidate.obstacleGroups.erase(candidate.obstacleGroups.begin()+remasterObstacleGroupIndex);
	remasterObstacleFocusedGroup.clear(); remasterObstacleMemberIndex = -1;
	[self commitRemasterObstacleProfile:candidate action:@"Delete Obstacle Group"];
}

- (void)showRemasterObstacleGroup:(id)sender
{
	if (remasterObstacleGroupIndex<0 || size_t(remasterObstacleGroupIndex)>=remasterEditingProfile.obstacleGroups.size()) return;
	remasterSelectedTiles.clear();
	for (const auto &member : remasterEditingProfile.obstacleGroups[remasterObstacleGroupIndex].members)
		if (std::find(remasterSelectedTiles.begin(),remasterSelectedTiles.end(),member.tileId)==remasterSelectedTiles.end())
			remasterSelectedTiles.push_back(member.tileId);
	remasterSelectionValid = !remasterSelectedTiles.empty();
	DrawRemasterFrame(remasterReplayFrame,remasterReplayDebugMode,&remasterSelectedTiles,remasterLightingEnabled,remasterLightingView);
}

- (void)refreshRemasterObstacleControls
{
	NSInteger groupIndex = -1;
	for (size_t g=0;g<remasterEditingProfile.obstacleGroups.size();g++)
		for (const auto &member : remasterEditingProfile.obstacleGroups[g].members)
			if (member.tileId==remasterSelectedTile) groupIndex = NSInteger(g);
	if (groupIndex!=remasterObstacleGroupIndex || (groupIndex>=0 &&
		remasterObstacleFocusedGroup!=remasterEditingProfile.obstacleGroups[groupIndex].name))
	{
		remasterObstacleMemberIndex = -1;
		if (groupIndex>=0) for (size_t m=0;m<remasterEditingProfile.obstacleGroups[groupIndex].members.size();m++)
			if (remasterEditingProfile.obstacleGroups[groupIndex].members[m].tileId==remasterSelectedTile)
			{ remasterObstacleMemberIndex = NSInteger(m); break; }
	}
	remasterObstacleGroupIndex = groupIndex;
	[remasterObstacleMembers removeAllItems];
	for (NSView *view in [remasterObstacleFootprint.subviews copy]) [view removeFromSuperview];
	const bool hasGroup = groupIndex>=0;
	remasterObstacleCreate.enabled = remasterEditingProfileLoaded && !remasterObstacleSelection.empty() && !hasGroup;
	for (NSControl *control in @[remasterObstacleMembers,remasterObstacleZ,remasterObstacleContact,
		remasterObstacleApply,remasterObstacleDelete,remasterObstacleShow]) control.enabled = remasterEditingProfileLoaded && hasGroup;
	if (!hasGroup)
	{
		if (!remasterObstacleFocusedGroup.empty()) remasterObstacleName.stringValue = @"";
		remasterObstacleFocusedGroup.clear();
		remasterObstacleStatus.stringValue = [NSString stringWithFormat:@"%lu occurrence tiles selected. In Metadata mode, Cmd-click each tile in ONE example. New groups start at Z=0 with editable south-boundary contacts.",static_cast<unsigned long>(remasterObstacleSelection.size())];
		return;
	}
	const auto &group = remasterEditingProfile.obstacleGroups[groupIndex];
	remasterObstacleFocusedGroup = group.name;
	remasterObstacleName.stringValue = [NSString stringWithUTF8String:group.name.c_str()];
	int minX=63,minY=63,maxX=-63,maxY=-63;
	for (const auto &member : group.members) { minX=std::min(minX,member.x); minY=std::min(minY,member.y); maxX=std::max(maxX,member.x); maxY=std::max(maxY,member.y); }
	const CGFloat size = std::min<CGFloat>(36,std::min(150.0/(maxX-minX+1),150.0/(maxY-minY+1)));
	bool missingHeight = false;
	for (size_t m=0;m<group.members.size();m++)
	{
		const auto &member = group.members[m];
		const auto asset = remasterEditingProfile.assets.find(member.tileId);
		missingHeight |= asset==remasterEditingProfile.assets.end() || !asset->second.hasHeight;
		[remasterObstacleMembers addItemWithTitle:[NSString stringWithFormat:@"(%d,%d) Z=%d %@  %016llx",member.x,member.y,member.z,member.supportContact?@"[support]":@"",static_cast<unsigned long long>(member.tileId.hash)]];
		NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect((member.x-minX)*size,150-(member.y-minY+1)*size,size,size)];
		button.title = [NSString stringWithFormat:@"%d%@",member.z,member.supportContact?@"*":@""];
		button.font = [NSFont systemFontOfSize:std::max<CGFloat>(7,std::min<CGFloat>(11,size/3))];
		button.bezelStyle = NSBezelStyleRegularSquare;
		button.buttonType = NSButtonTypeToggle;
		button.state = NSInteger(m)==remasterObstacleMemberIndex ? NSControlStateValueOn : NSControlStateValueOff;
		button.tag = NSInteger(m); button.target = self; button.action = @selector(selectRemasterObstacleMember:);
		button.toolTip = [NSString stringWithFormat:@"Member (%d,%d), Z=%d (+%d units), %@",member.x,member.y,member.z,member.z*8,member.supportContact?@"support contact":@"not a contact"];
		[remasterObstacleFootprint addSubview:button];
	}
	if (remasterObstacleMemberIndex<0 || size_t(remasterObstacleMemberIndex)>=group.members.size()) remasterObstacleMemberIndex=0;
	[remasterObstacleMembers selectItemAtIndex:remasterObstacleMemberIndex];
	const auto &member = group.members[remasterObstacleMemberIndex];
	remasterObstacleZ.integerValue = member.z;
	remasterObstacleContact.state = member.supportContact ? NSControlStateValueOn : NSControlStateValueOff;
	remasterObstacleZ.toolTip = @"Z×8 applies only to members without authored heights. Authored members retain their entire local shape and receive only the shared floor base.";
	remasterObstacleStatus.stringValue = missingHeight ? @"Missing-height members use bottom0/top7 + Z×8 + floor base. Authored members add only floor base. * marks contacts; support is evaluated on the next gameplay frame." :
		@"Authored heights preserved; all members add the shared floor base only. Z is used if a member has no height data. * marks contacts; support is evaluated on the next gameplay frame.";
}

- (void)refreshRemasterEditingControls
{
	const auto contexts = remasterEditingProfile.tileContexts.find(remasterSelectedTile);
	if (!remasterTileContextName.empty() && !RemasterSelectedContext(remasterSelectedTile)) remasterTileContextName.clear();
	const bool contextEditable = remasterEditingProfileLoaded && !remasterSelectedTiles.empty();
	remasterAssignContextButton.enabled=contextEditable && remasterSelectLocations && !remasterSelectedContextInstances.empty();
	remasterTileContextInput.stringValue = remasterTileContextName.empty() ? @"None" : @(remasterTileContextName.c_str());
	remasterTileContextInput.enabled = contextEditable;
	remasterRemoveContextButton.enabled = contextEditable && !remasterTileContextName.empty();
	remasterAddContextButton.enabled = contextEditable;
	remasterPreviousContextButton.enabled = remasterNextContextButton.enabled = contextEditable && contexts != remasterEditingProfile.tileContexts.end() && !contexts->second.empty();
	const NSInteger layer = remasterLayerSelector ? remasterLayerSelector.indexOfSelectedItem : 0;
	const bool editable = remasterEditingProfileLoaded && !remasterVariants.empty() && !RemasterEffectiveHeightPreview() &&
		(remasterTileContextName.empty() || std::all_of(remasterSelectedTiles.begin(), remasterSelectedTiles.end(), [](const auto &tile) { return RemasterSelectedContext(tile) != nullptr; }));
	remasterHeightDomainSelector.hidden = remasterHeightOccurrenceSelector.hidden = remasterHeightReport.hidden = layer != RemasterEditorHeight;
	remasterHeightOccurrenceSelector.enabled = !remasterHeightOccurrences.empty();
	remasterHeightDomainSelector.enabled = !remasterNpcInspectorActive;
	remasterSemanticsView.hidden = layer != RemasterEditorMetadata;
	remasterObstacleView.hidden = layer != RemasterEditorMetadata || remasterNpcInspectorActive;
	remasterInspectorScrollView.hidden = layer == RemasterEditorMetadata;
	if (layer == RemasterEditorMetadata) [self refreshRemasterObstacleControls];
	if (layer == RemasterEditorMetadata)
	{
		auto metadata = EffectiveRemasterMetadata(remasterSelectedTile);
		if (!metadata.hasTileSemantics)
		{
			NSData *data = [NSData dataWithContentsOfFile:[RemasterMetadataProjectRoot() stringByAppendingPathComponent:@"tools/ALttP/alttp-tile-semantics-v1.json"]];
			NSDictionary *document = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
			NSString *hash = [NSString stringWithUTF8String:RemasterProfileSerialization::TileId(remasterSelectedTile).c_str()];
			NSDictionary *annotation = document[@"tiles"][hash];
			if ([document[@"rom_sha256"] isEqualToString:[NSString stringWithUTF8String:remasterEditingProfile.romSha256.c_str()]] && [annotation isKindOfClass:[NSDictionary class]])
			{
				for (unsigned i = 0; i < static_cast<unsigned>(RemasterTileRole::Count); i++)
					if ([annotation[@"role"] isEqualToString:[NSString stringWithUTF8String:S9xRemasterTileRoleName(static_cast<RemasterTileRole>(i))]]) metadata.tileRole = static_cast<RemasterTileRole>(i);
				for (NSString *normal in annotation[@"normals"]) metadata.tileSemanticNormals.push_back(normal.UTF8String);
				metadata.tileSemanticCorner = [annotation[@"corner_type"] isEqualToString:@"unknown"] ? "" : ([annotation[@"corner_type"] UTF8String] ?: "");
				metadata.tileSemanticContextDependent = [annotation[@"context_dependent"] boolValue];
				metadata.tileSemanticNote = [annotation[@"note"] UTF8String] ?: "";
			}
		}
		[remasterRoleSelector selectItemAtIndex:static_cast<NSInteger>(metadata.tileRole)];
		[remasterFacingSelector selectItemAtIndex:0];
		[remasterSecondFacingSelector selectItemAtIndex:0];
		if (!metadata.tileSemanticNormals.empty()) [remasterFacingSelector selectItemWithTitle:[NSString stringWithUTF8String:metadata.tileSemanticNormals[0].c_str()]];
		if (metadata.tileSemanticNormals.size() > 1) [remasterSecondFacingSelector selectItemWithTitle:[NSString stringWithUTF8String:metadata.tileSemanticNormals[1].c_str()]];
		[remasterCornerSelector selectItemAtIndex:0];
		if (!metadata.tileSemanticCorner.empty()) [remasterCornerSelector selectItemWithTitle:[NSString stringWithUTF8String:metadata.tileSemanticCorner.c_str()]];
		remasterContextDependentButton.state = metadata.tileSemanticContextDependent ? NSControlStateValueOn : NSControlStateValueOff;
		remasterSemanticNoteInput.stringValue = [NSString stringWithUTF8String:metadata.tileSemanticNote.c_str()];
		for (NSControl *control in remasterSemanticsView.subviews) control.enabled = editable;
	}
	remasterMetadataToolButton.enabled = remasterSelectionValid && !remasterMetadataToolOpening;
	remasterTilePreview.showsNormalAxes = NO;
	remasterNormalAxesView.hidden = layer != RemasterEditorNormal;
	remasterNormalBackButton.hidden = layer != RemasterEditorNormal;
	remasterNormalUpdateButton.hidden = layer != RemasterEditorNormal;
	remasterNormalPixelScopeLabel.hidden = layer != RemasterEditorNormal;
	remasterMaterialBrush.hidden = layer != RemasterEditorMaterial;
	remasterDoorframeBrush.hidden = remasterDoorframeAnimationButton.hidden = layer != RemasterEditorDoorframe;
	remasterDoorframeBrush.enabled = editable;
	remasterDoorframeAnimationButton.enabled = editable && remasterVariants.size() > 1;
	if (layer == RemasterEditorDoorframe)
	{
		const auto metadata = EffectiveRemasterMetadata(remasterSelectedTile);
		remasterValueLabel.stringValue = !metadata.hasDoorframeMask ? @"Unknown mask (not authored)" :
			remasterTilePixelSelected ? [NSString stringWithFormat:@"Pixel (%lu,%lu): %@", remasterSelectedTilePixel % 8,
				remasterSelectedTilePixel / 8, S9xRemasterDoorframePixel(metadata.doorframeMask,
					static_cast<unsigned>(remasterSelectedTilePixel % 8), static_cast<unsigned>(remasterSelectedTilePixel / 8)) ? @"frame (true)" : @"not frame (false)"] :
			@"Gold = frame; others retain geometry";
	}
	remasterValueBrush.hidden = layer != RemasterEditorOcclusion && layer != RemasterEditorHeight && layer != RemasterEditorEmission;
	remasterValueBrush.minValue = layer == RemasterEditorHeight ? -32768 : 0;
	remasterValueBrush.maxValue = layer == RemasterEditorHeight ? 32767 : 255;
	remasterValueInput.hidden = remasterValueBrush.hidden;
	remasterValueDownButton.hidden = remasterValueBrush.hidden;
	remasterValueUpButton.hidden = remasterValueBrush.hidden;
	remasterValueLabel.hidden = layer == RemasterEditorArtwork || layer == RemasterEditorMetadata;
	// Emission's color and paint-mode controls need more room before the shared value controls.
	const bool emissionLayer = layer == RemasterEditorEmission;
	remasterValueDownButton.frame = NSMakeRect(emissionLayer ? 380 : 342, 377, 30, 28);
	remasterValueBrush.frame = NSMakeRect(emissionLayer ? 414 : 376, 380, 120, 22);
	remasterValueUpButton.frame = NSMakeRect(emissionLayer ? 538 : 500, 377, 30, 28);
	remasterValueInput.frame = NSMakeRect(emissionLayer ? 454 : 396, 399, emissionLayer ? 40 : 80, 20);
	remasterValueLabel.frame = layer == RemasterEditorNormal ? NSMakeRect(350, 468, 390, 18) :
		(emissionLayer ? NSMakeRect(573, 379, 167, 24) : NSMakeRect(535, 379, 205, 24));
	// Geometry authoring occupies the previously empty upper-right area. Keep
	// every layer-specific row above the shared copy/paste row at y=270.
	remasterNormalPixelScopeLabel.frame = NSMakeRect(244, 468, 100, 16);
	remasterNormalPreset.frame = NSMakeRect(244, 440, 90, 28);
	remasterNormalXInput.frame = NSMakeRect(340, 440, 70, 24);
	remasterNormalYInput.frame = NSMakeRect(420, 440, 70, 24);
	remasterNormalZInput.frame = NSMakeRect(500, 440, 70, 24);
	remasterNormalUpdateButton.frame = NSMakeRect(578, 438, 80, 28);
	remasterNormalApplyAnimationButton.frame = NSMakeRect(244, 400, 150, 28);
	remasterNormalFillTileButton.frame = NSMakeRect(401, 400, 130, 28);
	remasterNormalIgnoreTransformButton.frame = NSMakeRect(244, 360, 288, 28);
	remasterOppositeFacingDirectButton.frame = NSMakeRect(540, 360, 200, 28);
	remasterOcclusionFillOpaqueButton.hidden = layer != RemasterEditorOcclusion;
	remasterHeightSamplingSelector.hidden = layer != RemasterEditorHeight;
	remasterHeightPreviewMode.hidden = layer != RemasterEditorHeight;
	remasterHeightArtworkVisibleButton.hidden = YES;
	remasterHeightDataVisibleButton.hidden = layer != RemasterEditorHeight;
	remasterHeightApplyAnimationButton.hidden = layer != RemasterEditorHeight;
	remasterHeightFillTileButton.hidden = layer != RemasterEditorHeight;
	remasterHeightDecreaseTileLargeButton.hidden = layer != RemasterEditorHeight;
	remasterHeightDecreaseTileButton.hidden = layer != RemasterEditorHeight;
	remasterHeightIncreaseTileButton.hidden = layer != RemasterEditorHeight;
	remasterHeightIncreaseTileLargeButton.hidden = layer != RemasterEditorHeight;
	remasterNormalPreset.hidden = layer != RemasterEditorNormal;
	remasterNormalXInput.hidden = layer != RemasterEditorNormal;
	remasterNormalYInput.hidden = layer != RemasterEditorNormal;
	remasterNormalZInput.hidden = layer != RemasterEditorNormal;
	remasterNormalFillTileButton.hidden = layer != RemasterEditorNormal;
	remasterNormalApplyAnimationButton.hidden = layer != RemasterEditorNormal;
	remasterOppositeFacingDirectButton.hidden = layer != RemasterEditorNormal;
	remasterNormalIgnoreTransformButton.hidden = layer != RemasterEditorNormal;
	remasterEmissionColor.hidden = layer != RemasterEditorEmission;
	remasterEmissionPaintMode.hidden = layer != RemasterEditorEmission;
	remasterEmissionColorScope.hidden = layer != RemasterEditorEmission;
	remasterEmissionFromTileButton.hidden = layer != RemasterEditorEmission;
	remasterEmissionFromAnimationButton.hidden = layer != RemasterEditorEmission;
	remasterArtworkVisibleButton.hidden = YES;
	remasterEmissionVisibleButton.hidden = layer != RemasterEditorEmission;
	remasterEmissionDepthInput.hidden = remasterEmissionDepthLabel.hidden = layer != RemasterEditorEmission;
	remasterEmissionDepthInput.enabled = editable;
	remasterCombineEmittersButton.hidden = !emissionLayer;
	remasterCombineEmittersButton.enabled = editable && remasterSelectedTiles.size() > 1;
	remasterOmnidirectionalEmissionButton.hidden = !emissionLayer;
	remasterOmnidirectionalEmissionButton.enabled = editable && !remasterSelectedTiles.empty();
	if (remasterSelectionValid)
	{
		const bool omni = EffectiveRemasterMetadata(remasterSelectedTile).emissionOmnidirectional;
		bool mixed = false;
		for (const auto &tile : remasterSelectedTiles)
			mixed |= EffectiveRemasterMetadata(tile).emissionOmnidirectional != omni;
		remasterOmnidirectionalEmissionButton.state = mixed ? NSControlStateValueMixed :
			(omni ? NSControlStateValueOn : NSControlStateValueOff);
	}
	if (editable) remasterEmissionDepthInput.stringValue = [NSString stringWithFormat:@"%.6g", EffectiveRemasterMetadata(remasterSelectedTile).emissionDepth];
	remasterResetLayerButton.hidden = layer == RemasterEditorArtwork;
	remasterResetTileButton.hidden = false;
	remasterTilePreview.enabled = !remasterVariants.empty();
	remasterArtworkPreview.enabled = !remasterVariants.empty();
	remasterMaterialBrush.enabled = editable;
	remasterValueBrush.enabled = editable &&
		(layer == RemasterEditorOcclusion || layer == RemasterEditorHeight || layer == RemasterEditorEmission || remasterTilePixelSelected);
	remasterValueInput.enabled = remasterValueBrush.enabled;
	remasterValueDownButton.enabled = remasterValueBrush.enabled;
	remasterValueUpButton.enabled = remasterValueBrush.enabled;
	remasterOcclusionFillOpaqueButton.enabled = editable && !remasterSelectedTiles.empty();
	remasterHeightSamplingSelector.enabled = editable;
	remasterHeightPreviewMode.enabled = layer == RemasterEditorHeight && !RemasterEffectiveHeightPreview();
	remasterHeightArtworkVisibleButton.enabled = layer == RemasterEditorHeight;
	remasterHeightDataVisibleButton.enabled = layer == RemasterEditorHeight;
	remasterHeightApplyAnimationButton.enabled = editable && remasterVariants.size() > 1 &&
		remasterEditingProfile.assets.count(remasterSelectedTile) &&
		remasterEditingProfile.assets.at(remasterSelectedTile).hasHeight;
	remasterHeightFillTileButton.enabled = editable;
	remasterHeightDecreaseTileLargeButton.enabled = editable;
	remasterHeightDecreaseTileButton.enabled = editable;
	remasterHeightIncreaseTileButton.enabled = editable;
	remasterHeightIncreaseTileLargeButton.enabled = editable;
	remasterNormalPreset.enabled = editable;
	remasterNormalXInput.enabled = editable;
	remasterNormalYInput.enabled = editable;
	remasterNormalZInput.enabled = editable;
	remasterNormalUpdateButton.enabled = editable && remasterTilePixelSelected;
	remasterNormalAxesView.enabled = editable;
	remasterNormalBackButton.enabled = editable;
	remasterNormalFillTileButton.enabled = editable;
	remasterNormalApplyAnimationButton.enabled = editable && remasterVariants.size() > 1 &&
		remasterEditingProfile.assets.count(remasterSelectedTile) &&
		remasterEditingProfile.assets.at(remasterSelectedTile).hasNormals;
	remasterOppositeFacingDirectButton.enabled = editable;
	remasterNormalIgnoreTransformButton.enabled = editable;
	const bool geometryLayer = layer == RemasterEditorHeight || layer == RemasterEditorNormal;
	remasterGeometryByFacingButton.frame = layer == RemasterEditorHeight ?
		NSMakeRect(168, 530, 285, 28) : NSMakeRect(244, 500, 285, 28);
	remasterGeometryFacingSelector.frame = layer == RemasterEditorHeight ?
		NSMakeRect(460, 530, 270, 28) : NSMakeRect(540, 500, 200, 28);
	remasterGeometryByFacingButton.hidden = !geometryLayer;
	remasterGeometryFacingSelector.hidden = !geometryLayer;
	remasterGeometryByFacingButton.enabled = editable && remasterTileContextName.empty();
	const auto geometryAsset = remasterEditingProfile.assets.find(remasterSelectedTile);
	const bool byFacing = geometryAsset != remasterEditingProfile.assets.end() && geometryAsset->second.geometryByFlip;
	remasterGeometryByFacingButton.state = byFacing ? NSControlStateValueOn : NSControlStateValueOff;
	remasterGeometryFacingSelector.enabled = editable && byFacing && remasterTileContextName.empty();
	[remasterGeometryFacingSelector selectItemAtIndex:remasterGeometryFacing];
	if (byFacing) { remasterHeightApplyAnimationButton.enabled = NO; remasterNormalApplyAnimationButton.enabled = NO; }
	if (geometryAsset != remasterEditingProfile.assets.end() && geometryAsset->second.geometryByFacing)
	{ remasterHeightApplyAnimationButton.enabled = NO; remasterNormalApplyAnimationButton.enabled = NO; }
	// Flip-set vectors already describe gameplay space, so this legacy option
	// cannot change their direction a second time.
	if (byFacing && remasterGeometryFacing) remasterNormalIgnoreTransformButton.enabled = NO;
	remasterEmissionColor.enabled = editable;
	remasterEmissionPaintMode.enabled = editable;
	remasterEmissionColorScope.enabled = editable;
	const bool colorScopeAvailable = remasterEmissionColorScope.indexOfSelectedItem != 0 || remasterTilePixelSelected;
	remasterEmissionFromTileButton.enabled = editable && colorScopeAvailable;
	remasterEmissionFromAnimationButton.enabled = editable && remasterVariants.size() > 1 && colorScopeAvailable;
	remasterArtworkVisibleButton.enabled = layer == RemasterEditorEmission;
	remasterEmissionVisibleButton.enabled = layer == RemasterEditorEmission;
	remasterResetLayerButton.enabled = editable && remasterEditingProfile.assets.count(remasterSelectedTile);
	remasterResetTileButton.enabled = editable && remasterEditingProfile.assets.count(remasterSelectedTile);
	remasterCopyLayerButton.enabled = !remasterVariants.empty() && layer != RemasterEditorArtwork;
	remasterPasteLayerButton.enabled = editable && layer != RemasterEditorArtwork && RemasterClipboardAvailable(layer);
	bool canFlip = false;
	if (editable)
		for (const auto &tileId : remasterSelectedTiles)
		{
			auto metadata = EffectiveRemasterMetadata(tileId);
			if (S9xRemasterHasGeometryVariants(metadata) && (layer == RemasterEditorHeight || layer == RemasterEditorNormal))
			{
				const auto &geometry = RemasterEditingGeometry(metadata);
				canFlip |= layer == RemasterEditorHeight ? geometry.hasHeight : geometry.hasNormals;
			}
			else
				canFlip |= S9xRemasterFlipMetadataLayer(metadata, static_cast<int>(layer), true);
		}
	remasterFlipLayerHorizontalButton.enabled = remasterFlipLayerVerticalButton.enabled = canFlip;
	for (NSButton *button in remasterShiftLayerButtons) button.enabled = canFlip;
	remasterCopyTileButton.enabled = !remasterVariants.empty();
	remasterPasteTileButton.enabled = editable && RemasterClipboardAvailable(-1);
	remasterApplyTileToVariantsButton.enabled = editable && remasterVariants.size() > 1 &&
		RemasterMetadataHasData(EffectiveRemasterMetadata(remasterSelectedTile));
	if (!remasterTileContextName.empty()) {
		remasterHeightApplyAnimationButton.enabled = remasterNormalApplyAnimationButton.enabled = NO;
		remasterApplyTileToVariantsButton.enabled = NO;
	}
	if (layer == RemasterEditorHeight)
	{
		RemasterHeightSampling sampling;
		const auto *authored = RemasterAuthoredHeight(remasterSelectedTile, sampling);
		const uint32_t id = S9xRemasterInspectorHeightInstance(remasterReplayFrame,
			remasterSelectedTile, remasterHeightInstanceId, remasterHeightOccurrences);
		const auto *instance = id ? &remasterReplayFrame.tileInstances[id - 1] : nullptr;
		const auto effective = S9xRemasterInspectorEffectiveHeight(instance, authored);
		NSString *authoredValue = !authored ? @"missing (not zero)" : !remasterTilePixelSelected ? @"select a pixel" :
			[NSString stringWithFormat:@"%d", int((*authored)[remasterSelectedTilePixel])];
		NSString *effectiveValue = !id ? (remasterHeightOccurrences.empty() ? @"no occurrence in this frame" : @"ambiguous — choose occurrence") :
			!effective.available ? @"missing" : !remasterTilePixelSelected ? @"select a pixel" :
			[NSString stringWithFormat:@"%+d", effective.at(remasterSelectedTilePixel)];
		NSString *source = effective.placementOverride ? @"placement-local override" :
			authored ? @"authored local + placement base" : @"base only; local missing (runtime zero fallback)";
		NSString *localRange = @"No authored samples";
		if (authored)
		{
			const auto bounds = std::minmax_element(authored->begin(), authored->end());
			localRange = [NSString stringWithFormat:@"Authored range %d…%d · %@ · grayscale uses tile-local contrast",
				int(*bounds.first), int(*bounds.second), *bounds.first == *bounds.second ? @"flat" : @"gradient"];
		}
		remasterHeightReport.stringValue = [NSString stringWithFormat:@"Canonical pixel %@ · Authored / %@: %@ · Effective: %@\n%@\n%@\n%@",
			remasterTilePixelSelected ? [NSString stringWithFormat:@"(%lu,%lu)", static_cast<unsigned long>(remasterSelectedTilePixel % 8), static_cast<unsigned long>(remasterSelectedTilePixel / 8)] : @"—",
			remasterEditingProfileLoaded ? @"Profile" : @"Captured profile", authoredValue, effectiveValue,
			id ? [NSString stringWithFormat:@"Instance %u · signed base %+d · source: %@", id, effective.base, source] : @"Placement base/source unavailable; no arbitrary plane selected",
			localRange,
			RemasterEffectiveHeightPreview() ? [NSString stringWithFormat:@"Read only · nearest effective samples · display range %ld…%ld", remasterHeightPreviewMin, remasterHeightPreviewMax] : @"Edits affect local profile samples only; placement geometry is never copied"];
		remasterValueLabel.stringValue = authored ? @"Local height (-32768–32767)" : @"No authored height";
		// Only local authoring data can become a brush value. Missing metadata
		// leaves the explicit brush unchanged; it must not sample effective height.
		if (authored && remasterTilePixelSelected)
		{
			remasterValueBrush.integerValue = (*authored)[remasterSelectedTilePixel];
			remasterValueInput.integerValue = remasterValueBrush.integerValue;
		}
	}
	else if (remasterValueLabel && layer == RemasterEditorOcclusion)
	{
		if (!remasterTilePixelSelected)
			remasterValueLabel.stringValue = @"Select a pixel";
		else
		{
			uint8_t value = 255;
			auto editableMetadata = remasterEditingProfile.assets.find(remasterSelectedTile);
			if (editableMetadata != remasterEditingProfile.assets.end() && editableMetadata->second.hasOcclusion)
				value = editableMetadata->second.occlusion[remasterSelectedTilePixel];
			else
			{
				const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(
					remasterReplayFrame, remasterSelectedTile);
				if (captured && captured->hasOcclusion)
					value = captured->occlusion[remasterSelectedTilePixel];
			}
			remasterValueBrush.integerValue = value;
			remasterValueInput.integerValue = value;
			remasterValueLabel.stringValue = [NSString stringWithFormat:@"Pixel (%lu, %lu): %u",
				static_cast<unsigned long>(remasterSelectedTilePixel % 8),
				static_cast<unsigned long>(remasterSelectedTilePixel / 8), value];
		}
	}
	else if (remasterValueLabel && layer == RemasterEditorEmission)
	{
		if (!remasterTilePixelSelected)
			remasterValueLabel.stringValue = [NSString stringWithFormat:@"Intensity %ld", remasterValueBrush.integerValue];
		else
		{
			const RemasterAssetMetadata *editableMetadata = nullptr;
			auto found = remasterEditingProfile.assets.find(remasterSelectedTile);
			if (found != remasterEditingProfile.assets.end() && found->second.hasEmission)
				editableMetadata = &found->second;
			const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, remasterSelectedTile);
			const uint8_t *rgba = editableMetadata ? editableMetadata->emissionRgba.data() :
				(captured && captured->hasEmission ? captured->emissionRgba.data() : nullptr);
			const size_t offset = remasterSelectedTilePixel * 4;
			uint32_t artworkRed = 0;
			uint32_t artworkGreen = 0;
			uint32_t artworkBlue = 0;
			uint32_t artworkCount = 0;
			for (uint32_t occurrence : S9xRemasterFrameOccurrences(remasterReplayFrame, remasterSelectedTile))
			{
				const RemasterFramePixel &pixel = remasterReplayFrame.mainPixels[occurrence];
				if (pixel.tilePixel != remasterSelectedTilePixel)
					continue;
				const uint16_t color = remasterReplayFrame.originalRgb555[occurrence];
				artworkRed += (((color >> 10) & 31) * 527 + 23) >> 6;
				artworkGreen += (((color >> 5) & 31) * 527 + 23) >> 6;
				artworkBlue += ((color & 31) * 527 + 23) >> 6;
				artworkCount++;
			}
			if (rgba)
			{
				remasterValueBrush.integerValue = rgba[offset + 3];
				remasterValueInput.integerValue = rgba[offset + 3];
				remasterEmissionColor.color = [NSColor colorWithCalibratedRed:rgba[offset] / 255.0
					green:rgba[offset + 1] / 255.0 blue:rgba[offset + 2] / 255.0 alpha:1.0];
				NSString *emission = rgba[offset + 3] ?
					[NSString stringWithFormat:@"Emission %u,%u,%u  I %u", rgba[offset], rgba[offset + 1],
						rgba[offset + 2], rgba[offset + 3]] : @"Emission off";
				remasterValueLabel.stringValue = artworkCount ?
					[NSString stringWithFormat:@"%@  |  Artwork %u,%u,%u", emission, artworkRed / artworkCount,
						artworkGreen / artworkCount, artworkBlue / artworkCount] : emission;
			}
			else
				remasterValueLabel.stringValue = artworkCount ?
					[NSString stringWithFormat:@"No emission  |  Artwork %u,%u,%u", artworkRed / artworkCount,
						artworkGreen / artworkCount, artworkBlue / artworkCount] : @"No emission";
		}
	}
	else if (remasterValueLabel && layer == RemasterEditorNormal)
	{
		const size_t normalPixel = remasterTilePixelSelected ? remasterSelectedTilePixel : 0;
		const std::array<uint8_t, 3> first = EffectiveRemasterNormal(
			remasterSelectedTiles.empty() ? remasterSelectedTile : remasterSelectedTiles.front(), normalPixel);
		std::fill(std::begin(remasterNormalComponentMixed), std::end(remasterNormalComponentMixed), false);
		for (const RemasterTileContentId &tileId : remasterSelectedTiles)
		{
			const std::array<uint8_t, 3> value = EffectiveRemasterNormal(tileId, normalPixel);
			for (size_t component = 0; component < 3; component++)
				remasterNormalComponentMixed[component] |= value[component] != first[component];
		}
		float x, y, z;
		DecodeRemasterNormal(first.data(), x, y, z);
		const float values[3] = { x, y, z };
		NSTextField *inputs[3] = { remasterNormalXInput, remasterNormalYInput, remasterNormalZInput };
		for (size_t component = 0; component < 3; component++)
		{
			inputs[component].placeholderString = remasterNormalComponentMixed[component] ? @"-" :
				@[@"X", @"Y", @"Z"][component];
			if (remasterNormalComponentMixed[component])
				inputs[component].stringValue = @"";
			else
				inputs[component].floatValue = values[component];
		}
		[remasterNormalPreset selectItemAtIndex:6];
		if (remasterTilePixelSelected)
			remasterValueLabel.stringValue = [NSString stringWithFormat:@"Pixel (%lu, %lu): %@, %@, %@%@",
				static_cast<unsigned long>(remasterSelectedTilePixel % 8),
				static_cast<unsigned long>(remasterSelectedTilePixel / 8),
				remasterNormalComponentMixed[0] ? @"-" : [NSString stringWithFormat:@"%.3f", x],
				remasterNormalComponentMixed[1] ? @"-" : [NSString stringWithFormat:@"%.3f", y],
				remasterNormalComponentMixed[2] ? @"-" : [NSString stringWithFormat:@"%.3f", z],
				remasterSelectedTiles.size() > 1 ? [NSString stringWithFormat:@"  (%lu tiles)",
					static_cast<unsigned long>(remasterSelectedTiles.size())] : @""];
		else
			remasterValueLabel.stringValue = @"Select a pixel for Update, or use Fill Entire Tile";
	}
	else if (remasterValueLabel && layer == RemasterEditorMaterial)
	{
		if (!remasterTilePixelSelected)
			remasterValueLabel.stringValue = @"Select a pixel";
		else
		{
			std::string value;
			auto editableMetadata = remasterEditingProfile.assets.find(remasterSelectedTile);
			if (editableMetadata != remasterEditingProfile.assets.end() && editableMetadata->second.hasMaterialSelectors)
				value = editableMetadata->second.materialSelectors[remasterSelectedTilePixel];
			else
			{
				const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(
					remasterReplayFrame, remasterSelectedTile);
				if (captured && captured->hasMaterialSelectors)
					value = captured->materialSelectors[remasterSelectedTilePixel];
			}
			remasterValueLabel.stringValue = [NSString stringWithFormat:@"Pixel (%lu, %lu): %@",
				static_cast<unsigned long>(remasterSelectedTilePixel % 8),
				static_cast<unsigned long>(remasterSelectedTilePixel / 8),
				value.empty() ? @"Inherit" : [NSString stringWithUTF8String:value.c_str()]];
		}
	}
	if (remasterValueLabel && layer != RemasterEditorArtwork && !remasterEditingProfileLoaded)
		remasterValueLabel.toolTip = @"Load the matching remaster profile to edit this layer.";
	else if (remasterValueLabel)
		remasterValueLabel.toolTip = nil;
	if (layer == RemasterEditorNormal)
	{
		const std::array<uint8_t, 3> focused = EffectiveRemasterNormal(remasterSelectedTile,
			remasterTilePixelSelected ? remasterSelectedTilePixel : 0);
		float x, y, z;
		DecodeRemasterNormal(focused.data(), x, y, z);
		remasterNormalAxesView.normalX = x;
		remasterNormalAxesView.normalY = y;
		remasterNormalAxesView.normalZ = z;
		// Byte encoding cannot represent exact zero; keep the chosen hemisphere at the rim.
		if (std::fabs(z) > 0.005f)
			remasterNormalBackButton.state = z < 0.0f ? NSControlStateValueOn : NSControlStateValueOff;
		remasterNormalAxesView.toolTip = @"Drag inside the circle: center = Z, rim = XY, between = XYZ tilt. Release to apply. Back (-Z) selects the rear hemisphere. +X right, +Y down.";
	}
	else
		remasterNormalAxesView.toolTip = nil;
	[remasterNormalAxesView setNeedsDisplay:YES];
	remasterSaveProfileButton.enabled = remasterEditingProfileLoaded && remasterEditingProfileDirty;
	remasterNpcSaveButton.enabled = remasterEditingProfileLoaded && remasterEditingProfileDirty;
	if (remasterSettingsSaveButton)
		remasterSettingsSaveButton.enabled = remasterEditingProfileLoaded && remasterEditingProfileDirty;
}

- (BOOL)remasterTilePixelAtPoint:(NSPoint)point pixel:(size_t *)pixel
{
	const NSRect imageRect = [remasterTilePreview.cell drawingRectForBounds:remasterTilePreview.bounds];
	if (!NSPointInRect(point, imageRect) || NSWidth(imageRect) <= 0 || NSHeight(imageRect) <= 0)
		return NO;
	const size_t x = std::min<size_t>(7, static_cast<size_t>((point.x - NSMinX(imageRect)) * 8 / NSWidth(imageRect)));
	const size_t y = std::min<size_t>(7, static_cast<size_t>((NSMaxY(imageRect) - point.y) * 8 / NSHeight(imageRect)));
	*pixel = S9xRemasterGeometryCanonicalPixel(static_cast<unsigned>(y * 8 + x), RemasterInspectorPreviewFlip());
	return YES;
}

- (void)beginRemasterTileStrokeAtPoint:(NSPoint)point
{
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (remasterVariants.empty() || layer == RemasterEditorArtwork || layer == RemasterEditorMetadata)
		return;
	remasterStrokeSnapshot = nil;
	remasterStrokeVisited = 0;
	remasterStrokeChanged = false;
	remasterStrokeDragging = false;
	remasterStrokeStartPoint = point;
	remasterStrokeValue = static_cast<int16_t>(remasterValueBrush.integerValue);
	if (layer == RemasterEditorDoorframe)
		remasterStrokeValue = remasterDoorframeBrush.state == NSControlStateValueOn;
	if (layer == RemasterEditorNormal)
	{
		const std::array<uint8_t, 3> focused = EffectiveRemasterNormal(remasterSelectedTile,
			remasterTilePixelSelected ? remasterSelectedTilePixel : 0);
		remasterStrokeNormal = focused;
	}
	if (layer == RemasterEditorEmission)
	{
		NSColor *color = [remasterEmissionColor.color colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
		remasterStrokeEmissionRgb = {
			static_cast<uint8_t>(std::lround(color.redComponent * 255.0)),
			static_cast<uint8_t>(std::lround(color.greenComponent * 255.0)),
			static_cast<uint8_t>(std::lround(color.blueComponent * 255.0))
		};
		remasterStrokeEmissionIntensity = static_cast<uint8_t>(remasterValueBrush.integerValue);
		remasterStrokeEmissionMode = remasterEmissionPaintMode.indexOfSelectedItem;
	}
	size_t pixel;
	if (![self remasterTilePixelAtPoint:point pixel:&pixel])
		return;
	remasterTilePixelSelected = true;
	remasterSelectedTilePixel = pixel;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)continueRemasterTileStrokeAtPoint:(NSPoint)point
{
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty() || layer == RemasterEditorArtwork || layer == RemasterEditorMetadata)
		return;
	size_t pixel;
	if (![self remasterTilePixelAtPoint:point pixel:&pixel])
		return;
	if (!remasterStrokeDragging)
	{
		size_t startPixel;
		if (![self remasterTilePixelAtPoint:remasterStrokeStartPoint pixel:&startPixel] || pixel == startPixel)
			return;
		remasterStrokeDragging = true;
		[self continueRemasterTileStrokeAtPoint:remasterStrokeStartPoint];
	}
	remasterTilePixelSelected = true;
	remasterSelectedTilePixel = pixel;
	const uint64_t pixelBit = uint64_t(1) << pixel;
	if (remasterStrokeVisited & pixelBit)
		return;
	remasterStrokeVisited |= pixelBit;
	if (!remasterStrokeSnapshot)
	{
		std::set<RemasterTileContentId> touched(remasterSelectedTiles.begin(), remasterSelectedTiles.end());
		touched.insert(remasterSelectedTile);
		remasterStrokeSnapshot = CaptureRemasterAssetSnapshot(touched);
	}
	if (layer == RemasterEditorDoorframe && !remasterEditingProfile.assets.count(remasterSelectedTile))
		remasterEditingProfile.assets[remasterSelectedTile] = EffectiveRemasterMetadata(remasterSelectedTile);
	RemasterAssetMetadata &metadata = remasterEditingProfile.assets[remasterSelectedTile];
	metadata.tileId = remasterSelectedTile;
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, remasterSelectedTile);
	if (layer == RemasterEditorDoorframe)
	{
		remasterStrokeChanged |= S9xRemasterPaintDoorframe(metadata, static_cast<unsigned>(pixel), remasterStrokeValue != 0);
	}
	else if (layer == RemasterEditorMaterial)
	{
		const bool hadLayer = metadata.hasMaterialSelectors;
		if (!metadata.hasMaterialSelectors && captured && captured->hasMaterialSelectors)
			metadata.materialSelectors = captured->materialSelectors;
		metadata.hasMaterialSelectors = true;
		const std::string value = remasterMaterialBrush.indexOfSelectedItem == 0 ? "" :
			remasterMaterialBrush.titleOfSelectedItem.UTF8String;
		remasterStrokeChanged |= !hadLayer || metadata.materialSelectors[pixel] != value;
		metadata.materialSelectors[pixel] = value;
	}
	else if (layer == RemasterEditorOcclusion)
	{
		const bool hadLayer = metadata.hasOcclusion;
		if (!metadata.hasOcclusion)
			metadata.occlusion = captured && captured->hasOcclusion ? captured->occlusion : S9xRemasterDefaultOcclusion();
		metadata.hasOcclusion = true;
		const uint8_t value = remasterStrokeValue;
		remasterStrokeChanged |= !hadLayer || metadata.occlusion[pixel] != value;
		metadata.occlusion[pixel] = value;
	}
	else if (layer == RemasterEditorHeight)
	{
		auto &g = RemasterEditingGeometry(metadata);
		const bool hadLayer = g.hasHeight;
		if (!hadLayer) { g.height.fill(0); g.hasHeight = true; g.heightSampling = RemasterHeightSampling::Nearest; }
		const int16_t value = remasterStrokeValue;
		remasterStrokeChanged |= !hadLayer || g.height[pixel] != value;
		g.height[pixel] = value;
	}
	else if (layer == RemasterEditorNormal)
	{
		for (const RemasterTileContentId &tileId : remasterSelectedTiles)
		{
			RemasterAssetMetadata &target = remasterEditingProfile.assets[tileId];
			target.tileId = tileId;
			auto &g = RemasterEditingGeometry(target);
			const RemasterFrameAssetMetadata *targetCaptured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, tileId);
			const bool hadLayer = g.hasNormals;
			if (!hadLayer && remasterTileContextName.empty() && !S9xRemasterHasGeometryVariants(target) && targetCaptured && targetCaptured->hasNormals)
				g.normalXyz = targetCaptured->normalXyz;
			else if (!hadLayer) for (unsigned p = 0; p < 64; p++) { g.normalXyz[p*3] = g.normalXyz[p*3+1] = 128; g.normalXyz[p*3+2] = 255; }
			g.hasNormals = true;
			const size_t offset = pixel * 3;
			for (size_t component = 0; component < 3; component++)
			{
				remasterStrokeChanged |= !hadLayer || g.normalXyz[offset + component] != remasterStrokeNormal[component];
				g.normalXyz[offset + component] = remasterStrokeNormal[component];
			}
		}
	}
	else
	{
		const bool hadLayer = metadata.hasEmission;
		if (!metadata.hasEmission && captured && captured->hasEmission)
			metadata.emissionRgba = captured->emissionRgba;
		metadata.hasEmission = true;
		const size_t offset = pixel * 4;
		if (remasterStrokeEmissionMode != RemasterEmissionPaintIntensity)
		{
			for (size_t component = 0; component < remasterStrokeEmissionRgb.size(); component++)
			{
				remasterStrokeChanged |= !hadLayer ||
					metadata.emissionRgba[offset + component] != remasterStrokeEmissionRgb[component];
				metadata.emissionRgba[offset + component] = remasterStrokeEmissionRgb[component];
			}
		}
		if (remasterStrokeEmissionMode != RemasterEmissionPaintRgb)
		{
			remasterStrokeChanged |= !hadLayer ||
				metadata.emissionRgba[offset + 3] != remasterStrokeEmissionIntensity;
			metadata.emissionRgba[offset + 3] = remasterStrokeEmissionIntensity;
		}
	}
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion,
		layer == RemasterEditorDoorframe ? 21 : (layer == RemasterEditorNormal ? 5 : (layer == RemasterEditorEmission ? 3 : 2)));
	remasterMetadataNeedsSync |= remasterStrokeChanged;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)endRemasterTileStroke
{
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (!remasterStrokeDragging && (layer == RemasterEditorMaterial || layer == RemasterEditorDoorframe))
	{
		remasterStrokeDragging = true;
		[self continueRemasterTileStrokeAtPoint:remasterStrokeStartPoint];
	}
	if (remasterStrokeSnapshot && remasterStrokeChanged)
	{
		[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:)
			object:remasterStrokeSnapshot];
		[remasterUndoManager setActionName:layer == RemasterEditorDoorframe ? @"Paint Doorframe" : layer == RemasterEditorMaterial ? @"Paint Material" :
			(layer == RemasterEditorOcclusion ? @"Paint Occlusion" :
			(layer == RemasterEditorHeight ? @"Paint Height" :
			(layer == RemasterEditorNormal ? @"Paint Normal" : @"Paint Emission")))];
		remasterNonSceneDirty = true;
		remasterEditingProfileDirty = true;
	}
	remasterStrokeSnapshot = nil;
	remasterStrokeDragging = false;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)restoreRemasterAssetSnapshot:(S9xRemasterAssetUndoSnapshot *)snapshot
{
	std::set<RemasterTileContentId> touched = snapshot->missing;
	for (const auto &entry : snapshot->values)
		touched.insert(entry.first);
	S9xRemasterAssetUndoSnapshot *inverse = CaptureRemasterAssetSnapshot(touched);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:)
		object:inverse];
	for (const RemasterTileContentId &tileId : snapshot->missing)
		remasterEditingProfile.assets.erase(tileId);
	for (const auto &entry : snapshot->values)
		remasterEditingProfile.assets[entry.first] = entry.second;
	for (const auto &tile : touched) {
		auto context = snapshot->contexts.find(tile);
		if (context == snapshot->contexts.end()) remasterEditingProfile.tileContexts.erase(tile);
		else remasterEditingProfile.tileContexts[tile] = context->second;
	}
	for(auto b=remasterEditingProfile.contextBindings.begin();b!=remasterEditingProfile.contextBindings.end();) {
		auto c=remasterEditingProfile.tileContexts.find(b->second.tile);
		if(c==remasterEditingProfile.tileContexts.end() || c->second.size()<2 || !c->second.count(b->second.name))b=remasterEditingProfile.contextBindings.erase(b);else ++b;
	}
	remasterEditingProfile.schemaVersion = snapshot->schemaVersion;
	std::string current;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	remasterMetadataNeedsSync = true;
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterDoorframe:(id)sender
{
	if (remasterLayerSelector.indexOfSelectedItem != RemasterEditorDoorframe || !remasterTilePixelSelected) return;
	// Use the same snapshot/undo and canonical-pixel paint path as a stroke.
	remasterStrokeSnapshot = nil;
	remasterStrokeVisited = 0;
	remasterStrokeChanged = false;
	remasterStrokeDragging = true;
	remasterStrokeValue = remasterDoorframeBrush.state == NSControlStateValueOn;
	const NSRect rect = [remasterTilePreview.cell drawingRectForBounds:remasterTilePreview.bounds];
	const NSPoint point = NSMakePoint(NSMinX(rect) + (remasterSelectedTilePixel % 8 + 0.5) * NSWidth(rect) / 8,
		NSMaxY(rect) - (remasterSelectedTilePixel / 8 + 0.5) * NSHeight(rect) / 8);
	[self continueRemasterTileStrokeAtPoint:point];
	[self endRemasterTileStroke];
}

- (void)applyRemasterDoorframeToAnimation:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterVariants.empty()) return;
	const auto source = EffectiveRemasterMetadata(remasterSelectedTile);
	if (source.tileRole != RemasterTileRole::Door) return;
	std::set<RemasterTileContentId> touched(remasterVariants.begin(), remasterVariants.end());
	auto snapshot = CaptureRemasterAssetSnapshot(touched);
	for (const auto &id : touched)
	{
		auto metadata = EffectiveRemasterMetadata(id);
		metadata.tileId = id;
		// Explicit operation authors the Door role for these animation identities.
		CopyRemasterMetadataLayer(source, metadata, RemasterEditorMetadata);
		CopyRemasterMetadataLayer(source, metadata, RemasterEditorDoorframe);
		remasterEditingProfile.assets[id] = metadata;
	}
	remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 21u);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
	[remasterUndoManager setActionName:@"Apply Doorframe to Animation"];
	remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterPixelValue:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	const NSInteger layer = remasterLayerSelector.indexOfSelectedItem;
	if (sender == remasterValueInput)
	{
		if ([remasterValueInput.stringValue isEqualToString:@"-"] || remasterValueInput.stringValue.length == 0) return;
		remasterValueBrush.integerValue = std::max<NSInteger>(layer == RemasterEditorHeight ? -32768 : 0,
			std::min<NSInteger>(layer == RemasterEditorHeight ? 32767 : 255, remasterValueInput.integerValue));
	}
	remasterValueInput.integerValue = remasterValueBrush.integerValue;
	if (!remasterEditingProfileLoaded || remasterVariants.empty() || !remasterTilePixelSelected ||
		(layer != RemasterEditorOcclusion && layer != RemasterEditorHeight && layer != RemasterEditorEmission))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	RemasterAssetMetadata &metadata = remasterEditingProfile.assets[remasterSelectedTile];
	metadata.tileId = remasterSelectedTile;
	const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, remasterSelectedTile);
	const int16_t value = static_cast<int16_t>(remasterValueBrush.integerValue);
	if (layer == RemasterEditorOcclusion)
	{
		const bool hadLayer = metadata.hasOcclusion;
		if (!metadata.hasOcclusion)
			metadata.occlusion = captured && captured->hasOcclusion ? captured->occlusion : S9xRemasterDefaultOcclusion();
		metadata.hasOcclusion = true;
		if (hadLayer && metadata.occlusion[remasterSelectedTilePixel] == value)
			return;
		metadata.occlusion[remasterSelectedTilePixel] = value;
	}
	else if (layer == RemasterEditorHeight)
	{
		auto &g = RemasterEditingGeometry(metadata);
		const bool hadLayer = g.hasHeight;
		if (!hadLayer) { g.height.fill(0); g.hasHeight = true; }
		if (hadLayer && g.height[remasterSelectedTilePixel] == value)
			return;
		g.height[remasterSelectedTilePixel] = value;
		g.heightSampling = remasterHeightSamplingSelector.indexOfSelectedItem == 1 ?
			RemasterHeightSampling::Linear : RemasterHeightSampling::Nearest;
	}
	else
	{
		const bool hadLayer = metadata.hasEmission;
		if (!metadata.hasEmission && captured && captured->hasEmission)
			metadata.emissionRgba = captured->emissionRgba;
		metadata.hasEmission = true;
		const size_t offset = remasterSelectedTilePixel * 4 + 3;
		if (hadLayer && metadata.emissionRgba[offset] == value)
			return;
		metadata.emissionRgba[offset] = value;
	}
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion,
		layer == RemasterEditorEmission ? 3 : 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:layer == RemasterEditorOcclusion ? @"Change Occlusion" :
		(layer == RemasterEditorHeight ? @"Change Height" : @"Change Emission Intensity")];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)controlTextDidChange:(NSNotification *)notification
{
	if (notification.object == remasterAreaBrightness || notification.object == remasterAreaDirection[0] ||
		notification.object == remasterAreaDirection[1] || notification.object == remasterAreaDirection[2])
	{
		[self changeRemasterAreaSettings:nil];
		return;
	}
	if (notification.object == remasterValueInput)
		[self changeRemasterPixelValue:remasterValueInput];
	else if (notification.object == remasterHeightScaleInput)
		remasterSettingsSaveButton.enabled = YES;
	else if (notification.object == remasterCameraDirectionInputs[0] ||
		notification.object == remasterCameraDirectionInputs[1] ||
		notification.object == remasterCameraDirectionInputs[2])
		remasterSettingsSaveButton.enabled = YES;
	else if (notification.object == remasterIndirectRoughnessInput)
		remasterSettingsSaveButton.enabled = YES;
	else if (notification.object == remasterOriginalSceneInput)
		remasterSettingsSaveButton.enabled = YES;
	else if (notification.object == remasterReflectanceBoostInput)
		remasterSettingsSaveButton.enabled = YES;
}

- (void)stepRemasterPixelValue:(NSButton *)sender
{
	const bool height = remasterLayerSelector.indexOfSelectedItem == RemasterEditorHeight;
	remasterValueBrush.integerValue = std::max<NSInteger>(height ? -32768 : 0,
		std::min<NSInteger>(height ? 32767 : 255, remasterValueBrush.integerValue + sender.tag));
	remasterValueInput.integerValue = remasterValueBrush.integerValue;
	[self changeRemasterPixelValue:remasterValueBrush];
}

- (void)fillRemasterOpaqueOcclusion:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty())
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	const uint8_t value = static_cast<uint8_t>(remasterValueBrush.integerValue);
	bool changed = false;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		const RemasterFrameAsset *asset = S9xRemasterFrameAssetForTile(remasterReplayFrame, tileId);
		if (!asset)
			continue;
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		const bool hadLayer = metadata.hasOcclusion;
		metadata.hasOcclusion = true;
		for (size_t pixel = 0; pixel < 64; pixel++)
		{
			const uint8_t target = asset->indices[pixel] ? value : 0;
			changed |= !hadLayer || metadata.occlusion[pixel] != target;
			metadata.occlusion[pixel] = target;
		}
	}
	if (!changed)
		return;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Fill Opaque Occlusion"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterHeightSampling:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty())
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	RemasterAssetMetadata &metadata = remasterEditingProfile.assets[remasterSelectedTile];
	metadata.tileId = remasterSelectedTile;
	const RemasterHeightSampling sampling = remasterHeightSamplingSelector.indexOfSelectedItem == 1 ?
		RemasterHeightSampling::Linear : RemasterHeightSampling::Nearest;
	auto &g = RemasterEditingGeometry(metadata);
	if (g.hasHeight && g.heightSampling == sampling)
		return;
	if (!g.hasHeight) { g.height.fill(0); g.hasHeight = true; }
	g.heightSampling = sampling;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Change Height Sampling"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterGeometryByFacing:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	const bool enabled = remasterGeometryByFacingButton.state != NSControlStateValueOff;
	std::set<RemasterTileContentId> touched(remasterSelectedTiles.begin(), remasterSelectedTiles.end());
	if (!remasterUndoManager) remasterUndoManager = [[NSUndoManager alloc] init];
	auto snapshot = CaptureRemasterAssetSnapshot(touched);
	for (const auto &tile : touched)
	{
		auto &asset = remasterEditingProfile.assets[tile]; asset.tileId = tile; asset.geometryByFlip = enabled;
		if (!RemasterMetadataHasData(asset)) remasterEditingProfile.assets.erase(tile);
	}
	if (enabled) remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 25u);
	if (!enabled) remasterGeometryFacing = 0;
	else if (!remasterGeometryFacing) remasterGeometryFacing = 1;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
	[remasterUndoManager setActionName:@"Change Flip-Specific Geometry"];
	remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
	SyncRemasterEditingMetadataToFrame();
	[self showRemasterVariantAtIndex:remasterVariantIndex];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)changeRemasterGeometryFacing:(id)sender
{
	[self endRemasterTileStroke];
	remasterGeometryFacing = static_cast<unsigned>(std::max<NSInteger>(0, remasterGeometryFacingSelector.indexOfSelectedItem));
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterNormalIgnoreTransform:(id)sender
{
	if (!remasterTileContextName.empty()) {
		std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
		for (const auto &tile : remasterSelectedTiles) if (auto *c = RemasterSelectedContext(tile))
			c->normalIgnoreTileTransform = remasterNormalIgnoreTransformButton.state != NSControlStateValueOff;
		[self finishRemasterContextEdit:@(before.c_str()) action:@"Change Context Normal Transform"];
		return;
	}
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty()) return;
	const bool enabled = remasterNormalIgnoreTransformButton.state != NSControlStateValueOff;
	std::set<RemasterTileContentId> touched;
	for (const auto &tile : remasterSelectedTiles)
		if (EffectiveRemasterMetadata(tile).normalIgnoreTileTransform != enabled) touched.insert(tile);
	if (touched.empty()) return;
	if (!remasterUndoManager) remasterUndoManager = [[NSUndoManager alloc] init];
	auto snapshot = CaptureRemasterAssetSnapshot(touched);
	for (const auto &tile : touched)
	{
		auto &metadata = remasterEditingProfile.assets[tile];
		metadata.tileId = tile;
		metadata.normalIgnoreTileTransform = enabled;
		if (!RemasterMetadataHasData(metadata)) remasterEditingProfile.assets.erase(tile);
	}
	if (enabled) remasterEditingProfile.schemaVersion = std::max(remasterEditingProfile.schemaVersion, 23u);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterAssetSnapshot:) object:snapshot];
	[remasterUndoManager setActionName:@"Change Normal Tile Transform"];
	remasterMetadataNeedsSync = remasterNonSceneDirty = remasterEditingProfileDirty = true;
	SyncRemasterEditingMetadataToFrame();
	[self showRemasterVariantAtIndex:remasterVariantIndex];
	[remasterNpcPoseView setNeedsDisplay:YES];
}

- (void)changeRemasterOppositeFacingDirect:(id)sender
{
	if (!remasterTileContextName.empty()) {
		std::string before; std::vector<RemasterProfileDiagnostic> diagnostics;
		if (!S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics)) return;
		for (const auto &tile : remasterSelectedTiles) if (auto *c = RemasterSelectedContext(tile))
			c->directLightingOppositeFacing = remasterOppositeFacingDirectButton.state != NSControlStateValueOff;
		[self finishRemasterContextEdit:@(before.c_str()) action:@"Change Context Direct Lighting"];
		return;
	}
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty())
		return;
	// A displayed mixed value resolves to enabled if AppKit sends it through the action.
	const bool enabled = remasterOppositeFacingDirectButton.state != NSControlStateValueOff;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	bool changed = false;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		changed |= metadata.directLightingOppositeFacing != enabled;
		metadata.directLightingOppositeFacing = enabled;
		if (!RemasterMetadataHasData(metadata))
			remasterEditingProfile.assets.erase(tileId);
	}
	if (!changed)
		return;
	if (enabled)
		remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 6);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Change Opposite-Facing Direct Light"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)fillRemasterTileHeight:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty())
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	RemasterAssetMetadata &metadata = remasterEditingProfile.assets[remasterSelectedTile];
	metadata.tileId = remasterSelectedTile;
	const int16_t value = static_cast<int16_t>(remasterValueBrush.integerValue);
	auto &g = RemasterEditingGeometry(metadata);
	const bool changed = !g.hasHeight ||
		std::any_of(g.height.begin(), g.height.end(), [value](int16_t item) { return item != value; });
	if (!changed)
		return;
	g.height.fill(value);
	g.hasHeight = true;
	g.heightSampling = remasterHeightSamplingSelector.indexOfSelectedItem == 1 ?
		RemasterHeightSampling::Linear : RemasterHeightSampling::Nearest;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Fill Tile Height"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)stepRemasterTileHeight:(NSButton *)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.empty() ||
		(sender.tag != -8 && sender.tag != -1 && sender.tag != 1 && sender.tag != 8))
		return;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	auto existing = remasterEditingProfile.assets.find(remasterSelectedTile);
	const bool hadHeight = existing != remasterEditingProfile.assets.end() && RemasterEditingGeometry(existing->second).hasHeight;
	std::array<int16_t, 64> height = hadHeight ? RemasterEditingGeometry(existing->second).height : std::array<int16_t, 64>{};
	RemasterHeightSampling sampling = hadHeight ? RemasterEditingGeometry(existing->second).heightSampling : RemasterHeightSampling::Nearest;
	bool changed = false;
	for (int16_t &sample : height)
	{
		const int16_t adjusted = static_cast<int16_t>(std::max(-32768, std::min(32767,
			static_cast<int>(sample) + static_cast<int>(sender.tag))));
		changed |= sample != adjusted;
		sample = adjusted;
	}
	if (!changed)
		return;
	RemasterAssetMetadata &metadata = remasterEditingProfile.assets[remasterSelectedTile];
	metadata.tileId = remasterSelectedTile;
	auto &g = RemasterEditingGeometry(metadata);
	g.height = height;
	g.hasHeight = true;
	g.heightSampling = sampling;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:[NSString stringWithFormat:@"%@ Tile Height by %ld",
		sender.tag > 0 ? @"Increase" : @"Decrease", static_cast<long>(sender.tag > 0 ? sender.tag : -sender.tag)]];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)applyRemasterHeightToAnimation:(id)sender
{
	if (RemasterEffectiveHeightPreview()) return;
	if (!remasterEditingProfileLoaded || remasterVariants.size() < 2)
		return;
	auto source = remasterEditingProfile.assets.find(remasterSelectedTile);
	if (source == remasterEditingProfile.assets.end() || !source->second.hasHeight || S9xRemasterHasGeometryVariants(source->second))
		return;
	const std::array<int16_t, 64> sourceHeight = source->second.height;
	const RemasterHeightSampling sourceSampling = source->second.heightSampling;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	bool changed = false;
	for (const RemasterTileContentId &tileId : remasterVariants)
	{
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		if (S9xRemasterHasGeometryVariants(metadata)) continue;
		changed |= !metadata.hasHeight || metadata.height != sourceHeight ||
			metadata.heightSampling != sourceSampling;
		metadata.hasHeight = true;
		metadata.height = sourceHeight;
		metadata.heightSampling = sourceSampling;
	}
	if (!changed)
		return;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 2);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Apply Height to Animation"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterNormalPreset:(id)sender
{
	static const float presets[6][3] = {
		{ 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f },
		{ 1.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f },
		{ 0.0f, 1.0f, 0.0f }, { 0.0f, -1.0f, 0.0f }
	};
	const NSInteger preset = remasterNormalPreset.indexOfSelectedItem;
	if (preset >= 0 && preset < 6)
	{
		remasterNormalXInput.floatValue = presets[preset][0];
		remasterNormalYInput.floatValue = presets[preset][1];
		remasterNormalZInput.floatValue = presets[preset][2];
		remasterNormalAxesView.normalX = presets[preset][0];
		remasterNormalAxesView.normalY = presets[preset][1];
		remasterNormalAxesView.normalZ = presets[preset][2];
		remasterNormalBackButton.state = presets[preset][2] < 0.0f ?
			NSControlStateValueOn : NSControlStateValueOff;
		[remasterNormalAxesView setNeedsDisplay:YES];
		[self changeRemasterNormalValue:sender];
	}
}

- (void)changeRemasterNormalValue:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty())
		return;
	if (!remasterTilePixelSelected)
	{
		if (sender == remasterNormalBackButton)
		{
			const float sourceZ = remasterNormalZInput.stringValue.length ?
				remasterNormalZInput.floatValue : remasterNormalAxesView.normalZ;
			const float z = std::fabs(sourceZ) *
				(remasterNormalBackButton.state == NSControlStateValueOn ? -1.0f : 1.0f);
			remasterNormalZInput.floatValue = z;
			remasterNormalAxesView.normalZ = z;
			[remasterNormalAxesView setNeedsDisplay:YES];
		}
		return;
	}
	[remasterInspectorPanel makeFirstResponder:nil];
	NSTextField *inputs[3] = { remasterNormalXInput, remasterNormalYInput, remasterNormalZInput };
	float inputValues[3] = {};
	bool setComponents[3] = {};
	const bool hemisphereOnly = sender == remasterNormalBackButton;
	for (size_t component = 0; component < 3 && !hemisphereOnly; component++)
	{
		NSString *text = [inputs[component].stringValue stringByTrimmingCharactersInSet:
			[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		if (text.length == 0 && remasterNormalComponentMixed[component])
			continue;
		NSScanner *scanner = [NSScanner scannerWithString:text];
		if (![scanner scanFloat:&inputValues[component]] || !scanner.isAtEnd || !std::isfinite(inputValues[component]))
		{
			NSBeep();
			remasterValueLabel.stringValue = @"Enter finite XYZ numbers, then click Update.";
			[remasterInspectorPanel makeFirstResponder:inputs[component]];
			return;
		}
		setComponents[component] = true;
	}
	if (!hemisphereOnly && !setComponents[0] && !setComponents[1] && !setComponents[2])
		return;
	// Validate every destination before changing any of them (mixed components may differ).
	std::vector<std::array<uint8_t, 3>> values;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		const std::array<uint8_t, 3> current = EffectiveRemasterNormal(tileId, remasterSelectedTilePixel);
		float components[3];
		DecodeRemasterNormal(current.data(), components[0], components[1], components[2]);
		for (size_t component = 0; component < 3; component++)
			if (setComponents[component])
				components[component] = inputValues[component];
		if (hemisphereOnly)
			components[2] = std::fabs(components[2]) *
				(remasterNormalBackButton.state == NSControlStateValueOn ? -1.0f : 1.0f);
		if (std::hypot(std::hypot(components[0], components[1]), components[2]) < 0.0001f)
		{
			NSBeep();
			remasterValueLabel.stringValue = @"Normal must have a nonzero direction.";
			return;
		}
		values.push_back(EncodeRemasterNormal(components[0], components[1], components[2]));
	}
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	bool changed = false;
	size_t index = 0;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		const std::array<uint8_t, 3> &value = values[index++];
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		const RemasterFrameAssetMetadata *captured = S9xRemasterFrameMetadataForTile(remasterReplayFrame, tileId);
		auto &g = RemasterEditingGeometry(metadata);
		const bool hadLayer = g.hasNormals;
		if (!hadLayer && remasterTileContextName.empty() && !S9xRemasterHasGeometryVariants(metadata) && captured && captured->hasNormals)
			g.normalXyz = captured->normalXyz;
		else if (!hadLayer)
			for (size_t pixel = 0; pixel < 64; pixel++)
			{
				g.normalXyz[pixel * 3] = 128;
				g.normalXyz[pixel * 3 + 1] = 128;
				g.normalXyz[pixel * 3 + 2] = 255;
			}
		const size_t offset = remasterSelectedTilePixel * 3;
		changed |= !hadLayer || !std::equal(value.begin(), value.end(), g.normalXyz.begin() + offset);
		std::copy(value.begin(), value.end(), g.normalXyz.begin() + offset);
		g.hasNormals = true;
	}
	if (!changed)
	{
		[self refreshRemasterEditingControls];
		return;
	}
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 5);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Change Surface Normal"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)fillRemasterTileNormal:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterSelectedTiles.empty())
		return;
	[remasterInspectorPanel makeFirstResponder:nil];
	NSTextField *inputs[3] = { remasterNormalXInput, remasterNormalYInput, remasterNormalZInput };
	float inputValues[3] = {};
	bool setComponents[3] = {};
	for (size_t component = 0; component < 3; component++)
	{
		NSString *text = [inputs[component].stringValue stringByTrimmingCharactersInSet:
			[NSCharacterSet whitespaceAndNewlineCharacterSet]];
		if (text.length == 0 && remasterNormalComponentMixed[component])
			continue;
		NSScanner *scanner = [NSScanner scannerWithString:text];
		if (![scanner scanFloat:&inputValues[component]] || !scanner.isAtEnd || !std::isfinite(inputValues[component]))
		{
			NSBeep();
			remasterValueLabel.stringValue = @"Enter finite XYZ numbers before filling.";
			[remasterInspectorPanel makeFirstResponder:inputs[component]];
			return;
		}
		setComponents[component] = true;
	}
	std::vector<std::array<uint8_t, 3>> values;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		const std::array<uint8_t, 3> current = EffectiveRemasterNormal(tileId,
			remasterTilePixelSelected ? remasterSelectedTilePixel : 0);
		float components[3];
		DecodeRemasterNormal(current.data(), components[0], components[1], components[2]);
		for (size_t component = 0; component < 3; component++)
			if (setComponents[component])
				components[component] = inputValues[component];
		if (std::hypot(std::hypot(components[0], components[1]), components[2]) < 0.0001f)
		{
			NSBeep();
			remasterValueLabel.stringValue = @"Normal must have a nonzero direction.";
			return;
		}
		values.push_back(EncodeRemasterNormal(components[0], components[1], components[2]));
	}
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	bool changed = false;
	size_t index = 0;
	for (const RemasterTileContentId &tileId : remasterSelectedTiles)
	{
		const std::array<uint8_t, 3> &value = values[index++];
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		auto &g = RemasterEditingGeometry(metadata);
		changed |= !g.hasNormals;
		for (size_t pixel = 0; pixel < 64; pixel++)
			for (size_t component = 0; component < 3; component++)
			{
				changed |= g.normalXyz[pixel * 3 + component] != value[component];
				g.normalXyz[pixel * 3 + component] = value[component];
			}
		g.hasNormals = true;
	}
	if (!changed)
		return;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 5);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Fill Tile Normal"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)applyRemasterNormalToAnimation:(id)sender
{
	if (!remasterEditingProfileLoaded || remasterVariants.size() < 2)
		return;
	auto source = remasterEditingProfile.assets.find(remasterSelectedTile);
	if (source == remasterEditingProfile.assets.end() || !source->second.hasNormals || S9xRemasterHasGeometryVariants(source->second))
		return;
	const std::array<uint8_t, 192> sourceNormals = source->second.normalXyz;
	std::string before;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	S9xRemasterSerializeProfile(remasterEditingProfile, before, diagnostics);
	bool changed = false;
	for (const RemasterTileContentId &tileId : remasterVariants)
	{
		RemasterAssetMetadata &metadata = remasterEditingProfile.assets[tileId];
		metadata.tileId = tileId;
		if (S9xRemasterHasGeometryVariants(metadata)) continue;
		changed |= !metadata.hasNormals || metadata.normalXyz != sourceNormals;
		metadata.hasNormals = true;
		metadata.normalXyz = sourceNormals;
	}
	if (!changed)
		return;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 5);
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:before.c_str()]];
	[remasterUndoManager setActionName:@"Apply Normals to Animation"];
	std::string current;
	if (S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		UpdateRemasterEditingProfileDirty(current);
	[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)saveRemasterProfile:(id)sender
{
	[self writeRemasterProfile];
}

- (BOOL)writeRemasterProfile
{
	if (![self commitRemasterHeightScale] || ![self commitRemasterCameraDirection] ||
		![self commitRemasterIndirectRoughness] ||
		![self commitRemasterOriginalSceneContribution] ||
		![self commitRemasterReflectanceBoost])
		return NO;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!remasterEditingProfileLoaded || !remasterEditingProfileURL) return NO;
	NSData *diskBytes = [NSData dataWithContentsOfURL:remasterEditingProfileURL];
	const bool rebasing = ![diskBytes isEqualToData:remasterProfileDiskSnapshot] || !remasterPendingProfileConflicts.empty();
	RemasterProfileMergeResult merged;
	bool ready = false, keepPending = false;
	if (!diskBytes || !remasterProfileDiskSnapshot)
		diagnostics = {{0, "Unable to read the current profile or its loaded baseline. Reload the profile before saving."}};
	else
	{
		RemasterProfile base, disk;
		const std::string baseText(static_cast<const char *>(remasterProfileDiskSnapshot.bytes), remasterProfileDiskSnapshot.length);
		const std::string diskText(static_cast<const char *>(diskBytes.bytes), diskBytes.length);
		ready = S9xRemasterParseProfile(baseText, base, diagnostics) && S9xRemasterParseProfile(diskText, disk, diagnostics) &&
			S9xRemasterMergeProfiles(base, remasterEditingProfile, disk, remasterPendingProfileConflicts, false, merged, diagnostics, true);
		if (ready && !merged.conflicts.empty())
		{
			std::ostringstream details;
			details << merged.conflicts.size() << " profile record(s) changed in both this session and on disk. Changes to other records will be merged.\n\n";
			size_t shown = 0;
			for (const auto &key : merged.conflicts)
			{
				if (shown++ == 12) { details << "…\n"; break; }
				details << RemasterProfileMerge::Label(key) << '\n';
			}
			details << "\nKeeping disk changes leaves your conflicting edits pending in this session. Save again to overwrite, or reload to discard them.";
			NSAlert *alert = [NSAlert new];
			alert.messageText = @"Resolve Profile Save Conflicts";
			alert.informativeText = [NSString stringWithUTF8String:details.str().c_str()];
			[alert addButtonWithTitle:@"Overwrite with changes from this session"];
			[alert addButtonWithTitle:@"Keep changes currently on disk"];
			[alert addButtonWithTitle:@"Cancel"];
			const NSModalResponse choice = [alert runModal];
			if (choice == NSAlertThirdButtonReturn) return NO;
			keepPending = choice == NSAlertSecondButtonReturn;
		}
		if (ready)
			ready = S9xRemasterMergeProfiles(base, remasterEditingProfile, disk, remasterPendingProfileConflicts, !keepPending, merged, diagnostics);
	}
	std::string savedText;
	if (ready) ready = S9xRemasterSerializeProfile(merged.saved, savedText, diagnostics);
	// A dialog may stay open while another writer changes the file. Never use
	// its old disk version after that happens; the next Save recomputes the merge.
	if (ready && ![diskBytes isEqualToData:[NSData dataWithContentsOfURL:remasterEditingProfileURL]])
	{
		ready = false;
		diagnostics = {{0, "The profile changed again while saving. Your session edits are intact; click Save Profile again to merge the latest version."}};
	}
	if (ready && S9xRemasterWriteProfile(merged.saved, remasterEditingProfileURL.path.UTF8String, diagnostics))
	{
		remasterProfileDiskSnapshot = [NSData dataWithBytes:savedText.data() length:savedText.size()];
		remasterEditingProfile = std::move(merged.session);
		remasterPendingProfileConflicts = keepPending ? merged.conflicts : std::set<std::string>{};
		RegisterRemasterSemanticProfile();
		remasterSavedProfileText = std::move(savedText);
		remasterSavedSceneSettings = S9xRemasterGetSceneSettings(merged.saved);
		remasterSavedAreaLights = merged.saved.areaLights;
		std::string currentText;
		S9xRemasterSerializeProfile(remasterEditingProfile, currentText, diagnostics, false);
		UpdateRemasterEditingProfileDirty(currentText);
		remasterEditingProfileDirty |= !remasterPendingProfileConflicts.empty();
		remasterAreaNeedsRefresh = true;
		[self refreshRemasterAreaSettings];
		[self refreshRemasterProfileSceneControls];
		// Whole-profile undo snapshots predate the merge and could otherwise
		// resurrect stale copies of unrelated disk records after this save.
		if (rebasing) [remasterUndoManager removeAllActions];
		if (remasterSettingsSaveButton)
			remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;
		remasterNpcSaveButton.enabled = remasterEditingProfileDirty;
		if (running)
			S9xRemasterSetProfile(remasterEditingProfile);
		if (!remasterVariants.empty())
			[self showRemasterVariantAtIndex:remasterVariantIndex];
		// A close/load confirmation must not discard conflicts kept in memory.
		return remasterPendingProfileConflicts.empty();
	}
	std::ostringstream message;
	for (const RemasterProfileDiagnostic &diagnostic : diagnostics)
		message << (diagnostic.line ? "Line " + std::to_string(diagnostic.line) + ": " : "") << diagnostic.message << '\n';
	NSAlert *alert = [NSAlert new];
	alert.messageText = @"Unable to Save Remaster Profile";
	alert.informativeText = [NSString stringWithUTF8String:message.str().c_str()];
	[alert runModal];
	return NO;
}

- (void)restoreRemasterProfileFromText:(NSString *)text
{
	std::string current;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterSerializeProfile(remasterEditingProfile, current, diagnostics))
		return;
	RemasterProfile restored;
	diagnostics.clear();
	if (!S9xRemasterParseProfile(text.UTF8String, restored, diagnostics))
		return;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterProfileFromText:)
		object:[NSString stringWithUTF8String:current.c_str()]];
	remasterEditingProfile = std::move(restored);
	UpdateRemasterEditingProfileDirty(std::string(text.UTF8String));
	remasterAreaNeedsRefresh = true;
	SyncRemasterEditingMetadataToFrame();
	[self refreshRemasterProfileSceneControls];
	if (running)
		S9xRemasterSetProfile(remasterEditingProfile);
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
	else
		[self refreshRemasterEditingControls];
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
			remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled, remasterLightingView);
}

- (void)refreshRemasterProfileSceneControls
{
	if (remasterBounceSlider)
	{
		remasterBounceSlider.integerValue = remasterEditingProfile.indirectBounceCount;
		remasterBounceInput.integerValue = remasterEditingProfile.indirectBounceCount;
		remasterHeightScaleInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.lightingCoordinateScale];
		for (size_t component = 0; component < 3; component++)
			remasterCameraDirectionInputs[component].stringValue = [NSString stringWithFormat:@"%.6g",
				remasterEditingProfile.cameraDirection[component]];
		remasterIndirectRoughnessSlider.floatValue = remasterEditingProfile.indirectRoughness;
		remasterIndirectRoughnessInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.indirectRoughness];
		remasterOriginalSceneSlider.floatValue = remasterEditingProfile.originalSceneContribution;
		remasterOriginalSceneInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.originalSceneContribution];
		remasterReflectanceBoostSlider.floatValue = remasterEditingProfile.reflectanceBoost;
		remasterReflectanceBoostInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.reflectanceBoost];
		remasterHeightPreviewMultiplierSlider.integerValue = remasterEditingProfile.heightPreviewMultiplier;
		remasterHeightPreviewMultiplierInput.integerValue = remasterEditingProfile.heightPreviewMultiplier;
		remasterSamplesSlider.integerValue = remasterEditingProfile.samplesPerFrame;
		remasterSamplesInput.integerValue = remasterEditingProfile.samplesPerFrame;
		remasterEmitterPatchSlider.integerValue = remasterEditingProfile.emitterPatchSize;
		remasterEmitterPatchInput.integerValue = remasterEditingProfile.emitterPatchSize;
		[self refreshRemasterRepresentativeControls];
		remasterSampleAccumulationButton.state = remasterEditingProfile.sampleAccumulation ? NSControlStateValueOn : NSControlStateValueOff;
		remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;
	}
}

- (BOOL)confirmDiscardingRemasterChanges
{
	if (!remasterEditingProfileDirty)
		return YES;
	NSAlert *alert = [NSAlert new];
	alert.messageText = @"Save changes to the remaster profile?";
	alert.informativeText = @"Your unsaved profile edits will be lost if you discard them.";
	[alert addButtonWithTitle:@"Save"];
	[alert addButtonWithTitle:@"Cancel"];
	[alert addButtonWithTitle:@"Discard"];
	const NSModalResponse response = [alert runModal];
	if (response == NSAlertFirstButtonReturn)
		return [self writeRemasterProfile];
	if (response == NSAlertSecondButtonReturn)
		return NO;
	RemasterProfile restored;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterParseProfile(remasterSavedProfileText, restored, diagnostics))
		return NO;
	remasterEditingProfile = std::move(restored);
	remasterNonSceneDirty = false;
	remasterPendingProfileConflicts.clear();
	remasterAreaNeedsRefresh = true;
	remasterEditingProfileDirty = false;
	[remasterUndoManager removeAllActions];
	return YES;
}

- (NSUndoManager *)windowWillReturnUndoManager:(NSWindow *)window
{
	return window == remasterInspectorPanel || window == remasterProfileSettingsPanel ? remasterUndoManager : nil;
}

- (void)showRemasterProfileSettings
{
	if (!remasterEditingProfileLoaded)
		return;
	workspaceSceneOpen = YES;
	[self savePreviousWorkspace];
	if (!remasterProfileSettingsPanel)
	{
		remasterProfileSettingsPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 430, 1124)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
			backing:NSBackingStoreBuffered defer:NO];
		remasterProfileSettingsPanel.title = @"Remaster Scene Controls";
		remasterProfileSettingsPanel.releasedWhenClosed = NO;
		remasterProfileSettingsPanel.hidesOnDeactivate = YES;
		remasterProfileSettingsPanel.delegate = self;
		NSView *content = remasterProfileSettingsPanel.contentView;
		NSTextField *reflectanceTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 930, 300, 24)];
		reflectanceTitle.stringValue = @"Reflected Light Boost (0–8)";
		reflectanceTitle.editable = NO;
		reflectanceTitle.bezeled = NO;
		reflectanceTitle.drawsBackground = NO;
		[content addSubview:reflectanceTitle];
		remasterReflectanceBoostSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 897, 300, 24)];
		remasterReflectanceBoostSlider.minValue = 0;
		remasterReflectanceBoostSlider.maxValue = 8;
		remasterReflectanceBoostSlider.continuous = NO;
		remasterReflectanceBoostSlider.target = self;
		remasterReflectanceBoostSlider.action = @selector(changeRemasterReflectanceBoost:);
		[content addSubview:remasterReflectanceBoostSlider];
		remasterReflectanceBoostInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 897, 60, 24)];
		remasterReflectanceBoostInput.alignment = NSTextAlignmentCenter;
		remasterReflectanceBoostInput.delegate = self;
		remasterReflectanceBoostInput.target = self;
		remasterReflectanceBoostInput.action = @selector(changeRemasterReflectanceBoost:);
		[content addSubview:remasterReflectanceBoostInput];
		NSTextField *reflectanceNote = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 862, 390, 24)];
		reflectanceNote.stringValue = @"Lifts dark colors more; each bounce reflects less than it receives.";
		reflectanceNote.editable = NO;
		reflectanceNote.bezeled = NO;
		reflectanceNote.drawsBackground = NO;
		reflectanceNote.font = [NSFont systemFontOfSize:11];
		[content addSubview:reflectanceNote];
		NSTextField *heightPreviewTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 825, 220, 24)];
		heightPreviewTitle.stringValue = @"Height Preview Multiplier";
		heightPreviewTitle.editable = NO;
		heightPreviewTitle.bezeled = NO;
		heightPreviewTitle.drawsBackground = NO;
		[content addSubview:heightPreviewTitle];
		remasterHeightPreviewMultiplierSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 792, 300, 24)];
		remasterHeightPreviewMultiplierSlider.minValue = 1;
		remasterHeightPreviewMultiplierSlider.maxValue = 20;
		remasterHeightPreviewMultiplierSlider.numberOfTickMarks = 20;
		remasterHeightPreviewMultiplierSlider.allowsTickMarkValuesOnly = YES;
		remasterHeightPreviewMultiplierSlider.continuous = NO;
		remasterHeightPreviewMultiplierSlider.target = self;
		remasterHeightPreviewMultiplierSlider.action = @selector(changeRemasterHeightPreviewMultiplier:);
		[content addSubview:remasterHeightPreviewMultiplierSlider];
		remasterHeightPreviewMultiplierInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 792, 60, 24)];
		remasterHeightPreviewMultiplierInput.alignment = NSTextAlignmentCenter;
		remasterHeightPreviewMultiplierInput.target = self;
		remasterHeightPreviewMultiplierInput.action = @selector(changeRemasterHeightPreviewMultiplier:);
		[content addSubview:remasterHeightPreviewMultiplierInput];
		NSTextField *heightMinTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 749, 110, 24)];
		heightMinTitle.stringValue = @"Height range min";
		heightMinTitle.editable = NO;
		heightMinTitle.bezeled = NO;
		heightMinTitle.drawsBackground = NO;
		[content addSubview:heightMinTitle];
		remasterHeightPreviewMinInput = [[NSTextField alloc] initWithFrame:NSMakeRect(135, 749, 55, 24)];
		remasterHeightPreviewMinInput.alignment = NSTextAlignmentCenter;
		remasterHeightPreviewMinInput.delegate = self;
		remasterHeightPreviewMinInput.target = self;
		remasterHeightPreviewMinInput.action = @selector(changeRemasterHeightPreviewRange:);
		[content addSubview:remasterHeightPreviewMinInput];
		NSTextField *heightMaxTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(220, 749, 110, 24)];
		heightMaxTitle.stringValue = @"Height range max";
		heightMaxTitle.editable = NO;
		heightMaxTitle.bezeled = NO;
		heightMaxTitle.drawsBackground = NO;
		[content addSubview:heightMaxTitle];
		remasterHeightPreviewMaxInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 749, 60, 24)];
		remasterHeightPreviewMaxInput.alignment = NSTextAlignmentCenter;
		remasterHeightPreviewMaxInput.delegate = self;
		remasterHeightPreviewMaxInput.target = self;
		remasterHeightPreviewMaxInput.action = @selector(changeRemasterHeightPreviewRange:);
		[content addSubview:remasterHeightPreviewMaxInput];
		NSTextField *originalTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 690, 220, 24)];
		originalTitle.stringValue = @"Original Scene RGB Contribution";
		originalTitle.editable = NO;
		originalTitle.bezeled = NO;
		originalTitle.drawsBackground = NO;
		[content addSubview:originalTitle];
		remasterOriginalSceneSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 657, 300, 24)];
		remasterOriginalSceneSlider.minValue = 0;
		remasterOriginalSceneSlider.maxValue = 1;
		remasterOriginalSceneSlider.continuous = NO;
		remasterOriginalSceneSlider.target = self;
		remasterOriginalSceneSlider.action = @selector(changeRemasterOriginalSceneContribution:);
		[content addSubview:remasterOriginalSceneSlider];
		remasterOriginalSceneInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 657, 60, 24)];
		remasterOriginalSceneInput.alignment = NSTextAlignmentCenter;
		remasterOriginalSceneInput.delegate = self;
		remasterOriginalSceneInput.target = self;
		remasterOriginalSceneInput.action = @selector(changeRemasterOriginalSceneContribution:);
		[content addSubview:remasterOriginalSceneInput];
		NSTextField *samplesTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 616, 300, 24)];
		samplesTitle.stringValue = @"Connections Per Receiver (1–128)";
		samplesTitle.editable = NO;
		samplesTitle.bezeled = NO;
		samplesTitle.drawsBackground = NO;
		[content addSubview:samplesTitle];
		remasterSamplesSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 583, 300, 24)];
		remasterSamplesSlider.minValue = 1;
		remasterSamplesSlider.maxValue = 128;
		remasterSamplesSlider.numberOfTickMarks = 0;
		remasterSamplesSlider.allowsTickMarkValuesOnly = NO;
		remasterSamplesSlider.continuous = NO;
		remasterSamplesSlider.target = self;
		remasterSamplesSlider.action = @selector(changeRemasterSamplesPerFrame:);
		[content addSubview:remasterSamplesSlider];
		remasterSamplesInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 583, 60, 24)];
		remasterSamplesInput.alignment = NSTextAlignmentCenter;
		remasterSamplesInput.target = self;
		remasterSamplesInput.action = @selector(changeRemasterSamplesPerFrame:);
		[content addSubview:remasterSamplesInput];
		NSTextField *sampleNote = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 546, 390, 24)];
		sampleNote.stringValue = @"Emitter Patch Size (1–5; maximum N×N)";
		sampleNote.editable = NO;
		sampleNote.bezeled = NO;
		sampleNote.drawsBackground = NO;
		[content addSubview:sampleNote];
		remasterEmitterPatchSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 522, 300, 24)];
		remasterEmitterPatchSlider.minValue = 1;
		remasterEmitterPatchSlider.maxValue = 5;
		remasterEmitterPatchSlider.numberOfTickMarks = 5;
		remasterEmitterPatchSlider.allowsTickMarkValuesOnly = YES;
		remasterEmitterPatchSlider.continuous = NO;
		remasterEmitterPatchSlider.target = self;
		remasterEmitterPatchSlider.action = @selector(changeRemasterEmitterPatchSize:);
		[content addSubview:remasterEmitterPatchSlider];
		remasterEmitterPatchInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 522, 60, 24)];
		remasterEmitterPatchInput.target = self;
		remasterEmitterPatchInput.action = @selector(changeRemasterEmitterPatchSize:);
		[content addSubview:remasterEmitterPatchInput];
		// Make room between patch size and bounce controls without compressing rows.
		for (NSView *view in content.subviews) if (view.frame.origin.y >= 522) {
			NSRect rect = view.frame; rect.origin.y += 64; view.frame = rect;
		}
		remasterRepresentativeEnabled = [[NSButton alloc] initWithFrame:NSMakeRect(20, 550, 390, 24)];
		[remasterRepresentativeEnabled setButtonType:NSButtonTypeSwitch];
		remasterRepresentativeEnabled.title = @"Representative Emitters per Tile (1–8; approximate)";
		remasterRepresentativeEnabled.target = self;
		remasterRepresentativeEnabled.action = @selector(changeRemasterRepresentativeEmitters:);
		[content addSubview:remasterRepresentativeEnabled];
		remasterRepresentativeSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 522, 300, 24)];
		remasterRepresentativeSlider.minValue = 1;
		remasterRepresentativeSlider.maxValue = 8;
		remasterRepresentativeSlider.numberOfTickMarks = 8;
		remasterRepresentativeSlider.allowsTickMarkValuesOnly = YES;
		remasterRepresentativeSlider.continuous = NO;
		remasterRepresentativeSlider.target = self;
		remasterRepresentativeSlider.action = @selector(changeRemasterRepresentativeEmitters:);
		[content addSubview:remasterRepresentativeSlider];
		remasterRepresentativeInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 522, 60, 24)];
		remasterRepresentativeInput.alignment = NSTextAlignmentCenter;
		remasterRepresentativeInput.target = self;
		remasterRepresentativeInput.action = @selector(changeRemasterRepresentativeEmitters:);
		[content addSubview:remasterRepresentativeInput];
		NSTextField *title = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 498, 180, 24)];
		title.stringValue = @"Indirect Light Bounces";
		title.editable = NO;
		title.bezeled = NO;
		title.drawsBackground = NO;
		[content addSubview:title];
		remasterBounceSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 477, 300, 24)];
		remasterBounceSlider.minValue = 0;
		remasterBounceSlider.maxValue = 16;
		remasterBounceSlider.numberOfTickMarks = 17;
		remasterBounceSlider.allowsTickMarkValuesOnly = YES;
		remasterBounceSlider.continuous = NO;
		remasterBounceSlider.target = self;
		remasterBounceSlider.action = @selector(changeRemasterBounceCount:);
		[content addSubview:remasterBounceSlider];
		remasterBounceInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 477, 60, 24)];
		remasterBounceInput.alignment = NSTextAlignmentCenter;
		remasterBounceInput.target = self;
		remasterBounceInput.action = @selector(changeRemasterBounceCount:);
		[content addSubview:remasterBounceInput];
		NSTextField *roughnessTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 406, 260, 24)];
		roughnessTitle.stringValue = @"Surface Roughness (future)";
		roughnessTitle.editable = NO;
		roughnessTitle.bezeled = NO;
		roughnessTitle.drawsBackground = NO;
		[content addSubview:roughnessTitle];
		remasterIndirectRoughnessSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 373, 300, 24)];
		remasterIndirectRoughnessSlider.minValue = 0;
		remasterIndirectRoughnessSlider.maxValue = 1;
		remasterIndirectRoughnessSlider.continuous = NO;
		remasterIndirectRoughnessSlider.target = self;
		remasterIndirectRoughnessSlider.action = @selector(changeRemasterIndirectRoughness:);
		remasterIndirectRoughnessSlider.enabled = NO;
		[content addSubview:remasterIndirectRoughnessSlider];
		remasterIndirectRoughnessInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 373, 60, 24)];
		remasterIndirectRoughnessInput.alignment = NSTextAlignmentCenter;
		remasterIndirectRoughnessInput.delegate = self;
		remasterIndirectRoughnessInput.target = self;
		remasterIndirectRoughnessInput.action = @selector(changeRemasterIndirectRoughness:);
		remasterIndirectRoughnessInput.enabled = NO;
		[content addSubview:remasterIndirectRoughnessInput];
		NSTextField *heightScaleTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 342, 180, 24)];
		heightScaleTitle.stringValue = @"Height Scale";
		heightScaleTitle.editable = NO;
		heightScaleTitle.bezeled = NO;
		heightScaleTitle.drawsBackground = NO;
		[content addSubview:heightScaleTitle];
		remasterHeightScaleInput = [[NSTextField alloc] initWithFrame:NSMakeRect(335, 342, 60, 24)];
		remasterHeightScaleInput.alignment = NSTextAlignmentCenter;
		remasterHeightScaleInput.delegate = self;
		remasterHeightScaleInput.target = self;
		remasterHeightScaleInput.action = @selector(changeRemasterHeightScale:);
		[content addSubview:remasterHeightScaleInput];
		NSTextField *heightScaleDescription = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 306, 300, 30)];
		heightScaleDescription.stringValue = @"Maps height 255 to this many screen-space Z units.";
		heightScaleDescription.editable = NO;
		heightScaleDescription.bezeled = NO;
		heightScaleDescription.drawsBackground = NO;
		heightScaleDescription.font = [NSFont systemFontOfSize:11];
		[content addSubview:heightScaleDescription];
		remasterBounceDescription = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 438, 390, 26)];
		remasterBounceDescription.stringValue = @"Start with one bounce; each additional bounce adds GPU work.";
		remasterBounceDescription.editable = NO;
		remasterBounceDescription.bezeled = NO;
		remasterBounceDescription.drawsBackground = NO;
		remasterBounceDescription.font = [NSFont systemFontOfSize:11];
		[content addSubview:remasterBounceDescription];
		NSTextField *cameraDirectionTitle = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 270, 180, 24)];
		cameraDirectionTitle.stringValue = @"Camera Direction (future)";
		cameraDirectionTitle.editable = NO;
		cameraDirectionTitle.bezeled = NO;
		cameraDirectionTitle.drawsBackground = NO;
		[content addSubview:cameraDirectionTitle];
		NSArray<NSString *> *componentLabels = @[ @"X", @"Y", @"Z" ];
		for (NSInteger component = 0; component < 3; component++)
		{
			const CGFloat x = 20 + component * 125;
			NSTextField *label = [[NSTextField alloc] initWithFrame:NSMakeRect(x, 236, 20, 24)];
			label.stringValue = componentLabels[component];
			label.alignment = NSTextAlignmentRight;
			label.editable = NO;
			label.bezeled = NO;
			label.drawsBackground = NO;
			[content addSubview:label];
			remasterCameraDirectionInputs[component] = [[NSTextField alloc] initWithFrame:NSMakeRect(x + 25, 236, 85, 24)];
			remasterCameraDirectionInputs[component].alignment = NSTextAlignmentCenter;
			remasterCameraDirectionInputs[component].delegate = self;
			remasterCameraDirectionInputs[component].target = self;
			remasterCameraDirectionInputs[component].action = @selector(changeRemasterCameraDirection:);
			remasterCameraDirectionInputs[component].enabled = NO;
			[content addSubview:remasterCameraDirectionInputs[component]];
		}
		remasterMetricsButton = [[NSButton alloc] initWithFrame:NSMakeRect(20, 196, 250, 24)];
		remasterMetricsButton.buttonType = NSButtonTypeSwitch;
		remasterMetricsButton.title = @"Gather live performance metrics";
		remasterMetricsButton.target = self;
		remasterMetricsButton.action = @selector(changeRemasterMetricsEnabled:);
		[content addSubview:remasterMetricsButton];
		remasterMetricsText = [[NSTextField alloc] initWithFrame:NSMakeRect(20, 43, 390, 165)];
		remasterMetricsText.editable = NO;
		remasterMetricsText.selectable = YES;
		remasterMetricsText.bezeled = NO;
		remasterMetricsText.drawsBackground = NO;
		remasterMetricsText.font = [NSFont monospacedDigitSystemFontOfSize:10 weight:NSFontWeightRegular];
		remasterMetricsText.alignment = NSTextAlignmentLeft;
		remasterMetricsText.stringValue = @"Metrics are disabled.";
		[content addSubview:remasterMetricsText];
		remasterSettingsSaveButton = [[NSButton alloc] initWithFrame:NSMakeRect(300, 12, 110, 30)];
		remasterSettingsSaveButton.title = @"Save Profile";
		remasterSettingsSaveButton.bezelStyle = NSBezelStyleRounded;
		remasterSettingsSaveButton.target = self;
		remasterSettingsSaveButton.action = @selector(saveRemasterProfile:);
		[content addSubview:remasterSettingsSaveButton];
		// Reserve enough height for all metrics lines, including separate
		// directional timing, without drawing over the metrics checkbox.
		for (NSView *view in content.subviews)
		{
			NSRect rect = view.frame;
			if (view == remasterMetricsText) rect.size.height += 80;
			else if (view != remasterSettingsSaveButton) rect.origin.y += 80;
			view.frame = rect;
		}
		S9xInitializeWindowPlacement(remasterProfileSettingsPanel, @"RemasterSceneControls");
	}
	remasterBounceSlider.integerValue = remasterEditingProfile.indirectBounceCount;
	remasterBounceInput.integerValue = remasterEditingProfile.indirectBounceCount;
	remasterHeightScaleInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterEditingProfile.lightingCoordinateScale];
	for (size_t component = 0; component < 3; component++)
		remasterCameraDirectionInputs[component].stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.cameraDirection[component]];
	remasterIndirectRoughnessSlider.floatValue = remasterEditingProfile.indirectRoughness;
	remasterIndirectRoughnessInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterEditingProfile.indirectRoughness];
	remasterOriginalSceneSlider.floatValue = remasterEditingProfile.originalSceneContribution;
	remasterOriginalSceneInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterEditingProfile.originalSceneContribution];
	remasterReflectanceBoostSlider.floatValue = remasterEditingProfile.reflectanceBoost;
	remasterReflectanceBoostInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterEditingProfile.reflectanceBoost];
	remasterHeightPreviewMultiplierSlider.integerValue = remasterEditingProfile.heightPreviewMultiplier;
	remasterHeightPreviewMultiplierInput.integerValue = remasterEditingProfile.heightPreviewMultiplier;
	remasterHeightPreviewMinInput.integerValue = remasterHeightPreviewMin;
	remasterHeightPreviewMaxInput.integerValue = remasterHeightPreviewMax;
	remasterSamplesSlider.integerValue = remasterEditingProfile.samplesPerFrame;
	remasterSamplesInput.integerValue = remasterEditingProfile.samplesPerFrame;
	remasterEmitterPatchSlider.integerValue = remasterEditingProfile.emitterPatchSize;
	remasterEmitterPatchInput.integerValue = remasterEditingProfile.emitterPatchSize;
	[self refreshRemasterRepresentativeControls];
	remasterSampleAccumulationButton.state = remasterEditingProfile.sampleAccumulation ? NSControlStateValueOn : NSControlStateValueOff;
	remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;
	remasterMetricsButton.state = S9xRemasterPerformanceMetricsEnabled() ? NSControlStateValueOn : NSControlStateValueOff;
	[self refreshRemasterMetrics];
	if (!remasterMetricsTimer)
		remasterMetricsTimer = [NSTimer scheduledTimerWithTimeInterval:0.25 target:self
			selector:@selector(refreshRemasterMetrics) userInfo:nil repeats:YES];
	[remasterProfileSettingsPanel orderFront:nil];
}

- (void)showRemasterAreaSettings
{
	if (!remasterEditingProfileLoaded) return;
	if (!remasterAreaPanel)
	{
		remasterAreaPanel = [[NSPanel alloc] initWithContentRect:NSMakeRect(0, 0, 410, 400)
			styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable backing:NSBackingStoreBuffered defer:NO];
		remasterAreaPanel.title = @"Area Settings";
		remasterAreaPanel.releasedWhenClosed = NO;
		remasterAreaPanel.hidesOnDeactivate = YES;
		NSView *content = remasterAreaPanel.contentView;
		auto label = [&](NSString *text, NSRect rect) {
			NSTextField *field = [[NSTextField alloc] initWithFrame:rect];
			field.stringValue = text; field.editable = NO; field.selectable = NO;
			field.bezeled = NO; field.drawsBackground = NO; [content addSubview:field]; return field;
		};
		remasterAreaName = label(@"No active area", NSMakeRect(20, 356, 370, 24));
		remasterAreaName.font = [NSFont boldSystemFontOfSize:15];
		remasterAreaEnabled = [[NSButton alloc] initWithFrame:NSMakeRect(20, 316, 350, 24)];
		[remasterAreaEnabled setButtonType:NSSwitchButton]; remasterAreaEnabled.title = @"Enable directional light";
		remasterAreaEnabled.target = self; remasterAreaEnabled.action = @selector(changeRemasterAreaSettings:);
		[content addSubview:remasterAreaEnabled];
		remasterAreaAxes = [[S9xRemasterTileView alloc] initWithFrame:NSMakeRect(20, 208, 90, 100)];
		remasterAreaAxes.imageFrameStyle = NSImageFrameNone;
		remasterAreaAxes.showsNormalAxes = YES; remasterAreaAxes.areaDirection = YES;
		remasterAreaAxes.editor = self;
		remasterAreaAxes.toolTip = @"Drag: center = Z, rim = XY, between = XYZ tilt. Release to apply. Back (-Z) selects rays into the screen.";
		[content addSubview:remasterAreaAxes];
		remasterAreaBack = [[NSButton alloc] initWithFrame:NSMakeRect(125, 224, 180, 24)];
		[remasterAreaBack setButtonType:NSSwitchButton]; remasterAreaBack.title = @"Back (-Z)";
		remasterAreaBack.target = self; remasterAreaBack.action = @selector(changeRemasterAreaSettings:);
		[content addSubview:remasterAreaBack];
		label(@"Ray direction X / Y / Z", NSMakeRect(125, 278, 260, 22));
		for (int i = 0; i < 3; ++i)
		{
			remasterAreaDirection[i] = [[NSTextField alloc] initWithFrame:NSMakeRect(125 + i*85, 252, 78, 24)];
			remasterAreaDirection[i].target = self; remasterAreaDirection[i].action = @selector(changeRemasterAreaSettings:);
			remasterAreaDirection[i].delegate = self;
			[content addSubview:remasterAreaDirection[i]];
		}
		label(@"−Z points into the screen. +Y points down.", NSMakeRect(20, 150, 370, 22)).font = [NSFont systemFontOfSize:11];
		label(@"Brightness (0–100)", NSMakeRect(20, 162, 165, 22));
		remasterAreaBrightnessSlider = [[NSSlider alloc] initWithFrame:NSMakeRect(20, 120, 260, 24)];
		remasterAreaBrightnessSlider.minValue = 0; remasterAreaBrightnessSlider.maxValue = 100;
		remasterAreaBrightnessSlider.continuous = YES; remasterAreaBrightnessSlider.target = self;
		remasterAreaBrightnessSlider.action = @selector(changeRemasterAreaSettings:);
		[content addSubview:remasterAreaBrightnessSlider];
		remasterAreaBrightness = [[NSTextField alloc] initWithFrame:NSMakeRect(300, 120, 80, 24)];
		remasterAreaBrightness.target = self; remasterAreaBrightness.action = @selector(changeRemasterAreaSettings:);
		remasterAreaBrightness.delegate = self;
		[content addSubview:remasterAreaBrightness];
		label(@"Color", NSMakeRect(20, 72, 165, 22));
		remasterAreaColor = [[NSColorWell alloc] initWithFrame:NSMakeRect(190, 66, 90, 30)];
		remasterAreaColor.target = self; remasterAreaColor.action = @selector(changeRemasterAreaSettings:);
		[content addSubview:remasterAreaColor];
		remasterAreaSave = [[NSButton alloc] initWithFrame:NSMakeRect(280, 15, 110, 30)];
		remasterAreaSave.title = @"Save"; remasterAreaSave.bezelStyle = NSBezelStyleRounded;
		remasterAreaSave.target = self; remasterAreaSave.action = @selector(saveRemasterAreaSettings:);
		[content addSubview:remasterAreaSave];
		S9xInitializeWindowPlacement(remasterAreaPanel, @"RemasterAreaSettings");
	}
	[self refreshRemasterAreaSettings];
	if (!remasterAreaTimer) remasterAreaTimer = [NSTimer scheduledTimerWithTimeInterval:0.1 target:self
		selector:@selector(refreshRemasterAreaSettings) userInfo:nil repeats:YES];
	[remasterAreaPanel orderFront:nil];
}

- (void)refreshRemasterAreaSettings
{
	if (!remasterAreaPanel) return;
	const auto area = !remasterEditingProfileLoaded ? RemasterAreaContext{} :
		remasterFramePresenting ? remasterReplayFrame.area : S9xRemasterCurrentArea();
	// Preserve in-progress text while the player stays in the same area.
	const bool changed = area.id != remasterDisplayedArea.id || area.romDark != remasterDisplayedArea.romDark;
	if (changed && remasterDisplayedArea.valid() && !remasterAreaNeedsRefresh)
	{
		[remasterAreaPanel makeFirstResponder:nil];
		[self changeRemasterAreaSettings:nil];
	}
	remasterDisplayedArea = area;
	remasterAreaName.stringValue = area.id == 320 ? @"Light World" : area.id == 321 ? @"Dark World" :
		area.valid() ? [NSString stringWithFormat:@"Room 0x%03X", area.id] : @"No active area";
	const auto light = S9xRemasterAreaLight(remasterEditingProfile, area);
	if (changed || remasterAreaNeedsRefresh || !remasterAreaPanel.visible)
	{
		remasterAreaNeedsRefresh = false;
		remasterAreaEnabled.state = light.enabled ? NSControlStateValueOn : NSControlStateValueOff;
		for (int i = 0; i < 3; ++i) remasterAreaDirection[i].stringValue = [NSString stringWithFormat:@"%.9g", light.direction[i]];
		remasterAreaBrightness.stringValue = [NSString stringWithFormat:@"%.9g", S9xRemasterDirectionalBrightnessToUi(light.brightness)];
		remasterAreaBrightnessSlider.floatValue = S9xRemasterDirectionalBrightnessToUi(light.brightness);
		const float length = std::sqrt(light.direction[0]*light.direction[0] + light.direction[1]*light.direction[1] + light.direction[2]*light.direction[2]);
		remasterAreaAxes.normalX = light.direction[0]/length;
		remasterAreaAxes.normalY = light.direction[1]/length;
		remasterAreaAxes.normalZ = light.direction[2]/length;
		remasterAreaBack.state = light.direction[2] <= 0 ? NSControlStateValueOn : NSControlStateValueOff;
		[remasterAreaAxes setNeedsDisplay:YES];
		CGFloat components[] = {light.color[0], light.color[1], light.color[2], 1};
		remasterAreaColor.color = [NSColor colorWithColorSpace:[NSColorSpace genericRGBColorSpace]
			components:components count:4];
	}
	remasterAreaEnabled.enabled = area.valid();
	for (auto field : remasterAreaDirection) field.enabled = area.valid();
	remasterAreaBrightness.enabled = area.valid(); remasterAreaColor.enabled = area.valid();
	remasterAreaAxes.enabled = area.valid(); remasterAreaBack.enabled = area.valid();
	remasterAreaBrightnessSlider.enabled = area.valid();
	remasterAreaSave.enabled = remasterEditingProfileLoaded && RemasterAreaDirty();
}

- (BOOL)changeRemasterAreaSettings:(id)sender
{
	if (!remasterEditingProfileLoaded || !remasterDisplayedArea.valid()) return NO;
	if (sender == remasterAreaBack)
		remasterAreaDirection[2].floatValue = std::abs(remasterAreaDirection[2].floatValue) *
			(remasterAreaBack.state == NSControlStateValueOn ? -1 : 1);
	if (sender == remasterAreaBrightnessSlider) remasterAreaBrightness.floatValue = remasterAreaBrightnessSlider.floatValue;
	RemasterDirectionalLight light;
	light.enabled = remasterAreaEnabled.state == NSControlStateValueOn;
	bool valid = true;
	for (int i = 0; i < 4; ++i)
	{
		NSTextField *field = i < 3 ? remasterAreaDirection[i] : remasterAreaBrightness;
		float &value = i < 3 ? light.direction[i] : light.brightness;
		valid &= RemasterProfileParsing::ParseFloat(field.stringValue.UTF8String, value);
	}
	NSColor *color = [remasterAreaColor.color colorUsingColorSpace:[NSColorSpace genericRGBColorSpace]];
	if (!valid || light.brightness < 0 || light.brightness > 100) return NO;
	light.brightness = S9xRemasterDirectionalBrightnessFromUi(light.brightness);
	if (!color) return NO;
	light.color = {{float(color.redComponent), float(color.greenComponent), float(color.blueComponent)}};
	if (!valid || !S9xRemasterValidDirectionalLight(light)) return NO;
	remasterAreaBrightnessSlider.floatValue = S9xRemasterDirectionalBrightnessToUi(light.brightness);
	const float length = std::sqrt(light.direction[0]*light.direction[0] + light.direction[1]*light.direction[1] + light.direction[2]*light.direction[2]);
	remasterAreaAxes.normalX = light.direction[0]/length;
	remasterAreaAxes.normalY = light.direction[1]/length;
	remasterAreaAxes.normalZ = light.direction[2]/length;
	if (light.direction[2] != 0) remasterAreaBack.state = light.direction[2] < 0 ? NSControlStateValueOn : NSControlStateValueOff;
	[remasterAreaAxes setNeedsDisplay:YES];
	const auto old = S9xRemasterAreaLight(remasterEditingProfile, remasterDisplayedArea);
	if (old == light) return YES;
	const std::string key = S9xRemasterAreaKey(remasterDisplayedArea.id);
	RemasterProfile defaults;
	if (!remasterSavedAreaLights.count(key) && light == S9xRemasterAreaLight(defaults, remasterDisplayedArea))
		remasterEditingProfile.areaLights.erase(key);
	else remasterEditingProfile.areaLights[key] = light;
	remasterMetadataNeedsSync = true;
	remasterEditingProfileDirty = remasterNonSceneDirty || RemasterAreaDirty() || !S9xRemasterSceneSettingsEqual(
		S9xRemasterGetSceneSettings(remasterEditingProfile), remasterSavedSceneSettings);
	remasterAreaSave.enabled = RemasterAreaDirty();
	remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;
	remasterSaveProfileButton.enabled = remasterEditingProfileDirty;
	remasterNpcSaveButton.enabled = remasterEditingProfileDirty;
	if (running) S9xRemasterSetAreaLights(remasterEditingProfile.areaLights);
	if (remasterFramePresenting)
	{
		SyncRemasterEditingMetadataToFrame();
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
			remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled, remasterLightingView);
	}
	return YES;
}

- (void)saveRemasterAreaSettings:(id)sender
{
	[remasterAreaPanel makeFirstResponder:nil];
	if (remasterDisplayedArea.valid() && ![self changeRemasterAreaSettings:nil]) { NSBeep(); return; }
	[self writeRemasterProfile];
}

- (void)changeRemasterMetricsEnabled:(id)sender
{
	S9xRemasterSetPerformanceMetricsEnabled(remasterMetricsButton.state == NSControlStateValueOn);
	[self refreshRemasterMetrics];
}

- (void)refreshRemasterMetrics
{
	if (!remasterMetricsText)
		return;
	if (!S9xRemasterPerformanceMetricsEnabled())
	{
		remasterMetricsText.stringValue = @"Metrics are disabled.";
		return;
	}
	const RemasterState::PerformanceMetrics metrics = S9xRemasterGetPerformanceMetrics();
	remasterMetricsText.stringValue = [NSString stringWithFormat:
		@"BUILD  %s\n"
		 "DISPLAY  %.1f FPS  |  dropped %llu\n"
		 "SELECTED  %u connections  x  %u bounces\n"
		 "CPU LIGHTING  %.2f ms\n"
		 "  scene fields %.2f  +  GPU setup %.2f ms\n"
		 "  fields: wait %.2f  |  mesh %.2f ms\n"
		 "EMULATOR  refresh phase %.2f ms\n"
		 "  pacing wait %.2f ms\n"
		 "GPU FRAME  %.2f ms (all graphics)\n"
		 "  %@\n"
		 "  %u emitters / %u direct samples\n"
		 "  %u spheres / %u max samples | batch %u\n"
		 "PRESENT CPU  queue %.2f  |  drawable %.2f ms\n"
		 "Latest samples may be different frames.",
		__DATE__ " " __TIME__, metrics.presentedFps, static_cast<unsigned long long>(metrics.droppedPresentations),
		unsigned(remasterEditingProfile.samplesPerFrame), unsigned(remasterEditingProfile.indirectBounceCount),
		metrics.lightingFieldMs + metrics.lightingPreparationMs, metrics.lightingFieldMs,
		metrics.lightingPreparationMs, metrics.lightingFieldWaitMs, metrics.lightingMeshMs,
		metrics.emulationFrameMs, metrics.pacingWaitMs,
		metrics.gpuFrameMs,
		metrics.gpuStageTimingsAvailable ? [NSString stringWithFormat:@"direct %.2f | indirect %.2f ms\n  directional %.3f ms (included in direct)", metrics.gpuDirectMs, metrics.gpuIndirectMs, metrics.gpuDirectionalMs] : @"direct/indirect counters unavailable",
		metrics.directEmitterCount, metrics.directSampleCount,
		metrics.directSphereCount, metrics.directSphereSampleCount, metrics.directBatchSize,
		metrics.presentationQueueMs, metrics.drawableMs];
}

- (void)finishRemasterSceneSettingsChangeFrom:(NSData *)snapshot actionName:(NSString *)name
{
	remasterMetadataNeedsSync = true;
	[remasterUndoManager registerUndoWithTarget:self selector:@selector(restoreRemasterSceneSettings:)
		object:snapshot];
	[remasterUndoManager setActionName:name];
	remasterEditingProfileDirty = remasterNonSceneDirty || RemasterAreaDirty() || !S9xRemasterSceneSettingsEqual(
		S9xRemasterGetSceneSettings(remasterEditingProfile), remasterSavedSceneSettings);
	if (remasterSettingsSaveButton)
		remasterSettingsSaveButton.enabled = remasterEditingProfileDirty;
	if (remasterSaveProfileButton)
		remasterSaveProfileButton.enabled = remasterEditingProfileDirty;
	remasterNpcSaveButton.enabled = remasterEditingProfileLoaded && remasterEditingProfileDirty;
	if (running)
		S9xRemasterSetSceneSettings(S9xRemasterGetSceneSettings(remasterEditingProfile));
	if (remasterFramePresenting)
	{
		SyncRemasterEditingMetadataToFrame();
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
			remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled, remasterLightingView);
	}
}

- (void)restoreRemasterSceneSettings:(NSData *)snapshot
{
	if (snapshot.length != sizeof(RemasterSceneSettings))
		return;
	const RemasterSceneSettings current = S9xRemasterGetSceneSettings(remasterEditingProfile);
	const NSData *inverse = [NSData dataWithBytes:&current length:sizeof(current)];
	RemasterSceneSettings restored;
	[snapshot getBytes:&restored length:sizeof(restored)];
	S9xRemasterApplySceneSettings(remasterEditingProfile, restored);
	[self finishRemasterSceneSettingsChangeFrom:(NSData *)inverse actionName:@"Change Scene Settings"];
	remasterBounceSlider.integerValue = restored.indirectBounceCount;
	remasterBounceInput.integerValue = restored.indirectBounceCount;
	remasterHeightScaleInput.stringValue = [NSString stringWithFormat:@"%.6g", restored.lightingCoordinateScale];
	for (size_t component = 0; component < 3; component++)
		remasterCameraDirectionInputs[component].stringValue = [NSString stringWithFormat:@"%.6g", restored.cameraDirection[component]];
	remasterIndirectRoughnessSlider.floatValue = restored.indirectRoughness;
	remasterIndirectRoughnessInput.floatValue = restored.indirectRoughness;
	remasterOriginalSceneSlider.floatValue = restored.originalSceneContribution;
	remasterOriginalSceneInput.floatValue = restored.originalSceneContribution;
	remasterReflectanceBoostSlider.floatValue = restored.reflectanceBoost;
	remasterReflectanceBoostInput.floatValue = restored.reflectanceBoost;
	remasterHeightPreviewMultiplierSlider.integerValue = restored.heightPreviewMultiplier;
	remasterHeightPreviewMultiplierInput.integerValue = restored.heightPreviewMultiplier;
	remasterSamplesSlider.integerValue = restored.samplesPerFrame;
	remasterSamplesInput.integerValue = restored.samplesPerFrame;
	remasterEmitterPatchSlider.integerValue = restored.emitterPatchSize;
	remasterEmitterPatchInput.integerValue = restored.emitterPatchSize;
	[self refreshRemasterRepresentativeControls];
	remasterSampleAccumulationButton.state = restored.sampleAccumulation ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)changeRemasterHeightScale:(id)sender
{
	[self commitRemasterHeightScale];
}

- (void)changeRemasterHeightPreviewMultiplier:(id)sender
{
	if (!remasterEditingProfileLoaded)
		return;
	const NSInteger requested = sender == remasterHeightPreviewMultiplierInput ?
		remasterHeightPreviewMultiplierInput.integerValue : remasterHeightPreviewMultiplierSlider.integerValue;
	const uint8_t multiplier = static_cast<uint8_t>(std::max<NSInteger>(1, std::min<NSInteger>(20, requested)));
	remasterHeightPreviewMultiplierSlider.integerValue = multiplier;
	remasterHeightPreviewMultiplierInput.integerValue = multiplier;
	if (remasterEditingProfile.heightPreviewMultiplier == multiplier)
		return;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.heightPreviewMultiplier = multiplier;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 10);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Height Preview Multiplier"];
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
}

- (void)changeRemasterHeightPreviewRange:(id)sender
{
	NSInteger requestedMin = 0, requestedMax = 0;
	NSScanner *minScanner = [NSScanner scannerWithString:remasterHeightPreviewMinInput.stringValue];
	NSScanner *maxScanner = [NSScanner scannerWithString:remasterHeightPreviewMaxInput.stringValue];
	if (![minScanner scanInteger:&requestedMin] || !minScanner.isAtEnd ||
		![maxScanner scanInteger:&requestedMax] || !maxScanner.isAtEnd ||
		!S9xRemasterHeightPreviewRangeValid(static_cast<int>(requestedMin),
			static_cast<int>(requestedMax)))
	{
		remasterHeightPreviewMinInput.integerValue = remasterHeightPreviewMin;
		remasterHeightPreviewMaxInput.integerValue = remasterHeightPreviewMax;
		NSBeep();
		return;
	}
	if (requestedMin == remasterHeightPreviewMin && requestedMax == remasterHeightPreviewMax)
		return;
	remasterHeightPreviewMin = requestedMin;
	remasterHeightPreviewMax = requestedMax;
	SetRemasterHeightPreviewRange(static_cast<int32_t>(requestedMin), static_cast<int32_t>(requestedMax));
	if (remasterFramePresenting)
		DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
			remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled, remasterLightingView);
}

- (void)changeRemasterCameraDirection:(id)sender
{
	[self commitRemasterCameraDirection];
}

- (BOOL)commitRemasterCameraDirection
{
	if (!remasterEditingProfileLoaded || !remasterCameraDirectionInputs[0])
		return YES;
	std::array<float, 3> direction;
	float lengthSquared = 0.0f;
	for (size_t component = 0; component < direction.size(); component++)
	{
		NSScanner *scanner = [NSScanner scannerWithString:remasterCameraDirectionInputs[component].stringValue];
		if (![scanner scanFloat:&direction[component]] || !scanner.isAtEnd || !std::isfinite(direction[component]))
		{
			for (size_t restoreComponent = 0; restoreComponent < direction.size(); restoreComponent++)
				remasterCameraDirectionInputs[restoreComponent].stringValue = [NSString stringWithFormat:@"%.6g",
					remasterEditingProfile.cameraDirection[restoreComponent]];
			NSBeep();
			return NO;
		}
		lengthSquared += direction[component] * direction[component];
	}
	if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f)
	{
		for (size_t component = 0; component < direction.size(); component++)
			remasterCameraDirectionInputs[component].stringValue = [NSString stringWithFormat:@"%.6g",
				remasterEditingProfile.cameraDirection[component]];
		NSBeep();
		return NO;
	}
	if (remasterEditingProfile.cameraDirection == direction)
		return YES;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.cameraDirection = direction;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 9);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Camera Direction"];
	for (size_t component = 0; component < direction.size(); component++)
		remasterCameraDirectionInputs[component].stringValue = [NSString stringWithFormat:@"%.6g", direction[component]];
	return YES;
}

- (void)changeRemasterOriginalSceneContribution:(id)sender
{
	if (sender == remasterOriginalSceneSlider)
		remasterOriginalSceneInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterOriginalSceneSlider.floatValue];
	[self commitRemasterOriginalSceneContribution];
}

- (BOOL)commitRemasterOriginalSceneContribution
{
	if (!remasterEditingProfileLoaded || !remasterOriginalSceneInput)
		return YES;
	const float contribution = remasterOriginalSceneInput.floatValue;
	if (!std::isfinite(contribution) || contribution < 0.0f || contribution > 1.0f)
	{
		remasterOriginalSceneInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.originalSceneContribution];
		remasterOriginalSceneSlider.floatValue = remasterEditingProfile.originalSceneContribution;
		NSBeep();
		return NO;
	}
	if (remasterEditingProfile.originalSceneContribution == contribution)
		return YES;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.originalSceneContribution = contribution;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 8);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Original Scene Contribution"];
	remasterOriginalSceneSlider.floatValue = contribution;
	remasterOriginalSceneInput.stringValue = [NSString stringWithFormat:@"%.6g", contribution];
	return YES;
}

- (void)changeRemasterReflectanceBoost:(id)sender
{
	if (sender == remasterReflectanceBoostSlider)
		remasterReflectanceBoostInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterReflectanceBoostSlider.floatValue];
	[self commitRemasterReflectanceBoost];
}

- (BOOL)commitRemasterReflectanceBoost
{
	if (!remasterEditingProfileLoaded || !remasterReflectanceBoostInput)
		return YES;
	const float boost = remasterReflectanceBoostInput.floatValue;
	if (!std::isfinite(boost) || boost < 0.0f || boost > 8.0f)
	{
		remasterReflectanceBoostInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.reflectanceBoost];
		remasterReflectanceBoostSlider.floatValue = remasterEditingProfile.reflectanceBoost;
		NSBeep();
		return NO;
	}
	if (remasterEditingProfile.reflectanceBoost == boost)
		return YES;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.reflectanceBoost = boost;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 12);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Reflected Light Boost"];
	remasterReflectanceBoostSlider.floatValue = boost;
	remasterReflectanceBoostInput.stringValue = [NSString stringWithFormat:@"%.6g", boost];
	return YES;
}

- (void)refreshRemasterRepresentativeControls
{
	const uint8_t count = remasterEditingProfile.representativeEmitters;
	remasterRepresentativeEnabled.state = count ? NSControlStateValueOn : NSControlStateValueOff;
	remasterRepresentativeSlider.integerValue = count ? count : 4;
	remasterRepresentativeInput.integerValue = count ? count : 4;
	remasterRepresentativeSlider.enabled = remasterRepresentativeInput.enabled = count != 0;
	remasterEmitterPatchSlider.enabled = remasterEmitterPatchInput.enabled = count == 0;
}

- (void)changeRemasterRepresentativeEmitters:(id)sender
{
	if (!remasterEditingProfileLoaded) return;
	NSInteger value = sender == remasterRepresentativeInput ? remasterRepresentativeInput.integerValue : remasterRepresentativeSlider.integerValue;
	uint8_t count = remasterRepresentativeEnabled.state == NSControlStateValueOn ?
		static_cast<uint8_t>(std::max<NSInteger>(1, std::min<NSInteger>(8, value))) : 0;
	const auto before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.representativeEmitters = count;
	[self refreshRemasterRepresentativeControls];
	if (count == before.representativeEmitters) return;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(16, remasterEditingProfile.schemaVersion);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)] actionName:@"Change Representative Emitters"];
}

- (void)changeRemasterEmitterPatchSize:(id)sender
{
	if (!remasterEditingProfileLoaded) return;
	NSInteger value = sender == remasterEmitterPatchInput ? remasterEmitterPatchInput.integerValue : remasterEmitterPatchSlider.integerValue;
	uint8_t size = static_cast<uint8_t>(std::max<NSInteger>(1, std::min<NSInteger>(5, value)));
	remasterEmitterPatchSlider.integerValue = size;
	remasterEmitterPatchInput.integerValue = size;
	if (size == remasterEditingProfile.emitterPatchSize) return;
	const auto before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.emitterPatchSize = size;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(15, remasterEditingProfile.schemaVersion);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)] actionName:@"Change Emitter Patch Size"];
}

- (void)changeRemasterSamplesPerFrame:(id)sender
{
	if (!remasterEditingProfileLoaded)
		return;
	const NSInteger requested = sender == remasterSamplesInput ? remasterSamplesInput.integerValue : remasterSamplesSlider.integerValue;
	const uint8_t count = static_cast<uint8_t>(std::max<NSInteger>(1, std::min<NSInteger>(128, requested)));
	remasterSamplesSlider.integerValue = count;
	remasterSamplesInput.integerValue = count;
	if (remasterEditingProfile.samplesPerFrame == count)
		return;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.samplesPerFrame = count;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 8);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Samples Per Frame"];
}

- (void)changeRemasterSampleAccumulation:(NSButton *)sender
{
	if (!remasterEditingProfileLoaded)
		return;
	const bool enabled = sender.state == NSControlStateValueOn;
	if (remasterEditingProfile.sampleAccumulation == enabled)
		return;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.sampleAccumulation = enabled;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 8);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Toggle Sample Accumulation"];
}

- (void)changeRemasterIndirectRoughness:(id)sender
{
	if (sender == remasterIndirectRoughnessSlider)
		remasterIndirectRoughnessInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterIndirectRoughnessSlider.floatValue];
	[self commitRemasterIndirectRoughness];
}

- (BOOL)commitRemasterIndirectRoughness
{
	if (!remasterEditingProfileLoaded || !remasterIndirectRoughnessInput)
		return YES;
	const float roughness = remasterIndirectRoughnessInput.floatValue;
	if (!std::isfinite(roughness) || roughness < 0.0f || roughness > 1.0f)
	{
		remasterIndirectRoughnessInput.stringValue = [NSString stringWithFormat:@"%.6g", remasterEditingProfile.indirectRoughness];
		remasterIndirectRoughnessSlider.floatValue = remasterEditingProfile.indirectRoughness;
		NSBeep();
		return NO;
	}
	if (remasterEditingProfile.indirectRoughness == roughness)
		return YES;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.indirectRoughness = roughness;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 9);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Indirect Roughness"];
	remasterIndirectRoughnessSlider.floatValue = roughness;
	remasterIndirectRoughnessInput.stringValue = [NSString stringWithFormat:@"%.6g", roughness];
	return YES;
}

- (BOOL)commitRemasterHeightScale
{
	if (!remasterEditingProfileLoaded || !remasterHeightScaleInput)
		return YES;
	const float scale = remasterHeightScaleInput.floatValue;
	if (!std::isfinite(scale) || scale <= 0.0f)
	{
		remasterHeightScaleInput.stringValue = [NSString stringWithFormat:@"%.6g",
			remasterEditingProfile.lightingCoordinateScale];
		NSBeep();
		return NO;
	}
	if (remasterEditingProfile.lightingCoordinateScale == scale)
		return YES;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.lightingCoordinateScale = scale;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 4);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Height Scale"];
	remasterHeightScaleInput.stringValue = [NSString stringWithFormat:@"%.6g", scale];
	return YES;
}

- (void)changeRemasterBounceCount:(id)sender
{
	if (!remasterEditingProfileLoaded)
		return;
	const NSInteger requested = sender == remasterBounceInput ? remasterBounceInput.integerValue : remasterBounceSlider.integerValue;
	const uint8_t count = static_cast<uint8_t>(std::max<NSInteger>(0, std::min<NSInteger>(16, requested)));
	remasterBounceSlider.integerValue = count;
	remasterBounceInput.integerValue = count;
	if (remasterEditingProfile.indirectBounceCount == count)
		return;
	const RemasterSceneSettings before = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterEditingProfile.indirectBounceCount = count;
	remasterEditingProfile.schemaVersion = std::max<uint32_t>(remasterEditingProfile.schemaVersion, 4);
	[self finishRemasterSceneSettingsChangeFrom:[NSData dataWithBytes:&before length:sizeof(before)]
		actionName:@"Change Indirect Bounces"];
}

- (void)previousRemasterVariant:(id)sender
{
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:(remasterVariantIndex + remasterVariants.size() - 1) % remasterVariants.size()];
}

- (void)nextRemasterVariant:(id)sender
{
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:(remasterVariantIndex + 1) % remasterVariants.size()];
}

- (NSString *)loadRemasterProfile:(NSURL *)fileURL
{
	try
	{
		return [self loadRemasterProfileChecked:fileURL];
	}
	catch (const std::bad_alloc &)
	{
		return @"There was not enough memory to load this remaster profile. Close other applications and try again.";
	}
	catch (const std::exception &error)
	{
		return [NSString stringWithFormat:@"The remaster profile could not be loaded: %s", error.what()];
	}
}

- (NSString *)loadRemasterProfileChecked:(NSURL *)fileURL
{
	// Saving can replace fileURL itself. Resolve pending edits before reading so
	// reloading the current file cannot activate a pre-save snapshot.
	if (remasterEditingProfileLoaded && ![self confirmDiscardingRemasterChanges])
		return @"";
	RemasterProfile profile;
	std::vector<RemasterProfileDiagnostic> diagnostics;
	if (!S9xRemasterLoadProfile(fileURL.path.UTF8String, profile, diagnostics))
	{
		std::ostringstream message;
		for (size_t i = 0; i < diagnostics.size(); i++)
		{
			if (i)
				message << '\n';
			if (diagnostics[i].line)
				message << "Line " << diagnostics[i].line << ": ";
			message << diagnostics[i].message;
		}
		return [NSString stringWithUTF8String:message.str().c_str()];
	}

	std::string romHash;
	if (remasterNpcInspectorActive && !running)
		romHash = remasterNpcInspectorFrame.profileRomSha256;
	else if (remasterFramePresenting)
		romHash = remasterSceneFrame.profileRomSha256;
	else
	{
		static const char hex[] = "0123456789abcdef";
		romHash.assign(64, '0');
		for (size_t i = 0; i < 32; i++)
		{
			romHash[i * 2] = hex[Memory.ROMSHA256[i] >> 4];
			romHash[i * 2 + 1] = hex[Memory.ROMSHA256[i] & 15];
		}
	}
	if (profile.romSha256 != romHash)
		return remasterFramePresenting && remasterReplayFrame.profileRomSha256.empty() ?
			@"This frame was captured without an active remaster profile. Load the profile while the game is running, then make a new capture." :
			remasterFramePresenting ? @"Profile ROM SHA-256 does not match the captured frame." :
			@"Profile ROM SHA-256 does not match the running game.";
	std::vector<RemasterDungeonHeightMap> reviewMaps;
	NSString *reviewDirectory = [fileURL.path stringByAppendingString:@".geometry-review"];
	BOOL isDirectory = NO;
	if ([[NSFileManager defaultManager] fileExistsAtPath:reviewDirectory isDirectory:&isDirectory])
	{
		if (!isDirectory)
			return @"The profile's geometry-review path must be a directory.";
		NSError *listError = nil;
		NSArray<NSString *> *names = [[NSFileManager defaultManager]
			contentsOfDirectoryAtPath:reviewDirectory error:&listError];
		if (!names)
			return @"Could not read the profile's geometry-review directory.";
		std::set<unsigned> seenRooms;
		for (NSString *name in names)
		{
			if (![name hasSuffix:@".bin"])
				continue;
			unsigned expectedRoom = 0;
			if (name.length != 12 || ![name hasPrefix:@"room-"])
				return [NSString stringWithFormat:@"Invalid geometry-review map name: %@", name];
			NSScanner *scanner = [NSScanner scannerWithString:[name substringWithRange:NSMakeRange(5, 3)]];
			if (![scanner scanHexInt:&expectedRoom] || !scanner.isAtEnd || expectedRoom >= 320)
				return [NSString stringWithFormat:@"Invalid geometry-review map name: %@", name];
			RemasterDungeonHeightMap map;
			NSString *path = [reviewDirectory stringByAppendingPathComponent:name];
			if (!S9xRemasterReadDungeonHeightMap(path.UTF8String, map) ||
				map.roomIndex != expectedRoom || !seenRooms.insert(expectedRoom).second)
				return [NSString stringWithFormat:@"Invalid geometry-review map: %@", name];
			reviewMaps.push_back(std::move(map));
		}
	}
	const NSUInteger reviewMapCount = reviewMaps.size();
	std::string savedProfileText;
	if (!S9xRemasterSerializeProfile(profile, savedProfileText, diagnostics, false))
		return @"The remaster profile contains metadata that cannot be saved. Correct its asset metadata and try again.";

	remasterEditingProfile = std::move(profile);
	remasterEditingProfileURL = fileURL;
	remasterProfileDiskSnapshot = [NSData dataWithContentsOfURL:fileURL];
	remasterPendingProfileConflicts.clear();
	remasterEditingProfileLoaded = true;
	remasterMetadataNeedsSync = true;
	S9xRemasterState().requestedInspectableFrames.store(running);
	remasterEditingProfileDirty = false;
	remasterSavedSceneSettings = S9xRemasterGetSceneSettings(remasterEditingProfile);
	remasterSavedAreaLights = remasterEditingProfile.areaLights;
	remasterDisplayedArea = {};
	remasterAreaNeedsRefresh = true;
	[self refreshRemasterAreaSettings];
	remasterNonSceneDirty = false;
	[[NSUserDefaults standardUserDefaults] setObject:fileURL.path forKey:RemasterLastProfilePathKey];
	if (running) workspaceProfilePath = fileURL.path;
	remasterSavedProfileText = std::move(savedProfileText);
	[remasterUndoManager removeAllActions];
	if (running)
	{
		S9xRemasterSetProfile(remasterEditingProfile);
		S9xRemasterSetDungeonHeightMaps(std::move(reviewMaps));
		SetLiveRemasterPresentation(remasterLightingEnabled, remasterLightingView);
	}
	if (remasterMaterialBrush)
	{
		[remasterMaterialBrush removeAllItems];
		[remasterMaterialBrush addItemWithTitle:@"Inherit"];
		for (const auto &entry : remasterEditingProfile.materials)
			[remasterMaterialBrush addItemWithTitle:[NSString stringWithUTF8String:entry.first.c_str()]];
	}
	if (!remasterVariants.empty())
		[self showRemasterVariantAtIndex:remasterVariantIndex];
	else
	{
		SyncRemasterEditingMetadataToFrame();
		if (remasterFramePresenting)
			DrawRemasterFrame(remasterReplayFrame, remasterReplayDebugMode,
				remasterSelectionValid ? &remasterSelectedTiles : nullptr, remasterLightingEnabled,
				remasterLightingView);
		[self refreshRemasterEditingControls];
	}
	[self showRemasterProfileSettings];
	remasterProfileSettingsPanel.title = running && reviewMapCount ?
		[NSString stringWithFormat:@"Remaster Scene Controls — Geometry Review (%lu rooms)",
			static_cast<unsigned long>(reviewMapCount)] : @"Remaster Scene Controls";
	if (remasterNpcPanel.visible) [self changeRemasterNpcPose:nil];
	return nil;
}

- (void)setVideoMode:(int)mode
{
    videoMode = mode;
}

- (void)setMacFrameSkip:(int)_macFrameSkip
{
    macFrameSkip = _macFrameSkip;
	
	// contrains to -1 to 200
	if (macFrameSkip < -1)
		macFrameSkip = -1;
	if (macFrameSkip > 200)
		macFrameSkip = 200;
}

- (void)setDeviceSetting:(S9xDeviceSetting)_deviceSetting
{
	[s9xView cancelRemasterDebugLightGesture];
	deviceSetting = _deviceSetting;
	ChangeInputDevice();
}

- (void)setSuperFXClockSpeedPercent:(uint32_t)clockSpeed
{
	Settings.SuperFXClockMultiplier = clockSpeed;
}

- (void)setSoundInterpolationType:(int)type
{
	Settings.InterpolationMethod = type;
}

- (void)setCPUOverclockMode:(int)mode
{
	Settings.OverclockMode = mode;
}

- (void)setApplySpecificGameHacks:(BOOL)flag
{
	Settings.DisableGameSpecificHacks = !flag;
}

- (void)setAllowInvalidVRAMAccess:(BOOL)flag
{
	Settings.BlockInvalidVRAMAccessMaster = !flag;
}

- (void)setSeparateEchoBufferFromRAM:(BOOL)flag
{
	Settings.SeparateEchoBuffer = false;
}

- (void)setDisableSpriteLimit:(BOOL)flag
{
	if ( flag )
	{
		Settings.MaxSpriteTilesPerLine = 128;
	}
	else
	{
		Settings.MaxSpriteTilesPerLine = 34;
	}
}

@dynamic inputDelegate;
- (void)setInputDelegate:(id<S9xInputDelegate>)delegate
{
    inputDelegate = delegate;
}

- (id<S9xInputDelegate>)inputDelegate
{
    return inputDelegate;
}

@dynamic cheatsEnabled;
- (BOOL)cheatsEnabled
{
	return Cheat.enabled;
}

- (void)setCheatsEnabled:(BOOL)cheatsEnabled
{
	Cheat.enabled = cheatsEnabled;
}

- (void)copyRAM:(uint8_t *)buffer length:(size_t)length
{
	if ( length > 0x20000)
	{
		length = 0x20000;
	}

	memcpy(buffer, Memory.RAM, length);
}

- (NSArray<S9xWatchPoint *> *)getWatchPoints
{
	NSMutableArray<S9xWatchPoint *> *watchPoints = [NSMutableArray new];

	for (NSUInteger i = 0; i < sizeof(watches)/sizeof(*watches); ++i)
	{
		if (watches[i].on)
		{
			S9xWatchPoint *watchPoint = [S9xWatchPoint new];
			watchPoint.address = watches[i].address;
			watchPoint.size = watches[i].size;
			watchPoint.format = (S9xWatchPointFormat)watches[i].format;

			[watchPoints insertObject:watchPoint atIndex:0];
		}
	}

	return watchPoints;
}

- (void)setWatchPoints:(NSArray<S9xWatchPoint *> *)watchPoints
{
	memset(watches, 0, sizeof(watches));
	NSUInteger i = 0;

	for (S9xWatchPoint *watchPoint in watchPoints.reverseObjectEnumerator)
	{
		uint32_t address = watchPoint.address;
		watches[i].on = true;
		watches[i].address = address;
		watches[i].size = watchPoint.size;
		watches[i].format = watchPoint.format;

		if(address < 0x7E0000 + 0x20000)
		{
			snprintf(watches[i].desc, sizeof(watches[i].desc), "%6X", address);
		}
		else if(address < 0x7E0000 + 0x30000)
		{
			snprintf(watches[i].desc, sizeof(watches[i].desc), "s%05X", address - 0x7E0000 - 0x20000);
		}
		else
		{
			snprintf(watches[i].desc, sizeof(watches[i].desc), "i%05X", address - 0x7E0000 - 0x30000);
		}

		++i;

		if (i == 16)
		{
			break;
		}
	}
}

- (void)gameLoaded
{
	[self.emulationDelegate gameLoaded];
}

- (void)emulationPaused
{
	dispatch_async(dispatch_get_main_queue(), ^{ [self savePreviousWorkspace]; });
	[self.emulationDelegate emulationPaused];
}

- (void)emulationResumed
{
	dispatch_async(dispatch_get_main_queue(), ^{ [self savePreviousWorkspace]; });
	[self.emulationDelegate emulationResumed];
}

@end

@implementation S9xJoypad

- (BOOL)isEqual:(id)object
{
    if (![object isKindOfClass:[self class]])
    {
        return NO;
    }

    S9xJoypad *other = (S9xJoypad *)object;
    return (self.vendorID == other.vendorID && self.productID == other.productID && self.index == other.index);
}

@end

@implementation S9xJoypadInput
@end

@implementation S9xWatchPoint
@end
