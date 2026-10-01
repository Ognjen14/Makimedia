# third_party

Nothing here is committed. Each SDK below is dropped in by hand, because the
licences and sizes make vendoring them a poor fit for the repo.

## mpv (Windows, x86_64)

SPECS.md 2.1 calls for the prebuilt shinchiro libmpv rather than compiling
FFmpeg yourself on Windows. That removes the single most painful part of the
Android toolchain from the desktop build.

### 1. Download

From <https://github.com/shinchiro/mpv-winbuild-cmake/releases>, take the
**`mpv-dev-x86_64-*.7z`** asset. Three things to get right in that filename:

- **`-dev`**, not the plain player build - only the dev archive carries the
  headers and the export definitions.
- **`x86_64`**, not `i686`. We do not ship 32-bit.
- **No `-v3` suffix.** The `x86_64-v3` builds target the x86-64-v3
  microarchitecture level (AVX2, BMI2, FMA) and fault with an illegal
  instruction on anything older than roughly Haswell or Excavator. Releases
  go out as a portable zip to arbitrary machines (SPECS.md 9.2), so the
  baseline build is the only safe one. `-v3` is for a self-compile on a known
  CPU, not for distribution.

So of `mpv-dev-i686-*`, `mpv-dev-x86_64-*` and `mpv-dev-x86_64-v3-*`, the
middle one is correct. It contains:

| In the archive | What it is |
|---|---|
| `include/mpv/*.h` | `client.h`, `render.h`, `render_gl.h`, `stream_cb.h` |
| `libmpv-2.dll` | the runtime library |
| `libmpv.dll.a` | MinGW import library - not usable from MSVC |

### 2. Get a link library your toolchain can use

The archive is *built by* MinGW, but libmpv's API is pure C, so the DLL works
with either toolchain. Only the import library differs.

**MSVC (what SPECS.md 2.1 calls for).** Newer shinchiro archives no longer
ship `mpv.def`, so it has to be regenerated from the DLL's export table. The
committed `mpv.def` was produced that way and lists the 54 `mpv_*` public
symbols; the DLL also exports ~150 unrelated symbols from statically linked
dependencies, which are deliberately left out. Regenerate it after any SDK
update by reading the PE export directory of `libmpv-2.dll`.

Then, from an **x64 Native Tools Command Prompt for VS**, in this directory:

```
lib /def:mpv.def /name:libmpv-2.dll /out:mpv.lib /MACHINE:X64
```

This is the step that catches people out - linking `libmpv.dll.a` with MSVC
fails with unresolved externals that look like a missing library rather than
a toolchain mismatch.

**MinGW.** No step needed; drop `libmpv.dll.a` into `lib/` and
`FindLibmpv.cmake` picks it up, it is already in the search names. Note that a
MinGW build must also ship `libgcc_s_seh-1.dll`, `libstdc++-6.dll` and
`libwinpthread-1.dll` alongside the executable.

Whichever you pick, Qt, libmpv and the app all have to come from the same
toolchain - they cannot be mixed.

### 3. Lay it out

```
third_party/mpv/
  include/mpv/client.h
  include/mpv/render.h
  include/mpv/render_gl.h
  lib/mpv.lib
  bin/libmpv-2.dll
```

`cmake/FindLibmpv.cmake` looks exactly here, fails with a readable message if
anything is missing, and copies `libmpv-2.dll` next to the executable after
each build. Point it elsewhere with `-DMAKIMEDIA_MPV_ROOT=<path>`.

## mpv (Android)

This is the SPECS.md 11 go/no-go spike. FFmpeg must be built with
`--enable-mediacodec --enable-jni`, and libmpv built against it, once per ABI.

### 1. Lay it out

`cmake/FindLibmpv.cmake` switches root when `ANDROID` is set and looks for:

```
third_party/mpv-android/
  include/mpv/client.h
  include/mpv/render.h
  include/mpv/render_gl.h
  arm64-v8a/libmpv.so
  armeabi-v7a/libmpv.so
  x86_64/libmpv.so
```

The headers are shared across ABIs; only the `.so` files are per-ABI. Every
`.so` in the ABI directory is packaged into the APK through
`QT_ANDROID_EXTRA_LIBS`, so if FFmpeg is built as shared libraries put
`libavcodec.so` and friends in the same directory and they travel too.

Override the root with `-DMAKIMEDIA_MPV_ROOT=<path>`. Configure the Android
kit and the CMake output will name the ABI it resolved and list what it will
package - check those two lines before building.

### 2. The one that fails silently

`av_jni_set_java_vm()` must be handed the `JavaVM` before any decode, or
MediaCodec is unreachable and FFmpeg quietly decodes in software. Nothing
reports an error; 4K just stutters (SPECS.md 2.1).

`MpvController` does this in its constructor, before `mpv_create()`. It
resolves the symbol with `dlsym(RTLD_DEFAULT, ...)` rather than linking
FFmpeg directly, so it works whether FFmpeg is statically inside `libmpv.so`
with the symbol exported, or shipped as separate shared libraries.

