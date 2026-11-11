# Repository Guidelines

## Project Structure & Module Organization

This C++17/Qt 6 application configures the GAOMON 1060PRO on KDE Plasma Wayland. `main.cpp` contains the main window and KWin integration; the root-level headers implement mapping, pressure controls, shortcuts, and input state. `shortcut-helper.cpp` handles tablet input and virtual devices. Tests live in `tests/`, diagnostic utilities in `tools/`, the desktop launcher at the root, and icons in `assets/`. User guidance is in `README.md`; installation and architecture details are in `docs/DEVELOPMENT.md`.

## Build, Test, and Development Commands

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j2
ctest --test-dir build --output-on-failure
./build/gaomon-manage-panel
./build/gaomon-manage-panel --diagnose
```

Configure and build with CMake; run the full CTest suite before submitting changes. `--diagnose` reads the current platform and mapping without applying changes. Dependencies include Qt 6 Widgets and D-Bus, libevdev, pkg-config, and `dbus-run-session` for the isolated synchronization test. Installation instructions are in `docs/DEVELOPMENT.md`.

## Coding Style & Naming Conventions

Follow the surrounding C++ style: four-space indentation, `PascalCase` classes, `camelCase` functions and variables, and descriptive kebab-case filenames. Keep Qt-facing text consistent with the existing Chinese interface. No repository-wide formatter or linter is configured; keep changes focused and avoid unrelated reformatting.

## Testing Guidelines

Name tests `tests/*-test.cpp` and register them with CMake/CTest. Qt Test covers UI and D-Bus interactions; smaller logic tests use C++ assertions. Use Debug builds so assertions remain active. UI tests run offscreen, and the pressure synchronization test uses an isolated D-Bus session. No coverage threshold is configured. Validate device behavior with a real tablet when changing input handling; automated tests cannot establish hardware compatibility.

## Commits & Pull Requests

The repository has no commits yet, so no historical message convention is established. Use concise, imperative subjects such as `Add pressure curve presets`. Pull requests should explain the user-visible change, list verification performed, link relevant issues when available, and include screenshots for UI changes.

## Security & Configuration

The shortcut helper opens tablet devices through `pkexec` and creates virtual input devices. Keep device access narrowly scoped and do not require elevated privileges for the main window. Project code and documentation are MIT-licensed; the vendor icon has separate, unverified redistribution terms described in `assets/NOTICE.md`.
