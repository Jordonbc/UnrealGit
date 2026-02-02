# Repository Guidelines

## Project Structure & Module Organization

- Plugin descriptor: `UnrealGit.uplugin`
- Main module (Editor): `Source/UnrealGit/`
  - Public API headers: `Source/UnrealGit/Public/UnrealGit/**`
  - Implementation: `Source/UnrealGit/Private/UnrealGit/**`
- Test module (DeveloperTool): `Source/UnrealGitTests/` (Unreal Automation Tests)
- Assets: `Content/` (plugin content), `Resources/` (icons/resources)
- Generated outputs (do not commit): `Binaries/`, `Intermediate/` (also in `.gitignore`)

## Build, Test, and Development Commands

This is an Unreal Engine plugin; build and run it from a host Unreal project.

- Package the plugin (requires an Unreal Engine checkout):
  - Windows: `Engine/Build/BatchFiles/RunUAT.bat BuildPlugin -Plugin="<path>/UnrealGit.uplugin" -Package="<out>/UnrealGit" -TargetPlatforms=Win64`
  - macOS/Linux: `Engine/Build/BatchFiles/RunUAT.sh BuildPlugin -Plugin="<path>/UnrealGit.uplugin" -Package="<out>/UnrealGit"`
- Run tests in-editor: Session Frontend → Automation → filter `UnrealGit.*`
- Run tests headless (example):
  - `UnrealEditor-Cmd "<YourProject>.uproject" -ExecCmds="Automation RunTests UnrealGit.; Quit" -unattended -nop4 -NullRHI`

## Coding Style & Naming Conventions

- Follow Unreal C++ conventions (tabs for indentation, braces on new lines, `F/U/A` type prefixes).
- Keep headers in `Public/` minimal and stable; prefer internal helpers in `Private/`.
- Prefer descriptive class/file names matching existing patterns (e.g., `GitStatusParser`, `FGitRevisionMaterializer`).
- When updating `*.Build.cs`, keep dependency lists minimal and place modules in the correct `PublicDependencyModuleNames` vs `PrivateDependencyModuleNames`.

## Testing Guidelines

- Tests use Unreal’s Automation Framework (`IMPLEMENT_SIMPLE_AUTOMATION_TEST`) and should live under `Source/UnrealGitTests/Private/**`.
- Name tests under the `UnrealGit.*` namespace (example: `UnrealGit.Revision.MaterializeBinary`).
- Keep tests deterministic: avoid network access and real git calls; use mocks in `Source/UnrealGitTests/Private/Mocks/`.

## Commit & Pull Request Guidelines

- Commit messages: short, imperative summaries (matching existing history; e.g., “First implementation”).
- PRs: include a clear description, steps to validate, and link related issues. Add screenshots/gifs for editor UI behavior changes and note any new settings or dependencies.

## Security & Configuration Tips

- The plugin shells out to system Git; ensure `git` is available on `PATH` (and `git lfs` if using LFS/locks).
- Large/binary assets should use Git LFS where appropriate (e.g., `*.png` is configured in `.gitattributes`).
