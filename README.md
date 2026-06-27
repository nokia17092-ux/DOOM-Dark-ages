# DOOM Dark Ages by Nokia1709

A DOOM-style FPS game with raycasting engine, WAD loader (like Chocolate Doom), 25 levels.

## Features

- **WAD Loader** - Load Freedoom, DOOM 1, DOOM 2 WAD files
- **25 Built-in Levels** - Castle-themed
- **Full HUD** - Health, Ammo, Weapons, Minimap
- **Nokia1709 Branding** - Custom title screen

## Quick Start

### Linux
```bash
chmod +x DOOMDarkAges
./DOOMDarkAges
```

### Windows
Use GitHub Actions to build (see below)

## Menu

- **[1]** Freedoom 1 (DOOM)
- **[2]** Freedoom 2 (DOOM II)
- **[3]** Built-in Levels
- **[ESC]** Exit

## Controls

- **WASD** - Move
- **Mouse/Arrows** - Look
- **Left Click** - Shoot
- **1-3** - Weapons
- **ESC** - Pause/Menu
- **S** - Save Game

## Build Windows EXE (Portable)

### Option 1: GitHub Actions (Recommended)
1. Upload this project to GitHub
2. Go to **Actions** tab
3. Click **Build Windows Executable**
4. Wait for build
5. Download `DOOMDarkAges.exe` from artifacts

### Option 2: Local Build
```bash
# On Windows
pip install pygame numpy pillow pyinstaller
pyinstaller main.py --onefile --name DOOMDarkAges --windowed --icon=doom_icon.ico --add-data "levels;levels" --add-data "assets;assets"
```

## Build from Source

```bash
pip install pygame numpy pillow pyinstaller
pyinstaller main.py --onefile --name DOOMDarkAges
```

## Credits

- Assets: Freedoom Project (freedoom1.wad, freedoom2.wad)
- Game by Nokia1709
