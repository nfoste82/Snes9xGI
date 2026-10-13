# Previous game and authoring workspace

Choose **File > Open Previous Game and Windows** (Command-Shift-O) to reopen
the last successfully opened game (including MultiCart), load its saved remaster
profile, and reopen Scene Controls and NPC Poses if they were left open. The
game's paused/running state is remembered. This opens the ROM normally; it does
not create or restore an emulator save state or unsaved profile edits.

The workspace is updated when a game/profile is opened, authoring panels are
opened/closed, and the game is paused/resumed or the app quits. A newly opened
game starts a new workspace; its profile is recorded only after a successful
profile load for that game. Restore uses the normal profile parser and ROM hash
check. Pending profile edits use the existing save/discard/cancel prompt before
replacing a game.

Missing game files are checked before replacing the current game. The error
shows the saved path; open the game at its new location to update it. If the
profile is missing, invalid, or mismatched, the game still opens and an error
explains the profile failure. The profile path remains saved for retry. NPC
Poses uses its existing saved-library lookup and library chooser if unavailable.

Game, Scene Controls, NPC Poses, Pixel Inspector, debug light, preferences,
Cheats and Cheat Finder windows save their frames when moved/resized. Physical
display UUIDs are saved alongside screen coordinates. If the display is absent
or the record is invalid, window creation uses the default centered position.
Rearranged displays retain the window's display-relative position; resolution
or Dock/menu-bar changes constrain the title bar to the available screen.
Fullscreen frames are not saved over ordinary window placement.

**View > Mute Audio / Unmute Audio** is available during emulation and authoring.
Its preference survives relaunches, ROM loads and defrosts. The audio callback
consumes samples normally and silences its output, preserving audio-sync timing
and the configured volume.

## Verification (October 13, 2026)

- Full arm64 Debug Xcode build passed with signing disabled.
- Native Cocoa/framework smoke checks passed: no saved workspace, missing and
  invalid ROM paths, mute preference, window-frame round trip, disconnected
  display rejection, and offscreen title-bar recovery.
- Native runtime checks with the local ALTTP ROM and checked-in profile passed:
  restore opens the ROM/profile paused, stays paused after emulation startup,
  records the profile, and a missing profile leaves the reopened game paused
  without a stale editor profile.
- `git diff --check` passed. Interactive menu use, audible output, and physical
  monitor unplug/replug have not been manually verified.
