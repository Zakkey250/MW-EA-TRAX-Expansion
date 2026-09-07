# EA TRAX Expansion — 導入ガイド / Installation guide

## 日本語

### 用意するもの

64ビット版Windows 10/11向けです。MOD本体は32ビットのMWに対応します。

- PC版 Need for Speed Most Wanted（2005）。対応対象はv1.3 ENのNFSPatcher版実行ファイル（通常版・4GB Patch適用版）です。別の実行ファイルでの動作は保証していません。
- 32ビットのASIローダー。未導入の場合は [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) の公式配布からWin32版を用意し、公式手順に従って導入してください。ローダーが既に入っている場合はそのまま使用してください。
- 曲を追加する場合は、お手持ちのMP3・WAV、またはPC版Underground 2のインストール済みデータ。
- 変換データを保存できるディスク空き容量。生成データはMP3より大きくなり、再生成時には新旧両方を置く容量が必要です。

ゲーム本体、ASIローダー、UG2音源は付属しません。音楽の変換に必要なプログラムは付属しているため、Pythonや音声編集ソフトの追加インストールは不要です。

### インストール

1. ゲームを終了してZIPを展開します。
2. 展開した `scripts` フォルダーを、MWのゲーム実行ファイルと同じフォルダーへコピーします。`scripts` の中にもう一つ `scripts` を作らないでください。
3. 下の説明に従って音源を追加します。The Run音源を追加しなくても、通常曲の追加機能を利用できます。The Runの追跡BGMは別配布のコンバーターで追加できます。
4. ゲームを起動します。初回の再生データ生成には数分以上かかる場合があります。「生成中」画面が出ている間は完了を待ってください。
5. 完了通知でOKを押すとゲームが終了します。**ゲームを1回起動し直すと、新しい音楽が反映されます。** 通知はゲームの設定言語に合わせて表示されます。

配置は次のようになります。

```text
ゲームのフォルダー/
  speed.exe
  scripts/
    NFSMWEATraxExpansion.asi
    NFSMWEATraxExpansion/
      NFSMWEATraxExpansion.ini
      Tracks/           MP3・WAVを置く場所
      UG2MusicSFx/      UG2から取り出した2ファイルを置く場所
      Pursuit/          任意のThe Run変換後に作成される追跡BGM
      Runtime/          変換用プログラム（そのまま使用）
```

### MP3・WAVを追加する

ゲーム終了中に `Tracks` にファイルをコピーします。サブフォルダーも使用できます。曲の追加・削除・差し替え後は次回起動時に必要なデータが生成されます。完了通知が出たらゲームを起動し直してください。

曲名・アーティスト名を指定したい場合は、音源と同じ場所に同名のINIを作成します。例：`My Song.mp3` と `My Song.ini`。INIは省略できます。日本語を含むINIは「UTF-16 LE（BOM付き）」で保存してください。

```ini
[Track]
Title=My Song
Artist=My Artist
Album=My Album
Mode=ALL
```

`Mode` は `FE`（メニュー）、`IG`（走行中）、`ALL`（両方）から選べます。追加できる通常曲はUG2曲と合わせて最大94曲です。MP3・WAVをファイル名順に取り込み、その後にUG2曲を追加します。UG2の27曲をすべて使う場合、MP3・WAVは67曲以内にしてください。

### Underground 2の音源を用意する

お手持ちのPC版Underground 2をインストールしてから、次の操作を行います。**`sdat.viv` は複数ファイルをまとめたアーカイブです。このMODは直接読み込めません。拡張子だけを `.mpf` や `.mus` に変えても使用できません。**

