# The Run Pursuit Converter 0.4.7

## 日本語

お手持ちのPC版Need for Speed The Runから、MW EA TRAX Expansion向けの追跡BGM 8曲を生成するアプリです。音源は内蔵していません。

1. 64ビット版Windows 10/11で、MW EA TRAX Expansion 0.4.7を先に導入してください。
2. The Runをインストールし、MWを終了してください。
3. `TheRunPursuitConverter.exe` を起動します。
4. The RunとMWの**ゲーム本体のフォルダー**をそれぞれ選択し、「変換開始」を押します。
5. 完了まで待ちます。数分以上かかる場合があります。数GiBの作業用空き容量を用意してください。
6. MWを起動します。再生用キャッシュの生成後、通知のOKで終了してから起動し直してください。

アプリ本体は1つのEXEです。音量調整にはインストール済みMODの `Runtime/ffmpeg.exe`、圧縮には `Runtime/NFSMWEATraxProbe.exe` を使用します。圧縮生成に対応したMOD本体を先に導入してください。Python、SX、vgmstream-cliの別途導入は不要です。.NET FrameworkはWindows 10/11に含まれるものを使用します。

追跡BGMはゲームで直接再生できるEA-XA圧縮形式で保存します。非可逆圧縮のため音質にはわずかな変化があります。曲の長さと追跡状況に応じた構成は維持します。

入力はThe Runの `Data/Win32/ChunksAudio.sb` です。初期対応は検証済みPC音源レイアウトに限定します。必要な202音源のハッシュが異なる場合は停止し、既存の追跡データを保持します。UG2の音源はこのアプリでは扱いません。

出力はMW側の `scripts/NFSMWEATraxExpansion/Pursuit` です。既存フォルダーがある場合は、同じ場所の `Pursuit-backup-日時-識別子` へ退避します。復元する場合はMW終了中に現在のPursuitを退避し、バックアップをPursuitへ戻してください。次回起動時にキャッシュが再生成されます。

キャンセル後は終了処理が完了してからウィンドウを閉じてください。ゲームの元データ、通常曲の設定、セーブは変更しません。生成物はお使いのPCで利用してください。本アプリは音源の再配布許可を付与しません。

## English

Create eight pursuit scores for MW EA TRAX Expansion from your PC copy of Need for Speed The Run. No audio is embedded in this application.

1. Install MW EA TRAX Expansion 0.4.7 on 64-bit Windows 10/11 first.
2. Install The Run and close MW.
3. Open `TheRunPursuitConverter.exe`.
4. Select the **game installation folders** for The Run and MW, then click Convert.
5. Wait for completion. Allow several minutes or longer and several GiB of free working space.
6. Start MW. After its playback cache finishes, click OK to close the game, then start it again.

The application is a single EXE. It uses the installed MOD's `Runtime/ffmpeg.exe` for audio processing and `Runtime/NFSMWEATraxProbe.exe` for compression. Install the compression-enabled main MOD first. No separate Python, SX or vgmstream-cli installation is needed. It uses the .NET Framework supplied with Windows 10/11.

Pursuit music is saved in EA-XA format for direct playback by the game. This is lossy compression, so audio quality changes slightly. Track lengths and the adaptive pursuit structure are preserved.

Input is The Run's `Data/Win32/ChunksAudio.sb`. Initial support covers the verified PC audio layout. The 202 required source streams are checked by hash; mismatched data is rejected while preserving existing pursuit files. This application does not import UG2 audio.

Output is written to MW's `scripts/NFSMWEATraxExpansion/Pursuit`. An existing folder is moved to `Pursuit-backup-date-id` beside it. To restore it, close MW, move the current Pursuit folder aside, then rename the backup to Pursuit. The cache is regenerated on the next launch.

After cancelling, wait for cleanup before closing the window. Original game data, normal-track settings and saves remain unchanged. Generated audio is for use on your PC; this application does not grant permission to redistribute it.

## Credits

EA-XAS decoder implementation follows vgmstream's BSD-licensed decoder. See `Licenses/vgmstream.txt`.
