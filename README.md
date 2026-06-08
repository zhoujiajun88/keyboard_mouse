# KeyboardMouseMode

KeyboardMouseMode is a Windows tray utility for controlling the mouse with the keyboard.

Version: `1.1`

## Features

- Toggle keyboard mouse mode with a global shortcut
- Move the cursor with keyboard keys
- Simulate left click, right click, and wheel scrolling
- Configure cursor speed
- Start with Windows
- Switch UI language between Simplified Chinese, Traditional Chinese, and English
- Support normal and high-DPI displays

## Usage

| Key | Action |
| --- | --- |
| `Ctrl + Alt + M` | Turn mouse mode on / off |
| `Esc` | Turn mouse mode off |
| `W` / `A` / `S` / `D` | Move cursor |
| `J` | Left mouse button |
| `K` | Right mouse button |
| `Up` / `Down` | Vertical scrolling |
| `Left` / `Right` | Horizontal scrolling |

Tray icon:

- Left click: turn mouse mode on / off
- Right click: open menu

## Build

Requirements:

- Windows
- Visual Studio 2026
- MSVC C++ build tools
- Windows SDK

Build with Visual Studio:

1. Open `KeyboardMouseMode.sln`
2. Select `x64`
3. Build `Release`

Command-line build:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" KeyboardMouseMode.sln /p:Configuration=Release /p:Platform=x64 /m
```
