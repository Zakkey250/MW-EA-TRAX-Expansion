# Build 0.4.8 / ビルド手順

Use Windows x64, Visual Studio C++ v143 and Windows SDK, CPython 3.14 x64, PyInstaller 6.21.0 and the Windows .NET Framework C# compiler. Native code targets Win32. The reference runtime was built using Python 3.14.5.

From the repository root:

Before running TestMetadata.py, prepare the FFmpeg directory described under packaging below; the test creates synthetic audio with that executable.

```powershell
python -m pip install PyInstaller==6.21.0
python -m pip install --target NFSMWEATraxExpansion/runtime/vendor -r NFSMWEATraxExpansion/runtime/requirements.txt
./NFSMWEATraxExpansion/tools/Build-048.ps1
./NFSMWEATraxExpansion/bin/Release/NFSMWEATraxProbe.exe --self-test
./NFSMWEATraxExpansion/bin/Release/StartupTests.exe
./NFSMWEATraxExpansion/tools/Test-UpdateNotice.ps1
python NFSMWEATraxExpansion/tests/TestMetadata.py
```

Use `Build-048.ps1 -SkipRuntime` for the native ASI/Probe and converter only. Native outputs are under `NFSMWEATraxExpansion/bin/Release`; the converter is under `converter`; the complete frozen helper is under `staging/eatrax-048-runtime/BuildCache`. Do not copy only its EXE: all `_internal` files are required.

To package, install standard 7-Zip at `C:/Program Files/7-Zip/7z.exe` and place the FFmpeg 8.1.2 essentials build at `staging/eatrax-046-ffmpeg/ffmpeg-8.1.2-essentials_build`. See `FFmpeg-build-046.txt` for provenance and build details. Run:

```powershell
python NFSMWEATraxExpansion/tools/Package-048.py
```

This produces Core, Runtime and optional Converter 7z files and SHA-256 sidecars in `dist`. Existing output directories/archives are refused; use a clean output location or archive previous outputs before rebuilding. No media or cache is included. Core rejects executable applications, ZIPs and nested archives. `_internal/base_library.zip` is expanded by the build script before packaging. RuntimeRequired.json is generated from the actual runtime payload, so rebuild Core together with Runtime.

`Test-UpdateNotice.ps1 -Live` checks the public EA TRAX GitHub metadata endpoint; `-Dialog` briefly opens and auto-closes isolated native test dialogs. No game is started by these tests. The runtime generation fixture requires locally owned stock music and is not shipped as test media.

The included vgmstream source and native link dependencies are unchanged from 0.4.7. Their rebuild procedure is retained in BUILD-047.md. Metadata dependencies are pinned in runtime/requirements.txt; their license notices are included. No Python vendor environment, generated binary, personal music, game EXE, cache, save or local diagnostics needs to be committed to build this version.

## 日本語

上記コマンドをリポジトリのルートで実行してください。Windows x64、Visual StudioのC++ v143、Windows SDK、Python 3.14 x64、PyInstaller 6.21.0を使用します。ASI・ProbeはWin32、キャッシュ生成ツールはx64です。

requirements.txtの依存ライブラリをruntime/vendorへ導入してからBuild-048.ps1を実行します。-SkipRuntimeではASI・Probe・コンバーターのみビルドします。生成ツールはEXEだけでなく_internal全体が必要です。

梱包には7-Zipと指定のFFmpegビルドが必要です。Package-048.pyは3つの7zとハッシュを生成し、音源・キャッシュ・入れ子アーカイブの混入を検査します。既存出力への上書きは拒否します。RuntimeRequired.jsonは実際のRuntimeから生成するため、CoreとRuntimeを同時に梱包してください。ゲーム内動作確認はこれらの単体検証とは別に行ってください。
