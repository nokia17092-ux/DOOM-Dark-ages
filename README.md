# DOOM Dark Ages

A small C++ launcher for the full 3D vkQuake engine and the free LibreQuake campaign. The portable Windows build includes animated 3D monsters, maps, weapons, sound effects, music, and the required runtime libraries.

## Play

- Choose **PLAY CAMPAIGN** in the launcher to start the first LibreQuake campaign level.
- In game: WASD/mouse, fire with the mouse, Escape opens the game menu.

## Build on Linux

Requirements: CMake 3.20+, a C++17 compiler, and network access on the first configure (SDL2 is fetched at a pinned version).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/DOOMDarkAges
```

To play on Linux, place a compatible vkQuake binary and its runtime libraries beside the launcher. The Windows portable package is built by GitHub Actions.

## Windows x64 build

Push to `main`, or start the **Build C++ Game** workflow manually in GitHub Actions. It builds the C++ launcher on a Windows x64 runner, bundles vkQuake 1.37.0 and LibreQuake Lite v0.09-beta, verifies the portable install, and publishes `DOOMDarkAges-Windows-x64.zip`.

## LibreQuake status

LibreQuake assets are in `assets/librequake/librequake-lite-v0.09-beta.zip`. The engine is vkQuake (GPL-2.0); LibreQuake assets have the licenses noted in `id1/docs/COPYING`, `id1/docs/CREDITS`, and `id1/docs/README-IMPORTANT-LICENCE-INFO`. The portable package also contains source archives for vkQuake and LibreQuake, plus the vkQuake license. Original commercial DOOM: The Dark Ages assets are not included.
