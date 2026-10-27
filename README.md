# UnrealGit

An Unreal Engine **Source Control provider** that drives the editor's Source Control panel through the system `git` CLI (with Git LFS lock integration).

- **Engine:** Unreal Engine 5.8+
- **License:** GPL-3.0 (`LICENSE`)
- **Author:** Jordon Brooks

## Features

- Status, diff, history, revert, check-out, check-in (commit), sync (pull), and submit workflows wired into Unreal's Source Control menu.
- Git LFS lock integration: lock/unlock, cached lock state, auto-lock on checkout, configurable refresh interval.
- Branch ahead/behind reporting and repo-root discovery (with optional override).
- Per-project and per-user settings (pull rebase, auto-push after submit, LFS locks, executable override, repo root override).
- Deterministic unit tests (Unreal Automation Framework) using mocked git process runners.

## Requirements

- Unreal Engine 5.8 or newer.
- `git` available on `PATH` (or set via the User settings override).
- `git lfs` (only if you use LFS/locks).

## Installation

1. Place this plugin in your project's `Plugins/` directory (or in your engine's `Plugins/` directory).
2. Enable **UnrealGit** in the Plugins browser (it is an Editor plugin).
3. Restart the editor.
4. Open **Source Control** settings and select the `UnrealGit` provider, then connect to your repository.

## Configuration

Project-level (`Edit > Project Settings > Plugins > UnrealGit`):

| Setting | Description |
| --- | --- |
| Repository Root Override | Explicit repo root; empty = auto-discover from project directory. |
| Pull Rebase | Use `git pull --rebase` instead of `git pull`. |
| Auto Push After Submit | Push after a successful commit. |
| Enable LFS Locks | Toggle Git LFS lock integration. |
| LFS Locks Refresh Interval (s) | Refresh cadence for `git lfs locks`. |

User-level (`Edit > Editor Preferences > Plugins > UnrealGit (User)`):

| Setting | Description |
| --- | --- |
| Git Executable Override | Override the `git` binary path (empty = `PATH` lookup). |
| Auto Lock On Checkout | Automatically lock LFS files when checking them out. |

## Development

Build and test from a host Unreal project:

- Package the plugin:
  - Windows: `Engine/Build/BatchFiles/RunUAT.bat BuildPlugin -Plugin="<path>/UnrealGit.uplugin" -Package="<out>/UnrealGit" -TargetPlatforms=Win64`
  - macOS/Linux: `Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="<path>/UnrealGit.uplugin" -Package="<out>/UnrealGit"`
- Run tests in-editor: Session Frontend → Automation → filter `UnrealGit.*`
- Run tests headless:
  - `UnrealEditor-Cmd "<YourProject>.uproject" -ExecCmds="Automation RunTests UnrealGit.; Quit" -unattended -nop4 -NullRHI`

Tests live under `Source/UnrealGitTests/Private/**` and use mocks in `Source/UnrealGitTests/Private/Mocks/` (no network or real git calls).

## Project Layout

```
UnrealGit.uplugin
Source/UnrealGit/            # Editor module
  Public/UnrealGit/          # Public headers
  Private/UnrealGit/         # Implementation
Source/UnrealGitTests/       # DeveloperTool test module
Content/                     # Plugin content
Resources/                   # Icons/resources
```

## License

Distributed under the GNU General Public License v3.0. See `LICENSE`.