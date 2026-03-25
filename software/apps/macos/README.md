# macOS Application

This directory is the Roberta desktop shell client. The UI still consumes the
shared application shell from `../foundry/frontend`, but it now runs as a real
desktop app with a native backend:

- React/Vite frontend for the workspace UI
- Tauri host in `src-tauri/`
- Rust command layer for desktop IPC and app state
- C++ bridge library over `software/core`
- real serial-console axis control through the shared core stack

The desktop app is no longer backed by a TypeScript mock controller in runtime.

## Shell Package

This app uses the shell package documented in the sibling frontend workspace:

- package: `@yourorg/app-shell`
- peer package: `@yourorg/ui`
- peer dependencies: `react`, `react-dom`
- optional icon dependency used here: `lucide-react`

The shell is consumed as packaged artifacts, not by importing source files from
the shell workspace.

## Installation

First build and pack the shared shell packages from `../foundry/frontend`:

```sh
cd ../foundry/frontend
pnpm --filter @yourorg/ui build
pnpm --filter @yourorg/app-shell build
pnpm pack:ui
pnpm pack:app-shell
```

Then install the macOS app dependencies:

```sh
cd ../roberta/software/apps/macos
pnpm install
```

`package.json` pins the shell and UI packages to the generated local tarballs:

- `../../../../foundry/frontend/yourorg-ui-0.1.0.tgz`
- `../../../../foundry/frontend/yourorg-app-shell-0.1.0.tgz`

## Consumer Setup

The app follows the shell package's documented consumer contract:

- `ThemeProvider` from `@yourorg/ui` is mounted in `src/App.tsx`
- `AppShell` is mounted in `src/App.tsx`
- shell regions are supplied through public exports only:
  `AppHeader`, `AppSidebar`, `AppContent`, `SettingsPanel`,
  `ShellSettings`, `ShellPanelLayout`, and `StatusBar`
- packaged CSS is loaded in `src/main.tsx`:
  `@yourorg/ui/styles.css` and `@yourorg/app-shell/styles.css`
- shell persistence uses `storageNamespace="roberta"`
- theme persistence uses `storageKey="roberta-theme"`

This repo does not copy shell source or import shell internals.

## Backend Integration

The desktop backend is split across the native host and the shared C++ core:

- Rust/Tauri host: `src-tauri/src/main.rs`
- Rust workspace/backend state: `src-tauri/src/backend.rs`
- C++ FFI facade over the core axis stack:
  `software/core/include/roberta/ffi/axis_client.h`
  and `software/core/src/ffi/axis_client.cpp`

The bridge talks to real controllers through the existing shared core path:

`Tauri -> Rust backend -> C ABI facade -> ControllerAxisDevice / ConsoleRegisterTransport -> serial console firmware`

Robot logic stays in `software/core`. Rust is desktop glue, not a second robot
implementation.

## Frontend Service Boundary

Robot-specific UI state and service wiring stay in this app:

- state hook: `src/hooks/use-roberta-workspace.ts`
- domain models: `src/models/robot.ts`
- live service client: `src/services/live-robot-service.ts`
- register grouping/presentation mapping: `src/services/register-groups.ts`

The UI reads from the hook and sends actions through the service layer.
Components do not call controller APIs directly.

## Local Run

Run the desktop app with the native backend:

```sh
cd software/apps/macos
pnpm dev
```

If you only need the frontend bundle:

```sh
cd software/apps/macos
pnpm build
```

To check the Rust/Tauri host without launching the desktop window:

```sh
cd software/apps/macos
pnpm desktop:check
```

## Verified Behavior

The current local verification covers:

- `pnpm typecheck`
- `pnpm test`
- `pnpm build`
- `pnpm desktop:check`

The UI test suite verifies that the shell client:

- renders the operator workspace with all 6 axes visible
- updates the axis workspace in place when selecting and jogging an axis
- opens register inspection without losing axis context
- keeps shell shortcuts and the app command palette active

Hardware verification was not performed in this session. The desktop path is now
wired to the real backend and ready to be exercised against actual serial
controllers from the workspace settings surface.
