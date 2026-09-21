# Third-party components / サードパーティー構成

Individual licenses in this folder apply to their respective components, independently of the MOD author's permissions.
各コンポーネントには、このフォルダー内の各ライセンスが適用されます。

- FFmpeg 8.1.2 essentials, Gyan Doshi Windows x64 build, GPLv3. Build options and download integrity information: `FFmpeg-build.txt`. [Source revision](https://github.com/FFmpeg/FFmpeg/commit/38b88335f9), [build provider](https://www.gyan.dev/ffmpeg/builds/).
- vgmstream revision `e07a4a1f2d32658b8420e046276a7cfc425c9bf2`. [Source](https://github.com/vgmstream/vgmstream/tree/e07a4a1f2d32658b8420e046276a7cfc425c9bf2). The optional converter's EA-XAS decoder follows `src/coding/ea_xas_decoder.c`.
- mpg123: [source and downloads](https://www.mpg123.de/download.shtml). The supplied DLL and its license are retained from the previous release.
- miniaudio: [source](https://github.com/mackron/miniaudio). See `miniaudio.txt`.
- MinHook: [source](https://github.com/TsudaKageyu/minhook). See `minhook.txt`.
- CPython 3.14.5 x64: [source](https://www.python.org/downloads/release/python-3145/). See `Python.txt`.
- PyInstaller 6.21.0: [source](https://github.com/pyinstaller/pyinstaller/tree/v6.21.0). No-archive, one-folder build; UPX is disabled. See `PyInstaller.txt`.

The player package contains no music from MW, UG2 or The Run. Playback caches and converter output are generated locally from the user's files.
ゲーム音源は本体に同梱していません。再生用キャッシュと追跡音源は利用者のファイルからローカル生成します。

## Metadata support (0.4.8)

Pinned dependencies are listed in runtime/requirements.txt and supplied during the runtime build from runtime/vendor: Mutagen 1.47.0 (GPL-2.0-or-later), pykakasi 2.3.0 (GPL-3.0-or-later), jaconv 0.5.0, Deprecated 1.3.1 and wrapt 2.4.1. Their license notices are in distribution/Licenses/Metadata-*.txt. Mutagen reads tags locally; pykakasi uses bundled reading dictionaries. No audio or tag text is sent to a service.

The update metadata parser uses nlohmann/json under its MIT license (third_party/nlohmann/LICENSE.MIT). Notification serialization and release validation are adapted from Zakkey250's MW Native Free Roam Racer implementation.