**On first run, check the log.** One of these appears:

- `JavaVM handed to FFmpeg, MediaCodec is reachable` - correct.
- `av_jni_set_java_vm was not found in any loaded library` - the build does
  not export it. Either export it from `libmpv.so` or ship FFmpeg as shared
  libraries in the ABI directory. **Do not proceed past this**: everything
  will appear to work while decoding entirely in software.

### 3. The exact release in use

**<https://github.com/mpv-android/mpv-android/releases/tag/2025-08-25>**

Per-ABI APKs. Unzip and take every `.so` from `lib/<abi>/`:

| ABI | Asset | Needed for |
|---|---|---|
| `arm64-v8a` | `app-default-arm64-v8a-release.apk` | phones, tablets, 64-bit TV boxes |
| `armeabi-v7a` | `app-default-armeabi-v7a-release.apk` | **Google TV boxes** - see below |
| `x86_64` | `app-default-x86_64-release.apk` | the TV emulator |
| `x86` | `app-default-x86-release.apk` | nothing real, but Qt installs the kit |

All four are kept because `QT_ANDROID_BUILD_ALL_ABIS=ON` builds every ABI Qt
finds a *kit* for, not every ABI that has libraries here. One missing
directory fails that sub-build and takes the whole build with it, so the set
here has to match the kits installed - or the ABIs have to be named
explicitly with `-DQT_ANDROID_ABIS=...`.

**A Google TV box is very likely 32-bit.** The cheap ones - Chromecast with
Google TV and most Onn and Amlogic boxes - pair a 64-bit ARMv8-A chip with a
**32-bit Android userspace**. AIDA64 and every other tool reports the CPU as
"ARMv8-A 64-bit", which reads as if `arm64-v8a` would work; the giveaway is
"(32-bit mode)" beside it, or a kernel architecture of `armv8l` rather than
`aarch64`. An `arm64-v8a`-only APK is refused with nothing but "App not
installed", or `INSTALL_FAILED_NO_MATCHING_ABIS` if you can see a log.

Take every ABI. `-DQT_ANDROID_BUILD_ALL_ABIS=ON` packages them into one APK,
and every ABI directory must exist or its sub-build fails and takes the whole
build with it.

The tag is `2025-08-25`. The *filenames* carry `-release`, the tag does not,
so `2025-08-25-release` 404s.

What that release contains, and what the binaries here must keep matching:

| | |
|---|---|
| mpv | `v0.40.0-292-g9f153e2a2` (`mpv-player/mpv@9f153e2a2`) |
| FFmpeg | 8.0, libavcodec `62.11.100` |
| harfbuzz | 11.4.3 |
| Client API | `MPV_CLIENT_API_VERSION` 2.5 |

**Do not reach for the newest release.** `2026-08-11` is built against a
different libmpv commit and a different FFmpeg, and the headers in
`include/mpv` are shared by every ABI - so a newer `.so` mismatches whatever
else is already here. This has already been tried once and did not work.

All ABIs move together or none do. Adding `x86_64` for the TV emulator means
taking it from *this* release. Recover the version from any `.so` with:

```
strings libmpv.so | grep "^mpv v"
strings libavcodec.so | grep -o "Lavc[0-9.]*"
```

and match it against the "Full set of build dependencies" list in the
release notes.

### 4. Where the libraries come from

Either build them, or start from a prebuilt set to answer the go/no-go
question first. The build is the multi-weekend part; the spike question is
only "does MediaCodec decode 4K on real hardware", and a prebuilt answers it
in an afternoon. Build your own before shipping either way, so the FFmpeg
feature set and version are yours.

The reference for the build is the `mpv-android` project's buildscripts,
which already carry the correct FFmpeg configure flags for this case.

### 5. Confirming it actually worked

`hwdec` is set to `mediacodec-copy` on Android in `MpvController`, and the
resolved decoder is observed through `hwdec-current` and shown in the player
top bar - green when hardware, amber when software. A green `mediacodec` on
a 4K HEVC file is the pass. Amber `software decoding` means the JavaVM
handoff or the FFmpeg flags are wrong, whatever the log said earlier.

## OpenSSL (Android)

Android ships no OpenSSL that Qt can link against, so Qt Network has no TLS
backend and **every HTTPS request fails**. Video playback is unaffected -
this only shows up once something reaches the network, which for us means
TMDB metadata.

CMake fetches KDAB's prebuilt libraries automatically the first time you
configure an Android build, so normally there is nothing to do here.

If you would rather not have the build reach the network, clone it by hand:

```
git clone https://github.com/KDAB/android_openssl third_party/android_openssl
```

CMake looks there first and only falls back to fetching. To skip OpenSSL
entirely, configure with `-DMM_ANDROID_OPENSSL=OFF`; the app still builds and
plays video, and metadata simply never arrives.

### Confirming it worked

The log line at startup is the answer:

```
[I][mm] TLS backend: OpenSSL 3.x.x
```

An `[E]` line saying there is no TLS backend means the libraries were not
packaged, whatever CMake printed at configure time.