1. UG2のインストール先にある `SDATA\sdat.viv` を探し、作業用フォルダーにコピーします。標準的な場所の例は `C:\Program Files (x86)\EA GAMES\Need for Speed Underground 2\SDATA\sdat.viv` です。実際のインストール先に読み替えてください。
2. BIG4対応のアーカイブ抽出ソフトを用意します。例えば [FinalBIGv2](https://github.com/triatomic/finalBIGv2-2026) の開発元ページに配布先へのリンクがあります。抽出ソフトは本パッケージには付属しません。
3. コピーしたアーカイブを抽出ソフトで開きます。`.viv` がファイル選択に表示されない場合は「すべてのファイル」を選ぶか、**作業用コピーだけ**を `sdat.big` に改名して開いてください。
4. アーカイブ内の `PFDATA\MusicSFx.mpf` と `PFDATA\MusicSFx.mus` を探し、抽出機能で両方を作業用フォルダーへ取り出します。元のアーカイブには保存・書き戻しをしません。
5. 取り出した2ファイルを、MW側の `scripts\NFSMWEATraxExpansion\UG2MusicSFx` にコピーします。抽出時に `PFDATA` フォルダーができた場合は、その**中の2ファイルだけ**をコピーしてください。

最終的に、次の3ファイルが同じフォルダーに並んでいれば配置は完了です。

```text
UG2MusicSFx/
  MusicSFx.mpf
  MusicSFx.mus
  MusicSFx.metadata.ini   ← 付属の曲情報。残してください
```

2ファイルは同じUG2アーカイブから取り出した組で使用します。27曲分の表示情報は付属の `MusicSFx.metadata.ini` に設定済みです。ゲーム起動後の生成と再起動の手順は、MP3・WAVの場合と同じです。

### The Runの追跡BGMを追加する（任意）

本パッケージにはThe Runの音源を含みません。MP3・WAV・UG2曲の追加機能は、The Runなしで動作します。追跡時はMW標準BGMを使用します。

1. PC版The Runをインストールします。
2. 別配布の `TheRunPursuitConverter.exe` をダウンロードして起動します。MWを終了しておいてください。
3. The RunとMWのインストール先を選び、「変換開始」を押します。変換には数分以上と、数GiBの作業用空き容量が必要です。
4. 完了後、MWを起動します。再生用キャッシュの生成が終わったら通知に従って起動し直してください。

コンバーターはインストール済みMODのFFmpegを使用します。Pythonや別の音声ツールの導入は不要です。初期の対応対象は検証済みPC版音源レイアウトです。音源が一致しない版はエラーで停止します。既存の追跡データは置き換え前に `Pursuit-backup-日時-識別子` へ退避します。ゲームの元データは変更しません。

### 設定と追跡BGM

`NFSMWEATraxExpansion.ini` をテキストエディターで開くと、各項目の日英説明を読めます。最初は変更不要です。設定名・曲名はそのままにし、右側の値だけを変更してください。

- `[Main]`：MOD全体、MP3・WAVの取り込み、UG2曲の取り込み、追加通常曲の音量、曲数上限を設定します。変更はゲーム終了中に行います。音量を変えると通常曲の再変換が必要です。
- `[Pursuit]`：変換済みのThe Run 8曲とMW標準追跡BGMから、追跡開始ごとにランダムで選びます。曲の `true` は使用、`false` は除外です。`Vanilla` はMW標準追跡BGMです。同じ曲が続く場合があります。
- `Mode=Random` と `Mode=List` はどちらも有効な曲からランダムで選びます。順番再生にはなりません。全曲を無効にした場合はMW標準BGMになります。
- 特定の追跡曲だけを使う場合は `TestMode=true` にし、`TestTrack` にリストの曲名をそのまま指定します。通常のランダム選曲へ戻すには `TestMode=false` にします。

曲別設定は次の追跡開始時に反映されます。再生中の曲は途中で切り替わりません。`Enabled` の変更後はゲームを起動し直してください。The Run追跡BGMの音量はコンバーターで調整されます。`Pursuit/Pursuit.ini` は編集不要です。

### 困ったとき・削除方法

- UG2曲が出ない：2ファイルの名前と配置を確認してください。`sdat.viv` のまま、1ファイルだけ、または `UG2MusicSFx/PFDATA` の中に置いた状態では取り込めません。`LoadMusicSFx=1` と曲数上限も確認してください。
- 新しい曲が反映されない：生成完了後にゲームを起動し直してください。再生成が必要な場合はゲームを終了し、MODフォルダー内の `Cache` を削除してから起動します。
- 生成に失敗する：空き容量とフォルダーへの書き込み権限を確認してください。詳細はMODフォルダーの `Cache/Build.log` と `NFSMWEATraxExpansion.log` に記録されます。
- 更新する：ゲーム終了中に設定ファイルと追加した音源をバックアップします。設定済みINIは保持し、必要な設定を引き継いでください。
- 削除する：ゲームを終了し、追加した音源を退避したうえで `scripts/NFSMWEATraxExpansion.asi` と `scripts/NFSMWEATraxExpansion` フォルダーを削除します。

## English

### Requirements

Use 64-bit Windows 10/11. The ASI itself targets the 32-bit MW game.

- Need for Speed Most Wanted (2005), PC edition. The supported executable is NFSPatcher for v1.3 EN, both unpatched and with the 4GB Patch applied. Compatibility with other executables is not guaranteed.
- A 32-bit ASI loader. If needed, obtain the Win32 version of [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) and follow its official installation instructions. Keep your existing loader if one is already installed.
- Your own MP3/WAV files or an installed PC copy of Underground 2, if you want to add music.
- Free disk space for converted audio. Generated data can be larger than the MP3 originals; rebuilding requires room for both old and new data.

The game, ASI loader and UG2 audio are not included. Audio conversion programs are included; no separate Python or audio editor installation is needed.

### Installation

1. Close the game and extract this ZIP.
2. Copy its `scripts` folder into the folder containing the MW executable. Do not create a second `scripts` folder inside the first.
3. Add music using the instructions below. The Run pursuit music is optional and is prepared using the separately available converter.
4. Start the game. First-time generation may take several minutes or longer. Wait while the generation screen is displayed.
5. Click OK on the completion notice to close the game. **Start the game once more to use the new music.** Generation and completion notices follow the game's configured language.

```text
Game folder/
  speed.exe
  scripts/
    NFSMWEATraxExpansion.asi
    NFSMWEATraxExpansion/
      NFSMWEATraxExpansion.ini
      Tracks/           Your MP3/WAV files
      UG2MusicSFx/      The two extracted UG2 files
      Pursuit/          Optional pursuit music created by the converter
      Runtime/          Conversion programs; keep as supplied
```

### Adding MP3/WAV music

With the game closed, copy audio files into `Tracks`. Subfolders are supported. Adding, removing or replacing music triggers the necessary generation on the next launch. Restart the game when the completion notice appears.

To specify display information, create an optional INI next to the audio with the same base name, such as `My Song.mp3` and `My Song.ini`. Save INIs containing Japanese text as UTF-16 LE with BOM.

```ini
[Track]
Title=My Song
Artist=My Artist
Album=My Album
Mode=ALL
```

Choose `FE` for menus, `IG` for driving or `ALL` for both. The total limit is 94 added normal tracks, including UG2 music. MP3/WAV files are imported in filename order, followed by UG2 tracks. To include all 27 UG2 songs, use no more than 67 MP3/WAV tracks.

### Preparing Underground 2 music

Install your own PC copy of Underground 2 first. **`sdat.viv` is an archive containing multiple files. This MOD cannot read it directly. Renaming it to `.mpf` or `.mus` does not work.**

1. Locate `SDATA\sdat.viv` in your UG2 installation and copy it into a working folder. A typical location is `C:\Program Files (x86)\EA GAMES\Need for Speed Underground 2\SDATA\sdat.viv`; use your actual installation path.
2. Obtain an archive extractor supporting BIG4. For example, the developer's [FinalBIGv2 page](https://github.com/triatomic/finalBIGv2-2026) links to a public download. An archive extractor is not included in this package.
3. Open the copied archive in the extractor. If `.viv` is hidden by the file filter, select “All files” or rename **only the working copy** to `sdat.big` and open it.
4. Find `PFDATA\MusicSFx.mpf` and `PFDATA\MusicSFx.mus` inside the archive. Extract both into your working folder. Do not save changes back to the original archive.
5. Copy the two extracted files into MW's `scripts\NFSMWEATraxExpansion\UG2MusicSFx` folder. If extraction created a `PFDATA` folder, copy **the two files inside it**, not the folder itself.

The final layout must be:

```text
UG2MusicSFx/
  MusicSFx.mpf
  MusicSFx.mus
  MusicSFx.metadata.ini   ← Included track information; keep this file
```

Use a matching pair from the same UG2 archive. Display information for its 27 songs is already supplied in `MusicSFx.metadata.ini`. Start the game, wait for generation and restart when prompted, just as for MP3/WAV music.

### Adding The Run pursuit music (optional)

The Run audio is not included. MP3/WAV and UG2 imports work without The Run; pursuits use stock MW music until a pursuit pack is installed.

1. Install your PC copy of The Run.
2. Download and open the separately supplied `TheRunPursuitConverter.exe`. Close MW first.
3. Select both game installation folders and click Convert. Allow several minutes or longer and several GiB of working space.
4. After completion, launch MW. Wait for its playback cache to finish and restart when prompted.

The converter uses FFmpeg from your installed MOD. No separate Python or audio tools are required. Initial support covers the verified PC audio layout; mismatched source data is rejected. Existing pursuit data is moved to a `Pursuit-backup-date-id` folder before replacement. Original game files remain unchanged.

### Settings and pursuit music

Open `NFSMWEATraxExpansion.ini` in a text editor for Japanese and English explanations of each setting. The defaults are ready to use. Keep setting names and song names unchanged; edit only the values on the right.

- `[Main]` controls the MOD, MP3/WAV and UG2 imports, added normal-track volume and the track limit. Edit these settings with the game closed. Changing volume requires normal tracks to be converted again.
- `[Pursuit]` randomly chooses from the eight converted The Run scores and stock MW pursuit music at each pursuit start. Set a song to `true` to include it or `false` to exclude it. `Vanilla` means stock MW pursuit music. Repeats are possible.
- Both `Mode=Random` and `Mode=List` select randomly from enabled songs, rather than playing in order. If all songs are disabled, stock music is used.
- To use one specific pursuit score, set `TestMode=true` and copy its exact listed name into `TestTrack`. Return to normal random selection with `TestMode=false`.

Song switches take effect at the next pursuit start without interrupting the current music. Restart after changing `Enabled`. The converter adjusts pursuit score volume separately. You do not need to edit `Pursuit/Pursuit.ini`.

### Troubleshooting and removal

- Missing UG2 music: check both filenames and their location. A raw `sdat.viv`, a single file, or files nested under `UG2MusicSFx/PFDATA` will not work. Also check `LoadMusicSFx=1` and the track limit.
- New music is missing: restart after generation finishes. To regenerate data, close the game, delete only the MOD's `Cache` folder and start the game again.
- Generation fails: check free space and write access to the folder. Details are recorded in `Cache/Build.log` and `NFSMWEATraxExpansion.log` inside the MOD folder.
- Updating: close the game and back up your settings and added audio. Preserve configured INI files and transfer settings as needed.
- Removing: close the game, move your added audio somewhere safe, then remove `scripts/NFSMWEATraxExpansion.asi` and the `scripts/NFSMWEATraxExpansion` folder.
