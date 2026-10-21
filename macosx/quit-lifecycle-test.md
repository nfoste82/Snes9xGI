# Quit lifecycle regression

The October 20 crash reports show `exit()` destroying `RemasterState`/profile
maps while the detached emulation thread still processes input or frames, and a
Metal completion handler locking the already destroyed performance-metrics mutex.
`SNES9X_Quit()` saves SRAM/cheats; it does **not** stop the emulation loop.

`S9xEngine.stop` now requests an atomic loop stop, joins a dispatch group entered
before thread launch, drains/deinitializes presentation, and only then saves the
game. `quit` uses that same path. A per-renderer shared callback gate additionally
disables late drawable-presented notifications; command completion alone does not
bound their lifetime. Reinitializing the renderer creates a fresh gate.

After building Release, run from the repository root:

```sh
clang++ -std=c++17 -fobjc-arc -framework Cocoa macosx/quit-lifecycle-test.mm \
  -o build/quit-lifecycle-smoke
python3 - <<'PY'
import subprocess
for mode in ['empty', 'immediate', 'playing', 'paused', 'restart']:
    subprocess.run(['build/quit-lifecycle-smoke', mode], check=True, timeout=45)
PY
```

The native test loads the built framework, creates the engine/window, uses the
local ALTTP ROM and checked-in profile for non-immediate gameplay cases, invokes
quit twice, and returns normally to exercise static destruction. It requires a
macOS GUI session and updates the ordinary local SRAM/workspace preferences.
The restart case also reopens a game after renderer teardown. This is a lifecycle
smoke test, not a visual gameplay or manual menu/Command-Q validation.
