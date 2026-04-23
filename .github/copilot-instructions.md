# Project Guidelines

## Session Entry

- At the start of every new session, read `net/rep/rule/rule.md` first.
- Before editing, read `net/rep/rule/map.md` to narrow the target area and read `net/rep/rule/coderule.md` before changing C or headers.
- Do not sweep the whole repository before identifying the relevant directory and its authoritative Markdown entry document.

## Architecture

- Treat `net/rep/` as the reusable core layer. Unless the task explicitly requires core changes, prefer adapting behavior in `net/User/`, `net/User/port/`, `net/User/system/`, or other project-bound layers.
- The active embedded entry point and scheduler startup are anchored in `net/MDK/main.c`; the build source of truth is `net/MDK/network.uvprojx`.
- Project-bound orchestration, manager logic, and system wiring should stay out of reusable `rep/` code.
- Outside `User/port/rtos_port.*`, do not call native RTOS APIs directly; use the abstractions exposed by `net/rep/service/rtos/rtos.h`.

## Build And Validation

- Prefer existing VS Code tasks for build, rebuild, flash, reset, RTT, and serial monitor workflows.
- Build and flash tooling is driven by `scripts/vscode/keil-build.ps1` and `scripts/vscode/jlink.ps1`.
- `net/MDK/network.uvprojx` uses an explicit source list. When adding or moving compiled files, update the real Keil project inputs instead of creating compatibility shims that leave the build graph unchanged.
- There is no established host-side automated test suite. Validate changes with at least a Keil build, and use flash/RTT or serial workflows when the task affects runtime or hardware behavior.

## Code And Docs Conventions

- Macros, `typedef` declarations, enum aliases, and function-pointer typedefs must live in `.h` files, never in `.c` files.
- Use `LOG_I`, `LOG_W`, and `LOG_E` for logs; do not use `printf`, `puts`, or `putchar` for logging or debug output.
- Keep embedded changes lightweight and direct. Avoid unnecessary abstraction layers, complex branching, or large refactors mixed with functional changes.
- New `.c` and `.h` files should follow the templates in `net/rep/newfile/`.
- When adding a new directory, add its authoritative Markdown document first and update the parent directory document to register the new child.
- Code comments should be in English. New Markdown documentation should default to Chinese.