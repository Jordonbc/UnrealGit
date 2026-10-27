# UnrealVCS

An Unreal Engine **Source Control plugin** structured for multiple version-control backends. The current backend drives the editor's Source Control panel through the system `git` CLI (with Git LFS lock integration); additional backends are planned.

- **Engine:** Unreal Engine 5.8+
- **License:** GPL-3.0 (`LICENSE`)
- **Author:** Jordon Brooks

## Features

- Status, diff, history, revert, check-out, check-in (commit), sync (pull), delete, copy/redirector staging, conflict resolve, and submit workflows wired into Unreal's Source Control menu.
- Commit batching (50 paths per git invocation) with scoped commits, so partial submits only include the selected files and large submits stay under platform command-line limits.
- Revert discards both staged and worktree changes and restores files deleted on disk.
- Merge-conflict details (`base`/`remote` file and revision) exposed to the editor's resolve tooling.
- Git LFS integration: check-out acquires the LFS lock and makes the asset writable; submit and revert release it. Locks held by other users surface as "checked out by" state and icons.
- Branch ahead/behind reporting and repo-root discovery (with optional override).
- In-editor settings panel: git executable picker with automatic discovery of common installs, repository/branch/remote/user status, and a "Initialize project with Git" wizard (`.gitignore`, `README.md`, LFS `.gitattributes`, initial commit).
- History metadata: revision numbers, per-revision actions (add/delete/rename), and file sizes.
- Per-project and per-user settings (pull rebase, auto-push after submit, LFS locks, executable override, repo root override).
- Deterministic unit tests (Unreal Automation Framework) using mocked git process runners.

## Requirements

- Unreal Engine 5.8 or newer.
- `git` available on `PATH` (or set via the User settings override).
- `git lfs` (only if you use LFS/locks).

## Installation

1. Place this plugin in your project's `Plugins/` directory (or in your engine's `Plugins/` directory).
2. Enable **UnrealVCS** in the Plugins browser (it is an Editor plugin).
3. Restart the editor.
4. Open **Source Control** settings and select the `UnrealGit` provider (the current Git backend), then connect to your repository.

## Configuration

The Source Control settings panel (Source Control toolbar → gear icon) exposes the git executable picker, live repository/branch/remote/user status, and an "Initialize project with Git" wizard when the project is not yet a repository.

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
| Git Executable Override | Override the `git` binary path (empty = auto-detect, falling back to `PATH`). |

## Development

Build and test from a host Unreal project:

- Package the plugin:
  - Windows: `Engine/Build/BatchFiles/RunUAT.bat BuildPlugin -Plugin="<path>/UnrealVCS.uplugin" -Package="<out>/UnrealVCS" -TargetPlatforms=Win64`
  - macOS/Linux: `Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="<path>/UnrealVCS.uplugin" -Package="<out>/UnrealVCS"`
- Run tests in-editor: Session Frontend → Automation → filter `UnrealGit.*`
- Run tests headless:
  - `UnrealEditor-Cmd "<YourProject>.uproject" -ExecCmds="Automation RunTests UnrealGit.; Quit" -unattended -nop4 -NullRHI`

Tests live under `Source/UnrealGitTests/Private/**` and use mocks in `Source/UnrealGitTests/Private/Mocks/` (no network or real git calls).

## Project Layout

```
UnrealVCS.uplugin
Source/UnrealGit/            # Editor module (Git backend)
  Public/UnrealGit/          # Public headers
  Private/UnrealGit/         # Implementation
Source/UnrealGitTests/       # DeveloperTool test module
Content/                     # Plugin content
Resources/                   # Icons/resources
```

## License

Distributed under the GNU General Public License v3.0. See `LICENSE`.