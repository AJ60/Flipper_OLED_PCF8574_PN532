# Visual Studio Code workspace for Flipper Zero {#vscode}

## Setup

 * To start developing with VSCode, run `./fbt vscode_dist` in project root (on Windows use `cmd //c fbt.cmd vscode_dist` from Git Bash, or `fbt.cmd vscode_dist` from cmd/PowerShell). _That should only be done once_
 * After that, open firmware folder in VSCode: "File" > "Open folder"

 For more details on fbt, see [fbt docs](../documentation/fbt.md).


## Workflow

Commands for building firmware are invoked through the Build menu: **Ctrl+Shift+B**.

### Important: this fork has no `firmware` target

Running `fbt firmware` (or `fbt.cmd firmware`) fails with a *"target not built"* error. The default target (`basic_dist`) builds the complete firmware package, so run `fbt` / `fbt.cmd` with **no arguments**.

### Windows tasks (this fork)

Because the `./fbt` shell script refuses to run under MinGW, the tasks in `tasks.json` marked **`[Windows] ... (fbt.cmd)`** invoke the batch launcher directly (via `cmd.exe`, so they work no matter which shell VSCode uses):

| Task | Command | Purpose |
|---|---|---|
| `[Windows] Build Firmware (fbt.cmd, default target)` | `fbt.cmd` | Build the complete firmware package (`.bin` / `.dfu` / `.hex`) |
| `[Windows] Flash (USB, with resources) (fbt.cmd)` | `fbt.cmd FORCE=1 flash_usb_full` | Build (if needed) and flash firmware + resources over USB |
| `[Windows] Build Signal Generator FAP (fbt.cmd)` | `fbt.cmd fap_signal_generator` | Build only the signal generator FAP |

These tasks are generated from `.vscode/example/tasks.json` by `fbt vscode_dist`. The other (non-`[Windows]`) tasks use `./fbt` and are intended for POSIX hosts.

To attach a debugging session, first build and flash firmware, then choose your debug probe in Debug menu (Ctrl+Shift+D).

Note that you have to detach debugging session before rebuilding and re-flashing firmware.
