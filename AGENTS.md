# AGENTS.md

Unreal Engine 5.8 editor plugin (C++) providing a Source Control provider that shells out to system `git` (with Git LFS lock support).

## Repo layout

- `UnrealGit.uplugin` — plugin descriptor; modules `UnrealGit` (Editor) and `UnrealGitTests` (DeveloperTool).
- `Source/UnrealGit/` — implementation; `Public/UnrealGit/` headers, `Private/UnrealGit/` impl.
- `Source/UnrealGitTests/` — Unreal Automation tests; mocks in `Private/Mocks/`.
- `Engine/` — full UE 5.8.4 checkout used as the host toolchain; **gitignored**, do not commit or edit.
- Generated outputs `Binaries/`, `Intermediate/` are gitignored.

## PRINCIPLES

All agents must obey following principles.

1. **KISS** - Keep It Simple, Stupid. Simple > clever.
2. **DRY** - Don't Repeat Yourself. Extract shared logic into utilities.
3. **SOLID** - Single responsibility, open/closed, Liskov, interface seg, dependency inversion.
4. **SoC** - Separation of Concerns. Each module does one thing.
5. **Law of Demeter** - Talk only to your immediate neighbors. No chain `.get().thing().prop()`.

## Build & test

- **Build/compile (required):** `just build` from repo root. Do not invoke UBT/UAT directly for builds.
- No CI, lint, or typecheck config in repo; verification is local.

## Tooling Restrictions

- Use CodeGraph as an almost grep replacement: prefer `codegraph explore "<symbol names or question>"` (or the `codegraph_explore` MCP tool when available) over grep/find + read loops for locating or understanding code.
- When searching engine code, `cd Engine` first and run CodeGraph from there; running it from the plugin root resolves to the plugin's index, not the engine's.
- Agents must not use Python, Perl, sed, ex, ed, or other command-line editors.
- Do not rewrite files with `cat` or any stream editor; request maintainers to run scripts and keep edits within the provided tooling.
- Never run anything inside `Scripts/` directly. Document the need and let a maintainer execute it. Exception: `just build` and `just test` are allowed.
- **Never run `just package`, or any other cook/package command, under any circumstances.** This covers `just package <platform> [--config <name>] [--client] [--server]`, direct `RunUAT.sh BuildCookRun`, and `UnrealEditor-Cmd -run=Cook`. Packaging is maintainer-only, full stop. There is no exception for Development config, none for verifying a bug fix, and none for "just checking". Agents verify compilation with `just build` only, then hand the exact packaging command back to the maintainer.

## Conventions

- Unreal C++ style: tabs, braces on new lines, `F/U/A` prefixes.
- Keep `Public/` headers minimal/stable; internal helpers in `Private/`.
- Git/process access goes through `IGitProcessRunner`; tests use `FMockGitProcessRunner` — keep tests deterministic (no network, no real git).
- `*.Build.cs`: keep dependency lists minimal; correct `Public` vs `Private` module lists.
- Commit messages: short imperative summaries.

## Standards and documented deviations


- **Logging:** every plugin message uses `LogUnrealGit`, declared in `Source/UnrealGit/Public/UnrealGit.h`. Do not log plugin messages through the engine's `LogSourceControl`; that category is reserved for editor integration. The Lunaris `Source/Logging/Log.h` helpers are not linked into this plugin.
- **Enums:** public enums use PascalCase enumerators (Epic Games standard) rather than the all-uppercase Lunaris style, so engine-facing types read like engine types.
- **Tests:** existing suites use `IMPLEMENT_SIMPLE_AUTOMATION_TEST`; new multi-case suites should prefer `BEGIN_DEFINE_SPEC`. Tests drive `FMockGitProcessRunner` and must never invoke a real `git` binary or the network.
- **Helper linkage:** prefer file-scope `static` over anonymous namespaces in new files; existing anonymous namespaces are tolerated (soft rule).

## General Rules

- Follow [Epic Games C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) as primary reference
- **Indentation**: Tab indentation with visual width of 4 (see `.editorconfig`)
- **Line endings**: LF (Unix-style)
- **Charset**: UTF-8 for all source files
- **Braces**: Opening brace on new line (Allman style)
- Prefer early returns over nested if-statements
- Generally discourage anonymous namespaces
- Prefer protected variables over `meta=(AllowPrivateAccess=true)`
- Avoid looking in external engine directors that are not in the current workspace

## Agent rules

- `@fixer` must split work into smaller chunks and edit files in small incremental passes — never one huge patch. Edits touching 1000+ lines per patch frequently fail; land part of the change, verify, then continue.

## Gotchas

- `Engine/` is a vendored local checkout, not part of the plugin; never commit it.
- Tests write to `ProjectIntermediateDir()`, so they require a host project; running against the engine alone will fail.