# DOOM Dark Ages

A C++17 first-person game built with SDL2. Its software 3D renderer projects textured world geometry with perspective-correct texture mapping and a per-pixel depth buffer. It loads the repository's 25 JSON maps and indexed Doom-style flat textures.

## Play

- Arrow keys: choose a level in the menu
- Enter: start or resume
- WASD: move
- Mouse: look around
- Left mouse button: shoot
- Escape: pause, return to the menu, or quit
- R: restart after defeat

## Build on Linux

Requirements: CMake 3.20+, a C++17 compiler, and network access on the first configure (SDL2 and nlohmann/json are fetched at pinned versions).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/DOOMDarkAges
```

CMake copies the maps and game assets beside the executable.

## Windows x64 build

Push to `main`, or start the **Build C++ Game** workflow manually in GitHub Actions. It builds on a Windows x64 runner and publishes `DOOMDarkAges-Windows-x64.zip` with the executable, SDL2 runtime, maps, and assets. This avoids requiring a Windows cross-compiler in the Linux development container.

The workflow runs `DOOMDarkAges.exe --render-smoke` before packaging to verify the 3D framebuffer, depth buffer, and loaded maps.

## LibreQuake status

LibreQuake is not currently supported by this game. Its Quake PAK/BSP data and QuakeC game logic need a compatible Quake engine; this C++ raycaster currently reads the project's JSON maps and extracted Doom-style textures. The current build is a playable C++ foundation, not a LibreQuake port.
