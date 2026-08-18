# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
Full plan: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## Status

Phase 0 (Foundation) scaffolding. Builds a borderless, topmost, layered
window with no rendering yet — proves the toolchain and project structure
before Phase 1 (character rendering, drag, cursor tracking, petting).

## Prerequisites

- Visual Studio 2022+ with the "Desktop development with C++" workload
  (provides the MSVC toolset and Windows SDK)
- CMake 3.28+
- Git

Neither Visual Studio nor CMake is currently installed on this machine —
install both before the first build.

## Build

```
cmake -B build -S .
cmake --build build --config Debug
```

Or open the folder in Visual Studio (File > Open > Folder) for native
CMake integration.

## Structure

```
src/
  app/          entry point (WinMain)
  core/         logging, shared utilities
  window/       Win32 window management
  rendering/    Direct2D/Direct3D character rendering (Phase 1+)
  character/    character state (emotion, action, personality)
  behavior/     idle/behavior selection logic
  interaction/  mouse interaction, petting, hitboxes
  content/      character/item/furniture package loading
  items/        interactive props
  context/      desktop awareness, screen understanding
  ai/           AI engine integration (conversation, vision)
  memory/       session/daily/long-term memory
  audio/        STT/TTS
content/        character & item packages (data, not code)
assets/         shared assets
config/         runtime configuration
tests/          tests
```

See section 47 of the project plan for the rationale behind this layout,
and section 52 for the full phase roadmap (Phase 0–13).
