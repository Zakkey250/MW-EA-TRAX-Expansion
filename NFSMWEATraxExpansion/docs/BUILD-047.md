# Build 0.4.7 / ビルド手順

This is developer documentation. Player installation is described in User-Guide.md.
開発者向けの手順です。利用者向け導入手順はUser-Guide.mdをご覧ください。

## Toolchain

- Windows x64, Visual Studio with C++ v143 tools and Windows SDK.
- CPython 3.14.5 x64 and PyInstaller 6.21.0 for BuildCache.
- Windows .NET Framework C# compiler (`Framework64/v4.0.30319/csc.exe`) for the optional GUI converter.
- FFmpeg 8.1.2 essentials is a separately built third-party executable. Its source revision, build options, license and download SHA256 are in `FFmpeg-build-046.txt` and `ThirdParty-Sources.md`.

## Build the MOD and converter

From the workspace root containing NFSMWEATraxExpansion:

```powershell
./NFSMWEATraxExpansion/tools/Build-047.ps1
```

The native projects use Release/Win32, static MSVC runtime and the supplied vgmstream/mpg123 link dependencies. The source bundle includes those dependencies and the corresponding vgmstream source tree. The converter embeds only `recipe.json`, `graph.mpf` and `profile.ini`: source locations/hashes and playback control metadata, with no audio.

Outputs:

- `NFSMWEATraxExpansion/bin/Release/NFSMWEATraxExpansion.asi`
- `NFSMWEATraxExpansion/bin/Release/NFSMWEATraxProbe.exe`
- `NFSMWEATraxExpansion/converter/TheRunPursuitConverter.exe`
- `staging/eatrax-047-runtime/BuildCache/`

The cache runtime uses `noarchive=True`, one-folder output and `upx=False`. It has loose Python modules instead of `base_library.zip`. Do not use one-file self-extracting mode.

To rebuild the vgmstream static library, use its `src/libvgmstream.vcxproj`, Release/Win32, with `VLibPreprocessorDefinitions=VGM_USE_MPEG` and the v143 toolset. Copy the resulting libvgmstream.lib to `NFSMWEATraxExpansion/third_party/vgmstream/lib/Win32/`. Its public headers are under `src/libvgmstream.h` and related API headers in the source tree. The mpg123 import library is retained separately.

The bundled corresponding vgmstream source is under `upstream/vgmstream`. This source archive is a developer download, separate from both player ZIPs.

## Packaging

After building, download the FFmpeg package identified in `FFmpeg-build-046.txt`, verify its SHA256 and extract it so that `staging/eatrax-046-ffmpeg/ffmpeg-8.1.2-essentials_build/bin/ffmpeg.exe` exists beneath the workspace root. Then run:

```powershell
python ./NFSMWEATraxExpansion/tools/Package-047.py
```

This writes the main MOD and optional converter ZIPs, their file manifests and SHA256 files under `dist`. The packaging script rejects music files, playback caches and nested archives. FFmpeg is not included in this source archive; obtain the exact build separately as described above.

## Validation

```powershell
./NFSMWEATraxExpansion/bin/Release/NFSMWEATraxProbe.exe --self-test
./NFSMWEATraxExpansion/bin/Release/StartupTests.exe
./NFSMWEATraxExpansion/tools/Test-HookSurface.ps1 -Executable 'PATH_TO_MW/speed.exe'
```

The converter also supports:

```text
TheRunPursuitConverter.exe --convert "THE_RUN_GAME_FOLDER" "MW_GAME_FOLDER"
```

Exit 0 means success; exit 1 means failure. Use the GUI for progress/error details. BuildCache exits 0 on a cache hit, 2 after successful generation requiring a game restart, and a nonzero error code on failure.

The developer-only `Prepare-TheRunRecipe.py` refers to historical local research records and is not required to rebuild the shipped converter. The resulting no-audio recipe is included directly. Do not publish source game archives, extracted audio, generated pursuit banks, or playback caches.

Build-047.ps1 extracts the Python standard-library ZIP into the runtime folder before packaging. Package-047.py preserves the complete version number in archive filenames.

Build-047.ps1はPython標準ライブラリを展開してから梱包します。配布物に入れ子ZIPは含めません。
