# Demon's Souls Port PC

**Demon's Souls (PS3), recompiled to run natively on PC.**

![Status](https://img.shields.io/badge/status-experimental-orange) ![Platform](https://img.shields.io/badge/platform-Windows-blue) ![License](https://img.shields.io/badge/license-MIT-green)

[Português (Brasil)](README.md) · English

This project translates the game's code, both the PS3's main processor (PPU) and its SPUs, into C++ and compiles it for your PC, on top of the [ps3recomp](https://github.com/sp00nznet/ps3recomp) runtime. It is not an emulator: it is the game itself, recompiled.

> [!WARNING]
> This is an experiment. The game boots and the tutorial is playable, but you can't play it start to finish yet, and nothing past the tutorial has been tested. Windows only.

> [!IMPORTANT]
> **No game data is included**: no code, no disc files, no PS3 firmware. You need your own copy, the US release **BLUS30443 (1.00)**, dumped from your own disc, and the official PS3 firmware installed in RPCS3. Nobody here can give you the game, the firmware or keys. Fan project, not affiliated with Sony Interactive Entertainment, FromSoftware or Bluepoint.

## Where we are

October 2026, on Windows:

- **Boot to tutorial**: notices, main menu, character creation, opening cinematic and the tutorial (corridor, courtyard, fighting the hollows, the tower). Saves work.
- **Its own Direct3D 12 renderer** (ps3recomp's "live draw" with this project's changes): shadows, fog, soft particles, eye adaptation, depth of field, and occlusion for the sun and lens flares.
- **60 fps**: RPCS3's "Unlock FPS" patch (Gibbed and Whatcookie), applied as code. The tutorial corridor holds ~59 fps and the courtyard fight 46-59 fps. The stock 30 fps mode (`-Fps30`) stays locked at 30.
- **Video cinematics** (`.pam`), played through [ffmpeg](https://ffmpeg.org) with sound: the opening, the title screen and the story intro.
- **Audio** complete and clean: music, voices and effects (MultiStream + ATRAC, using the codec from your firmware).
- **The real SPURS kernel** (`libsre`, from your firmware) runs recompiled rather than approximated.
- **Known issues**: fights with many enemies drop into the 40s, because the limit is the recompiled SPU code, not the GPU. Some particles may be missing, and the sky sometimes blows out in outdoor areas.

The full technical diary (in Portuguese), with every problem found and how it was solved: [docs/REPORT.md](docs/REPORT.md).

## What you need

- **Windows 10 or 11** and a Direct3D 12 GPU. Tested on a Radeon RX 7600.
- **The game**: your **Demon's Souls (USA), BLUS30443** disc, extracted and decrypted. A `PS3_GAME` folder from [ps3-disc-dumper](https://github.com/13xforever/ps3-disc-dumper) works as is. For an encrypted Redump ISO, decrypt it first with PS3Dec and your disc's key (`.dkey`), then extract it.
- **[RPCS3](https://rpcs3.net) with the official PS3 firmware installed** (`PS3UPDAT.PUP` from Sony's site). It decrypts `EBOOT.BIN` and `libsre.sprx`, and its `dev_flash` folder serves the audio codec.
- **To build**:
  - Visual Studio 2022 or newer with "Desktop development with C++" and the **"C++ Clang tools for Windows"** component (clang-cl);
  - Python 3.10+ and Git on PATH;
- **[ffmpeg](https://ffmpeg.org)** on PATH (`winget install Gyan.FFmpeg`) or in `third_party\ffmpeg\bin`, for the movies. Without it the game skips its cinematics.
  - ~20 GB free and **16 GB of RAM** or more. The generated code is ~2.4 GB, and a build from scratch takes 30-60 minutes.

## Building

```
demons-souls-port-pc/
  port/          the program (main, game bridge, CMake)
  lle/           the bridge to the firmware's SPURS kernel
  patches/       this project's changes to ps3recomp
  scripts/       preparation, build, play and tests
  docs/          technical diary, environment variables, automated tests
  third_party/   patched ps3recomp (created by setup; not in git)
  work/          your disc and everything made from it (not in git, never upload it)
```

In PowerShell, from the repository folder:

1. **Set up** (once). Clones ps3recomp at the commit used, applies `patches/ps3recomp.patch` and creates the Python environment:

   ```powershell
   .\scripts\windows\setup.ps1
   ```

2. **Put the disc** in `work\disc`, so that `work\disc\PS3_GAME\USRDIR\EBOOT.BIN` exists.

3. **Generate the code**. Decrypts `EBOOT.BIN` and `libsre.sprx` with your RPCS3, translates the game's and the SPURS kernel's code, and extracts the SPU programs (a few minutes):

   ```powershell
   .\scripts\windows\prepare.ps1 -Rpcs3 "C:\path\to\rpcs3"
   ```

4. **Build**:

   ```powershell
   .\scripts\windows\build.ps1          # -Jobs 6 if memory runs short
   ```

## Playing

```powershell
.\scripts\windows\play.ps1 -Rpcs3 "C:\path\to\rpcs3"   # first time (remembered)
.\scripts\windows\play.ps1                              # afterwards
.\scripts\windows\play.ps1 -Fps30                       # the stock game at 30 fps
.\scripts\windows\play.ps1 -Uncapped                    # no 60 fps cap (experimental)
.\scripts\windows\play.ps1 -Resolution 1080p            # bigger window: 1080p, 1440p, 4k or WxH
```

**Controller**: an XInput controller (Xbox or compatible) works out of the box, with its buttons in the PS3 controller's positions.

**Keyboard** (used when no controller is connected):

| Key | Button | Key | Button |
|---|---|---|---|
| Arrows | move / d-pad | Enter | Start |
| Z | ✕ | Tab | Select |
| X | ◯ | Q / W | L1 / R1 |
| A | □ | 1 / 2 | L2 / R2 |
| S | △ | | |

The camera only turns with a controller for now.

Saves go to `work\gamedata\dev_hdd0` and logs to `work\logs`.

## Automated tests

`scripts\windows\autotest.ps1` runs the game for a fixed time with scripted input (buttons and sticks, no one playing), measures fps and keeps the log, so a change can be tested along the same route every time. See [docs/AUTOTEST.md](docs/AUTOTEST.md). Every environment variable (diagnostics, renderer, audio) is in [docs/ENVIRONMENT.md](docs/ENVIRONMENT.md).

## Credits

- [ps3recomp](https://github.com/sp00nznet/ps3recomp), by sp00nznet and contributors: the recompiler, runtime and system libraries. This project is a set of changes on top of it (`patches/`).
- [RPCS3](https://rpcs3.net): the reference for PS3 behaviour, decryption, and the firmware you install in it. The BLUS30443 "Unlock FPS" patch is by Gibbed and Whatcookie.
- [Ascension](https://github.com/arnaldolima1/gow-ascension-pc) (God of War: Ascension on PC, same runtime): several SPU fixes came from there.
- Demon's Souls is a trademark of Sony Interactive Entertainment. Developed by FromSoftware.

## License

MIT: see [LICENSE](LICENSE). It covers only the code in this repository, not the game. ps3recomp has its own license (MIT).
