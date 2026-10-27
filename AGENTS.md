# AGENTS.md

Unreal Engine 5.8 editor plugin (C++) providing a Source Control provider that shells out to system `git` (with Git LFS lock support).

## Repo layout
- `UnrealGit.uplugin` — plugin descriptor; modules `UnrealGit` (Editor) and `UnrealGitTests` (DeveloperTool).
- `Source/UnrealGit/` — implementation; `Public/UnrealGit/` headers, `Private/UnrealGit/` impl.
- `Source/UnrealGitTests/` — Unreal Automation tests; mocks in `Private/Mocks/`.
- `Engine/` — full UE 5.8.4 checkout used as the host toolchain; **gitignored**, do not commit or edit.
- Generated outputs `Binaries/`, `Intermediate/` are gitignored.

## Build & test
- **Build/compile (required):** `just build` from repo root. Do not invoke UBT/UAT directly for builds.
- Package plugin (Linux/macOS):
  `Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="$PWD/UnrealGit.uplugin" -Package="$PWD/../UnrealGitPackage"`
- Package plugin (Windows):
  `Engine/Build/BatchFiles/RunUAT.bat BuildPlugin -Plugin="<path>/UnrealGit.uplugin" -Package="<out>/UnrealGit" -TargetPlatforms=Win64`
- Tests need a **host `.uproject`** (none exists in this repo); point the command at one:
  `Engine/Binaries/Linux/UnrealEditor-Cmd "<HostProject>.uproject" -ExecCmds="Automation RunTests UnrealGit.; Quit" -unattended -nop4 -NullRHI`
- Single test: replace filter with full name, e.g. `Automation RunTests UnrealGit.Revision.MaterializeBinary`.
- No CI, lint, or typecheck config in repo; verification is local.

## Conventions
- Unreal C++ style: tabs, braces on new lines, `F/U/A` prefixes.
- Keep `Public/` headers minimal/stable; internal helpers in `Private/`.
- Git/process access goes through `IGitProcessRunner`; tests use `FMockGitProcessRunner` — keep tests deterministic (no network, no real git).
- `*.Build.cs`: keep dependency lists minimal; correct `Public` vs `Private` module lists.
- Commit messages: short imperative summaries.

## Gotchas
- `Engine/` is a vendored local checkout, not part of the plugin; never commit it.
- Tests write to `ProjectIntermediateDir()`, so they require a host project; running against the engine alone will fail.