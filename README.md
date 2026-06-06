# KeyboardMouseMode

KeyboardMouseMode is a lightweight Windows tray utility that lets you control the mouse with the keyboard.

Version: `1.0`

## Features

- Toggle mouse mode with `Ctrl + Alt + M`
- Exit mouse mode with `Esc`
- Move the cursor with `W` / `A` / `S` / `D`
- Use `J` as the left mouse button
- Use `K` as the right mouse button
- Use arrow keys for vertical and horizontal scrolling
- Tray icon with on/off state
- Settings window for startup and cursor speed
- Optional start with Windows
- Center-screen one-second status toast
- Focus-friendly toast window that does not steal input focus

## Controls

| Key | Action |
| --- | --- |
| `Ctrl + Alt + M` | Toggle mouse mode |
| `Esc` | Turn mouse mode off |
| `W` / `A` / `S` / `D` | Move cursor |
| `J` | Left mouse button |
| `K` | Right mouse button |
| `Up` / `Down` | Vertical scrolling |
| `Left` / `Right` | Horizontal scrolling |

## Movement Curve

Cursor movement starts with a small precision phase, then ramps up according to the configured speed.

For the default `800 px/s` speed:

| Held time | Speed |
| --- | --- |
| `0-20ms` | Initial 1px nudge only |
| `100ms` | `40 px/s` (`5%`) |
| `200ms` | `80 px/s` (`10%`) |
| `300ms` | `160 px/s` (`20%`) |
| `400ms` | `400 px/s` (`50%`) |
| `400ms+` | `800 px/s` (`100%`) |

Other configured speeds use the same percentages.

## Tray

- Left click: toggle mouse mode
- Right click: open menu
- Menu:
  - Toggle mouse mode
  - Settings
  - Start with Windows
  - Exit

## Settings

The settings window includes:

- Mouse movement speed in `px/s`
- Start with Windows
- Version information
- Built-in usage instructions

Configuration is saved under:

```text
%APPDATA%\KeyboardMouseMode\config.ini
```

Start with Windows is stored in the current-user Run key:

```text
HKCU\Software\Microsoft\Windows\CurrentVersion\Run
```

## Build

Requirements:

- Windows
- Visual Studio 2026
- MSVC C++ build tools
- Windows SDK

Open `KeyboardMouseMode.sln` in Visual Studio 2026 and build the `x64` configuration.

Command-line build example:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" KeyboardMouseMode.sln /p:Configuration=Release /p:Platform=x64 /m
```

The project uses:

- C++17
- Pure Win32 API
- Platform toolset `v145`

## Project Structure

```text
.
├── KeyboardMouseMode.sln
├── KeyboardMouseMode.vcxproj
├── src/
│   ├── main.cpp
│   ├── app.rc
│   ├── resource.h
│   └── assets/
│       ├── app.ico
│       ├── tray_off.ico
│       └── tray_on.ico
├── tools/
│   └── generate_icons.py
└── DESIGN.md
```

## Notes

- Keyboard hooks and mouse simulation may be flagged by some security tools because that behavior is inherently sensitive.
- If the target app is running as administrator, this tool may also need to run as administrator to affect that app.
- Some games, remote desktop sessions, and virtual machines may block or ignore simulated input.
