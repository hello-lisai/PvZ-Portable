# PvZ-Portable

[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/wszqkzqk/PvZ-Portable)

A **cross-platform** community-driven reimplementation of Plants vs. Zombies: Game of the Year Edition, aiming to bring the **100% authentic PvZ experience** to every platform.

| 🌿 Authentic | 🎮 Portable | 🛠️ Open |
| :---: | :---: | :---: |
| Almost 100% gameplay recreation | Run on Linux, Windows, macOS, Switch... | OpenGL & SDL |

## License

This project is licensed under the **[GNU Lesser General Public License v2.1](LICENSE) or later (LGPL-2.1-or-later)**.

* The code is provided "as is", **WITHOUT WARRANTY** of any kind.
* The **original game IP (Plants vs. Zombies) belongs to PopCap/EA**. This license applies **only to the code implementation** in this repository.

**⚠️ Notice:**

* This repository does **NOT** contain any copyrighted game assets (such as images, music, or levels) owned by PopCap Games or Electronic Arts. Users must provide their own `main.pak` and `properties/` folder from a **legally purchased copy** of Plants vs. Zombies: GOTY Edition.
* The codebase is a manual reimplementation derived from publicly available reverse-engineering documentation and community research (such as [植物大战僵尸吧](https://tieba.baidu.com/f?ie=utf-8&kw=%E6%A4%8D%E7%89%A9%E5%A4%A7%E6%88%98%E5%83%B5%E5%B0%B8), [PVZ Wiki](https://wiki.pvz1.com/doku.php?id=home) and [PvZ Tools](https://pvz.tools/memory/)). It is written to utilize portable backends like SDL2 and OpenGL.
* This project is intended solely for **educational purposes**, focusing on **cross-platform porting techniques**, engine modernization, and learning how classic game logic can be adapted to various hardware architectures (e.g., Nintendo Switch, 3DS).
* Non-Commercial: This project is not affiliated with, authorized, or endorsed by PopCap Games or Electronic Arts.
* Most of the previous re-implementation code of the framework is contributed by [Patoke](https://github.com/Patoke/) and [Headshotnoby](https://github.com/headshot2017/).
* Project icons and platform-specific logos are carried over from the original fork by [Headshotnoby](https://github.com/headshot2017/).
* To play the game using this project you **MUST** have access to the original game files by purchasing it on [EA's official website](https://www.ea.com/games/plants-vs-zombies/plants-vs-zombies) or [Steam](https://store.steampowered.com/app/3590/Plants_vs_Zombies_GOTY_Edition/).

## Features

This is a **fork** of [Patoke](https://github.com/Patoke/re-plants-vs-zombies) and [Headshotnoby](https://github.com/headshot2017/re-plants-vs-zombies)'s PVZ GOTY implementation with the following objectives:
- [x] Replace renderer with SDL + OpenGL
  - Also enable to **resize the window**, which was not possible in the original game
- [x] Replace Windows code with cross-platform code
- [x] Replace DirectSound/BASS/FMod with [SDL Mixer X](https://github.com/WohlSoft/SDL-Mixer-X)
  * This project uses a fork of SDL Mixer X that adds compatibility with the MO3 format by using libopenmpt. This fork is located under SexyAppFramework/sound/SDL-Mixer-X
- [x] main.pak support
- [x] Optimize memory usage for console ports (Partial)
* Port the game to these platforms:

| Platform        | Data path                    | Status                                                                                 |
|-----------------|------------------------------|----------------------------------------------------------------------------------------|
| Linux (SDL2)    | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Windows (SDL2)  | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| macOS (SDL2)    | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Haiku (SDL2)    | Executable dir (resources); per-user app-data for writable files | Partially works: no music                                                              |
| Nintendo Switch | sdmc:/switch/PlantsvsZombies | Works on real hardware. Kenji-NX crashes on boot.                           |
| Nintendo 3DS    | sdmc:/3ds/PlantsvsZombies    | In development, might not have enough memory for Old 3DS, might barely work on New 3DS |
| Nintendo Wii U  |                              | No work started yet, but planned                                                       |

To play the game, you need the game data from PvZ GOTY. Place `main.pak` and the `properties/` folder next to the `pvz-portable` executable (the game will search for resources relative to the executable's directory). You can also use extracted data instead of `main.pak` if you prefer.

Note about writable data and caches:

- The game will read resources (like `main.pak` and `properties/`) from the executable directory by default, so you can launch the binary from any working directory and it will still find them.
- Per-user writable files (settings, savegames, compiled caches, screenshots) are stored in the **OS-recommended application data path**. With the current build these are under `io.github.wszqkzqk/PlantsVsZombies` and include subfolders such as:
  - `userdata/` — player save files
  - `compiled/` — compiled binary caches (reanimation / compiled definitions)
  - `registry.regemu` — settings/registry emulation
  - `popcinfo.dat` — basic runtime stats
  - `_screenshots/` — screenshots created by the game

Examples:

- Linux: `~/.local/share/io.github.wszqkzqk/PlantsVsZombies/`
- Windows: `%APPDATA%\io.github.wszqkzqk\PlantsVsZombies\`
- macOS: `~/Library/Application Support/io.github.wszqkzqk/PlantsVsZombies/`

If you prefer to keep everything in the same folder as the executable, you can still configure a custom data directory via the `-changedir` command-line parameter when launching the game.

## Dependencies

Before building on PC, ensure you have the necessary dependencies installed:

- **Build Tools**: `CMake`, `Ninja`, A C/C++ compiler (e.g., `gcc`, `clang`, `MSVC`)
- **Graphics**: `GLEW`, `OpenGL`
- **Audio**: `libopenmpt`, `libogg`, `libvorbis`, `mpg123`
- **Image**: `libpng`, `libjpeg-turbo`
- **Windowing/Input**: `SDL2`

### Arch Linux

You can install the required dependencies using the following command:

```bash
sudo pacman -S --needed base-devel cmake glew libjpeg-turbo libogg libopenmpt libpng libvorbis mpg123 ninja sdl2-compat
```

### Debian/Ubuntu

You can install the required dependencies using the following command:

```bash
sudo apt install cmake ninja-build libogg-dev libglew-dev libjpeg-dev libopenmpt-dev libpng-dev libvorbis-dev libmpg123-dev libsdl2-dev
```

### Windows (MSYS2 UCRT64)

You can install the required dependencies using the following command:

```bash
pacman -S --needed base-devel mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-glew mingw-w64-ucrt-x86_64-libjpeg-turbo mingw-w64-ucrt-x86_64-libopenmpt mingw-w64-ucrt-x86_64-libogg mingw-w64-ucrt-x86_64-libpng mingw-w64-ucrt-x86_64-libvorbis mingw-w64-ucrt-x86_64-mpg123 mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-SDL2
```

### macOS (Homebrew)

You can install the required dependencies using [Homebrew](https://brew.sh/) with the following command:

```bash
brew install cmake dylibbundler glew jpeg-turbo libogg libopenmpt libpng libvorbis mpg123 ninja sdl2
```

## Build Instructions

Run the following commands (assuming you have CMake and other dependencies installed) where the `CMakeLists.txt` file is located:

```bash
cmake -G Ninja -B build
```

```bash
cmake --build build
```

### Performance Optimization

It is recommended to use the **Release** build type for the best performance, as it usually implies compiler optimizations:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
```

For advanced users, you can also specify `CFLAGS` and `CXXFLAGS` to enable architecture-specific optimizations (e.g. `-march=native` to fully utilize your CPU's instruction set):

```bash
cmake -G Ninja -B build -DCMAKE_C_FLAGS="-march=native" -DCMAKE_CXX_FLAGS="-march=native" -DCMAKE_BUILD_TYPE=Release
```

### Configuration Options

You can customize the game features by adding options to the first `cmake` command:

| Option | Default | Description |
| :--- | :--- | :--- |
| `PVZ_DEBUG` | `OFF`<br>(`ON` if `CMAKE_BUILD_TYPE` is `Debug`) | Enable **cheat keys**, debug displays and other debug features. |
| `LIMBO_PAGE` | `ON` | Enable access to the limbo page which contains hidden levels. |
| `DO_FIX_BUGS` | `OFF` | Apply community fixes for "bugs" of official 1.2.0.1073 GOTY Edition.[^1] These "bugs" usually be ***considered as "features"*** by many players, though. |
| `CONSOLE` | `OFF`<br>(`ON` if `CMAKE_BUILD_TYPE` is `Debug`) | Show a console window (Windows only). |

[^1]: Current `DO_FIX_BUGS` includes the following fixes:
    - Fix bungee zombie duplicate sun/item drop in I, Zombie mode.
    - Mind-controlled Gargantuars smash enemy zombies instead of plants.
    - Mind-controlled Gargantuars throw mind-controlled Imps (with scale, health, and direction fixes).
    - Mind-controlled Gargantuars can smash vases in Scary Potter mode.
    - Mind-controlled Pea/Gatling Head zombies shoot forward instead of backward.
    - Mind-controlled Jalapeno/Squash zombies damage enemy zombies instead of plants.
    - Coordinate fixes for mind-controlled Squash zombies tracking and smashing enemy zombies.
    - Jalapeno zombies correctly clear Dr. Zomboss' skills (Iceball/Fireball) and ladder logic.
    - Sync Dancer Zombie animations (fixes "Maid" displacement bug).
    - Fix visual glitch of Ladder Zombie's arm recovery.
    - Fix Dr. Zomboss' attack (RV, Fireball/Iceball) and summon range coverage for 6-lane (Pool) levels.

Example: Manually enable `PVZ_DEBUG` in **Release build** so that you can use **cheat keys** while having optimized performance:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DPVZ_DEBUG=ON
```

If running these commands does not create a successful build please create an issue and detail your problem.

## Contributing

When contributing please follow the following guides:

<details><summary>SexyAppFramework coding philosophy</summary>

### From the SexyAppFramework docs:

<br>
The framework differs from many other APIs in that some class properties are not wrapped in accessor methods, but rather are made to be accessed directly through public member data.   The window caption of your application, for example, is set by assigning a value to the std::string mTitle in the application object before the application’s window is created.  We felt that in many cases this reduced the code required to implement a class.  Also of note is the prefix notation used on variables: “m” denotes a class member, “the” denotes a parameter passed to a method or function, and “a” denotes a local variable.
</br>
</details>

## Thanks

- **[@Headshotnoby](https://www.github.com/headshot2017)**: For fully implementing the 64-bit support and OpenGL backend.
- **[@Patoke](https://www.github.com/Patoke)**: For the incredible initial reimplementation of PvZ GOTY.
- **[@rspforhp](https://www.github.com/octokatherine)**: For the 0.9.9 version's work.
- **[@ruslan831](https://github.com/ruslan831)**: For archiving the 0.9.9 version's re-implementation.
- **The SDL Team**: For the amazing cross-platform development library that powers this port.
- **The GLFW Team**: For their work on the windowing library used in the original re-implementation.
- **The OpenMPT Team**: For libopenmpt, enabling high-quality MO3 music playback.
- **PopCap Games**: For creating the amazing PvZ franchise.
- All the contributors who have worked or are actively working in this amazing project.
