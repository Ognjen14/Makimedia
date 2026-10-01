# Makimedia

**Your film library, on every screen.**

Makimedia is a free, open source video player for Windows, Android phones and tablets, and Android TV. It turns the video folders you already have into a library with posters, details and progress, and streams it from your PC to your other devices over your home network.

No account, no ads, no subscription, no tracking.

> **Makimedia is a video player, not a streaming service.** It has no catalogue of its own and doesn't provide, sell, host or stream any films or shows. It plays video files you already have, and streams them only between your own devices on your own network. Only play and stream videos you have the right to use.

## Features

- **Your files come first.** The library is built from your folders and works with the network off. A file that can't be identified still plays and still shows up.
- **Posters and details.** Films and shows are matched from their file names, with artwork, cast and episode lists from TMDB. Unsure matches are suggested rather than applied, and any match can be fixed by hand.
- **Continue watching.** Every film and episode resumes where you left it.
- **Collections and universes.** Film series are grouped for you, in release or story order, and you can make your own.
- **Stream from your PC.** Turn on streaming on Windows and your Android phone, tablet or TV finds the PC on its own. No server to set up and no addresses to type; the video never leaves your home.
- **Plays almost anything.** Built on mpv, with hardware decoding up to 4K HEVC, audio and subtitle track selection, timing adjustment, and optional subtitle search on OpenSubtitles.
- **Made for each screen.** Mouse and keyboard on Windows, touch gestures on phones and tablets, and full remote control on Android TV.

## Platforms

| Platform | Notes |
|---|---|
| Windows 10 and 11 | x86_64. Can stream its library to your devices |
| Android phones and tablets | arm64-v8a, armeabi-v7a, x86_64 |
| Android TV | Remote-first layout; hardware decoding on TV boxes |

## Building

Makimedia is written in C++ and QML.

**You need:**

- Qt 6.10 (Core, Quick, Qml, OpenGL, Concurrent, Sql, Network) with the MSVC kit on Windows, or an Android kit
- CMake 3.16 or newer
- libmpv, placed as described in [`third_party/README.md`](third_party/README.md)

**Build** with Qt Creator or from the command line with `qt-cmake`. The build downloads OpenSSL for Android automatically, so HTTPS works there.

**API keys are optional.** A fresh clone builds and runs without any keys: every file still plays, and only matching and subtitle search are unavailable. To enable them, put your own keys in `tmdb_key.txt` and `opensubtitles_key.txt` in the repository root, then run:

```
python scripts/generate_api_key.py
```

Neither the key files nor the generated header are ever committed.

**Tests** are built by default (`MM_BUILD_TESTS=ON`) and run with `ctest`.

**Logging.** The app writes a local log, `makimedia.log`, which never leaves the device. Development builds keep every line; build store releases with `-DMM_LOG_VERBOSE=OFF` to keep only info, warnings and errors.

## Privacy

Makimedia has no account, no analytics and no server of its own. Your library, progress and settings stay on your devices. The only network traffic is TMDB look-ups for posters and details, OpenSubtitles searches if you enable them, and streaming between your own devices. See the full privacy policy on the website.

## Contact

Questions, bug reports and copyright requests: [support@makimedia.org](mailto:support@makimedia.org). Bugs can also be reported as GitHub issues.

## Licence

Copyright (C) 2026 TopicDev.

Makimedia is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 2 of the License, or (at your option) any later version.

Makimedia is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the [GNU General Public License](LICENSE) for more details.

## Credits

This product uses the TMDB API but is not endorsed or certified by TMDB. Film and show details, posters and artwork come from [TMDB](https://www.themoviedb.org/) and belong to their respective owners.

Subtitle search uses [OpenSubtitles](https://www.opensubtitles.com/).

Makimedia is built with open source components, each under its own licence:

| Component | Licence |
|---|---|
| [Qt](https://www.qt.io/) | LGPL v3, dynamically linked |
| [mpv](https://mpv.io/) / [FFmpeg](https://ffmpeg.org/) | GPL v3 in the builds Makimedia ships, unmodified |
| [SQLite](https://sqlite.org/) | Public domain |
| [OpenSSL](https://www.openssl.org/) (inside libmpv on Windows, bundled on Android) | Apache License 2.0 |
| [Roboto](https://fonts.google.com/specimen/Roboto) | Apache License 2.0 |

Makimedia's own code is GPL v2 or later. Because the mpv and FFmpeg builds it includes are GPL v3, the program as distributed for Windows and Android is covered by GPL v3. The full notices and licence texts are in [`licenses/`](licenses/), and in the app under *Settings, About*. The source for those libraries is available on request: see [makimedia.org/legal.html#source-code](https://makimedia.org/legal.html#source-code).

Makimedia is not affiliated with Netflix, Prime Video, Disney+ or any other streaming service. All trademarks belong to their respective owners.
