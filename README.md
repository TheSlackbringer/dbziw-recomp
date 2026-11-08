<h1 align="center">DBZ: Infinite World Recomp</h1>

<p align="center"><b>Dragon Ball Z: Infinite World (PS2) as a native PC game, built on your PC from your own disc.</b></p>

<p align="center">
  <a href="https://github.com/TheSlackbringer/dbziw-recomp/actions/workflows/build.yml"><img alt="build" src="https://img.shields.io/github/actions/workflow/status/TheSlackbringer/dbziw-recomp/build.yml?branch=main&label=build&logo=github"></a>
  <img alt="platform: Windows | Linux" src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux-0a7bbb">
  <img alt="graphics: Vulkan" src="https://img.shields.io/badge/graphics-Vulkan-ac162c">
  <a href="LICENSE"><img alt="license: GPL-3.0" src="https://img.shields.io/badge/license-GPL--3.0-6a9a1f"></a>
  <img alt="status: playable (beta)" src="https://img.shields.io/badge/status-playable%20(beta)-e6a200">
  <img alt="disc: PAL SLES-55347" src="https://img.shields.io/badge/disc-PAL%20SLES--55347-555">
</p>

A native PC port of **Dragon Ball Z: Infinite World** for the PlayStation 2, built with PS2Recomp: the game's code is translated into C++ and runs natively, with graphics on **Vulkan through paraLLEl-GS**. Windows and Linux releases build the game on your PC from your own disc.

> [!IMPORTANT]
> You need your own legal copy of **Dragon Ball Z: Infinite World** (European PAL, `SLES_553.47`). Nothing from the game is in this repository or in its releases: no disc images, game executables, IRX modules, AFS archives, SFD videos, game assets or translated game code.

## Status

**Beta: playable.** The game boots, and its menus, music, sound effects, movies and Dragon Duel fights run; the first Dragon Mission has been reached, and the game creates its save on the memory card. It has not been played from start to finish, so expect rough edges: report them in [Issues](https://github.com/TheSlackbringer/dbziw-recomp/issues).

**Known issues:**

- The opening movie can stall now and then on Windows.
- Two-player play and arcade fightsticks are not finished.
- Long play sessions have not been tested.
- There is no in-game settings menu yet: use the function keys below.

## What you need

- **Dragon Ball Z: Infinite World**, European PAL (`SLES_553.47`), as an `.iso`.
- A 64-bit CPU with AVX2 (Intel since 2013, AMD Ryzen) and a graphics card with a current Vulkan driver; about 10 GB of free space.
- Windows 10 (1903 or newer) / 11, or Linux with glibc 2.31 or newer.

Other regions and revisions are not supported (every file is checked against known checksums).

## Playing

Download the latest release from the [Releases](https://github.com/TheSlackbringer/dbziw-recomp/releases) page. It contains our program, the source code, a compiler and the tools; nothing else needs to be installed and nothing is downloaded while it builds.

- **Windows:** unzip `DBZIW-<version>-windows-x64.zip` and start **`DBZIW.exe`**. It is not signed: if Windows says *"Windows protected your PC"*, choose **More info → Run anyway**.
- **Linux:** make `DBZIW-RECOMP-<version>-x86_64.AppImage` executable and open it. The file and folder pickers need `zenity` or `kdialog`; without them, type the paths.

**First start:** choose your disc image and press **Continue**, choose where to install it (`C:\Games\DBZIW-Recomp` or `~/Games/DBZIW-Recomp` is filled in), and press **Install & Play**. The disc is checked, and the game is translated and compiled for your PC: a few minutes, once, longer on older PCs. You can then delete the download. Every later start of the installed `DBZIW.exe` or `DBZIW.AppImage` opens the game directly.

**Updating:** open the new release. It finds your installation and offers **Update & Play**, without asking for the disc again, and keeps your saves.

**Saves and settings:** the install page lets you keep them on this PC (recommended) or with the game (portable).

| | Saves | Settings |
| --- | --- | --- |
| **On this PC, Windows** | `%LOCALAPPDATA%\DBZIW\user-data` | `%LOCALAPPDATA%\DBZIW\config` |
| **On this PC, Linux** | `~/.local/share/dbziw/user-data` | `~/.config/dbziw/config` |
| **Portable** | `dbziw-data/user-data` in the install folder | the same folder |

**Manage installation:** hold Shift while starting `DBZIW.exe`, or start `DBZIW.exe` or the AppImage with `--manage`. Close the game first. From there you can change where saves live, copy the installation to another drive or PC, or uninstall the game and choose whether to keep your saves.

If something goes wrong, please attach the files from the `dbziw-data/logs` folder in your install folder to an issue.

## Controls

| Keyboard | PS2 input |
| --- | --- |
| Arrow keys | D-pad |
| W A S D | Left stick (free flight ignores the D-pad) |
| X or Space / C or Esc | Cross / Circle |
| Z or Numpad 0 / V or Numpad 1 | Square / Triangle |
| Q / E | L1 / R1 |
| Left Shift / Right Shift | L2 / R2 |
| Enter / Tab | Start / Select |

Gamepads work too.

### Function keys

| Key | Description |
| --- | --- |
| **F1** | Runtime debugger, for developers. |
| **F2** | Show or hide the build ID and game tick (for bug reports). |
| **F3** | Show or hide the frame rate. |
| **F4** | Cycle the resolution (1× → 2× → 4×). |
| **F5** | Cycle anti-aliasing (1× → 2× → 4× → 8× → 16×). |

The resolution and anti-aliasing choices are remembered in `dbziw.ini`, in your settings folder.

### 50 or 60 Hz

The game starts in its own **60 Hz** mode, which suits most PC monitors. To play at the PAL disc's original **50 Hz**,
add the line `video_mode=50` to `dbziw.ini` in your settings folder (see the table above; create the file if it is not
there yet), then start the game again. Change it to `video_mode=60`, or remove it, to go back to 60 Hz.

The disc's own hidden switch works too, while the game is at 50 Hz: on the language screen, hold **L1 + L2 + R1 + R2**
(Q, E, Left Shift and Right Shift) until *"Would you like to change NTSC/PAL mode?"* appears, then choose **Yes** (Up,
then Cross). This switches only the running game; `dbziw.ini` decides how it starts.

## Building from source

See [BUILDING.md](BUILDING.md) for building on Linux from source, the developer tools and making a release.

## License and attribution

The port's own source and tools are licensed under GPL-3.0; see [LICENSE](LICENSE). This license grants no rights to the original game or its assets.

The vendored [PS2Recomp](https://github.com/ran-j/PS2Recomp) runtime and recompiler are modified versions of the upstream project by ran-j and contributors. Revision and maintenance details are in [DBZIW_VENDORED.md](external/PS2Recomp/DBZIW_VENDORED.md). Third-party code retains its applicable licenses; see [THIRD_PARTY.md](THIRD_PARTY.md) and the accompanying notices.

Dragon Ball, Dragon Ball Z, and related characters are trademarks and copyrights of Bird Studio / Shueisha / Toei Animation. Dragon Ball Z: Infinite World is developed by Dimps and published by Atari / Bandai Namco Games. This non-profit fan preservation project is not affiliated with, endorsed by, or sponsored by Dimps, Atari, Bandai Namco Games, Bird Studio, Shueisha, or Toei Animation.
