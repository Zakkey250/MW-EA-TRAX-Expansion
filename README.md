# MW EA TRAX Expansion and The Run Pursuit Music

## Source use / ソース利用条件

Research, exchange of ideas and personal use (including local modifications/builds) are permitted. Redistribution and distribution of modified builds require prior express permission. See [LICENSE.md](LICENSE.md); third-party licenses and previously granted rights are preserved.

研究・意見交換・個人利用（個人用の改変・ビルドを含む）は可能です。再配布および改変物のビルド配布には事前の明示的な許可が必要です。[利用条件](LICENSE.md)を確認してください。第三者ライセンスと過去に付与済みの権利は維持します。

Source code for **version 0.4.7**, an ASI mod for Need for Speed Most Wanted (2005), and its optional The Run Pursuit Converter.

**[Build instructions / ビルド手順](NFSMWEATraxExpansion/docs/BUILD-047.md)** · **[Installation guide / 導入ガイド](NFSMWEATraxExpansion/docs/User-Guide.md)** · **[Converter guide / コンバーター](NFSMWEATraxExpansion/docs/Converter-README.md)**

**[Downloads / ダウンロード](https://github.com/Zakkey250/MW-EA-TRAX-Expansion/releases/latest)** · **[0.4.7 changes / 修正内容](NFSMWEATraxExpansion/docs/Release-0.4.7.md)**

## English

The mod imports user-provided MP3/WAV files and Underground 2 music into EA TRAX. The optional converter prepares eight adaptive pursuit scores from the user's PC installation of The Run. The Run is not required for normal-track imports.

No game executable, game audio, generated playback cache, save file, or user configuration is included. `converter/graph.mpf` contains playback control metadata only, not audio.

### Source layout

- `NFSMWEATraxExpansion/src` and `include`: native ASI and audio support.
- `NFSMWEATraxExpansion/runtime`: Python cache generation code and PyInstaller specification.
- `NFSMWEATraxExpansion/converter`: C# converter, source-selection recipe, and playback control metadata.
- `NFSMWEATraxExpansion/tests`: startup notification tests.
- `NFSMWEATraxExpansion/tools`: build, packaging, probe, and executable hook checks.
- `NFSMWEATraxExpansion/third_party`: required headers, licenses, and native link dependencies.
- `upstream/vgmstream`: corresponding source at revision `e07a4a1f2d32658b8420e046276a7cfc425c9bf2`.

The supplied vgmstream static library and mpg123 import library/DLL are build dependencies. They are included to support rebuilding the released code; the repository is not an installable player package.

### Build

Use Windows x64 with Visual Studio C++ v143 tools and a Windows SDK. The cache helper uses CPython 3.14.5 x64 and PyInstaller 6.21.0. The converter uses the Windows .NET Framework C# compiler.

From the repository root:

```powershell
python -m pip install PyInstaller==6.21.0
./NFSMWEATraxExpansion/tools/Build-047.ps1
```

To build only the native projects and converter, omit the Python dependency installation and run:

```powershell
./NFSMWEATraxExpansion/tools/Build-047.ps1 -SkipRuntime
```

See the full build instructions for outputs, tests, rebuilding vgmstream, and obtaining the exact FFmpeg build needed for player packaging.

## 日本語

バージョン0.4.7のMOD本体と、任意導入のThe Runコンバーターのソースです。お手持ちのMP3・WAVやUnderground 2の曲をEA TRAXへ追加し、任意でThe Runの追跡BGMを取り込めます。

ゲームEXE、ゲーム音源、生成済みキャッシュ、セーブ、利用者の設定は含みません。`converter/graph.mpf`は音声を含まない再生制御データです。

Windows x64、Visual StudioのC++ v143ツール、Windows SDKを使用します。キャッシュ生成ツールにはCPython 3.14.5 x64とPyInstaller 6.21.0が必要です。コンバーターはWindowsの.NET Framework C#コンパイラーを使用します。上記コマンドはリポジトリのルートで実行してください。

配布版の再ビルドに必要なヘッダー、vgmstream静的ライブラリ、mpg123のリンク依存ファイル、および対応するvgmstreamソースを含めています。導入用ZIPではありません。詳細は[ビルド手順](NFSMWEATraxExpansion/docs/BUILD-047.md)をご覧ください。

## Credits and licenses

See [third-party sources](NFSMWEATraxExpansion/docs/ThirdParty-Sources.md) and [included license notices](NFSMWEATraxExpansion/distribution/Licenses). Each third-party component retains its respective license. Publishing source does not grant rights to redistribute game audio.
