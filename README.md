# PvZ-Portable

A **cross-platform** community-driven reimplementation of Plants vs. Zombies: Game of the Year Edition, aiming to bring the **100% authentic PvZ experience** to every platform.

| 🌿 Authentic | 🎮 Portable | 🛠️ Open |
| :---: | :---: | :---: |
| Almost 100% gameplay recreation | Run on Linux, Windows, Switch... | OpenGL & SDL |

## License

This project is licensed under the **[GNU Lesser General Public License v2.1](LICENSE) or later (LGPL-2.1-or-later)**.

* The code is provided "as is", **WITHOUT WARRANTY** of any kind.
* The **original game IP (Plants vs. Zombies) belongs to PopCap/EA**. This license applies **only to the code implementation** in this repository.

**⚠️ Notice:**

* This repository does **NOT** contain any copyrighted game assets (such as images, music, or levels) owned by PopCap Games or Electronic Arts. Users must provide their own `main.pak` and `properties/` folder from a **legally purchased copy** of Plants vs. Zombies: GOTY Edition.
* The codebase is based on a manual reimplementation based on publicly available reverse-engineering documentation and community research (such as [植物大战僵尸吧](https://tieba.baidu.com/f?ie=utf-8&kw=%E6%A4%8D%E7%89%A9%E5%A4%A7%E6%88%98%E5%83%B5%E5%B0%B8), [PVZ Wiki](https://wiki.pvz1.com/doku.php?id=home) and [PvZ Tools](https://pvz.tools/memory/)). It is written to utilize portable backends like SDL2 and OpenGL.
* This project is intended solely for **educational purposes**, focusing on **cross-platform porting techniques**, engine modernization, and learning how classic game logic can be adapted to various hardware architectures (e.g., Nintendo Switch, 3DS).
* Non-Commercial: This project is not affiliated with, authorized, or endorsed by PopCap Games or Electronic Arts.
* Most of the re-implementation code of the framework is contributed by [Patoke](https://github.com/Patoke/) and [Headshotnoby](https://github.com/headshot2017/).
* To play the game using this project you need to have access to the original game files by [purchasing it](https://www.ea.com/games/plants-vs-zombies/plants-vs-zombies)

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
| Windows (SDL2)  | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Linux (SDL2)    | Executable dir (resources); per-user app-data for writable files | Works                                                                                  |
| Haiku (SDL2)    | Executable dir (resources); per-user app-data for writable files | Partially works: no music                                                              |
| Nintendo Switch | sdmc:/switch/PlantsvsZombies | Works on real hardware and Citron. Kenji-NX crashes on boot.                           |
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

If you prefer to keep everything in the same folder as the executable, you can still configure a custom data directory via the `-changedir` command-line parameter when launching the game.

## Build Instructions

Run the following commands (assuming you have CMake and other dependencies installed) where the `CMakeLists.txt` file is located:

```bash
cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release
```

```bash
cmake --build build
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

- [@Headshotnoby](https://www.github.com/headshot2017) for fullly implementing the 64-bit and OpenGL backends support
- [@Patoke](https://www.github.com/Patoke) for the amazing reimplementation of PvZ GOTY
- [@rspforhp](https://www.github.com/octokatherine) for their amazing work decompiling the 0.9.9 version of PvZ
- [@ruslan831](https://github.com/ruslan831) for archiving the [0.9.9 decompilation of PvZ](https://github.com/ruslan831/PlantsVsZombies-decompilation)
- The GLFW team for their amazing work
- PopCap for creating the amazing PvZ franchise
- All the contributors which have worked or are actively working in this amazing project
