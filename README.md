# Sveta Assistant

Windows Desktop AI Companion / Interactive Character Platform.
Full plan: `docs/Sveta_Assistant_Integrated_Project_Plan.docx`.

## Status

Phase 0 (Foundation) and Phase 1 (Desktop Character) done: borderless,
topmost, per-pixel-transparent window rendering a PNG sprite (via WIC),
draggable, with position persisted to `%LOCALAPPDATA%\SvetaAssistant\`.

The sprite at `content/characters/sveta/assets/calm.png` is a placeholder
(plain shape, no real character art) — swap it for real art at the same
path once available. Next up: Phase 2 (cursor tracking, hitboxes,
petting detection).

Known follow-up: the app has no DPI-awareness manifest yet, so its own
view of window coordinates can diverge from what DPI-aware external
tools report. Doesn't affect the app's own save/restore round-trip;
worth fixing before Phase 13 polish.

## Prerequisites

- Visual Studio 2022+ with the "Desktop development with C++" workload
  (provides the MSVC toolset and Windows SDK)
- CMake 3.28+
- Git

Both are installed on this machine (VS 2026 Community, CMake 4.4.2).

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
  core/         logging, filesystem paths, shared utilities
  window/       Win32 window management, drag, position persistence
  rendering/    PNG sprite loading (WIC); Direct2D/Direct3D land later
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
