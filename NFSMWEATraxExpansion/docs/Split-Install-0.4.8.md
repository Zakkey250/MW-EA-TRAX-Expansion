# MW EA TRAX Expansion 0.4.8

Install both **Core** and **Runtime**. The **The Run Converter** is optional.

1. Close the game. Extract Core into the game folder (the folder containing speed.exe).
2. Get the matching Runtime package from https://github.com/Zakkey250/MW-EA-TRAX-Expansion/releases and extract it into the same game folder. Keep its complete Runtime/_internal directory.
3. Put your own MP3/WAV/M4A files under scripts/NFSMWEATraxExpansion/Tracks. See the included user guide for UG2 extraction and optional The Run conversion.
4. Launch the game to generate music. When notified, close/restart as instructed to use the completed cache.

When updating from 0.4.7, back up your installation and **keep your existing NFSMWEATraxExpansion.ini, track INIs, Tracks, UG2MusicSFx, Pursuit and Cache**. Replace the Core binaries, RuntimeRequired.json and the complete Runtime contents from the same release. Do not mix runtime versions. The existing layout, pursuit formats and compressed cache format are retained. New/changed inputs or volume settings can still require regeneration.

Existing INIs work without replacement. To match stock reference loudness, change only [Main] VolumeMultiplier to 1.0; an existing 0.8 value is respected. An older INI without JapaneseMetadata retains romanized display. Add JapaneseMetadata=1 under [Main] to enable original Japanese text when your game has Japanese-capable fonts. New installations default to JapaneseMetadata=0. Keep custom display INIs: the importer never overwrites them.

[Updates] Enabled=0 disables online update checks. Language=auto follows game language, with Japanese and English notices; other languages use English. Set Language=Japanese or English to override. The missing-runtime notice remains enabled even when update checks are disabled.

Core has no music converter or downloader. Its missing-runtime dialog opens GitHub only when you click the button, and otherwise continues with stock music. Updates read public release metadata only; no accounts, game files or music metadata are uploaded. Offline errors do not block play. Notices appear once per launch, do not interrupt active driving, and close automatically after 60 seconds.

## 日本語

**CoreとRuntimeの両方が必須**です。**The Run Converterは任意**です。

1. ゲームを終了し、Coreをspeed.exeのあるゲームフォルダーへ展開します。
2. https://github.com/Zakkey250/MW-EA-TRAX-Expansion/releases から対応するRuntimeを取得し、同じゲームフォルダーへ展開します。Runtime/_internalもすべて必要です。
3. 所有するMP3/WAV/M4Aをscripts/NFSMWEATraxExpansion/Tracksへ配置します。UG2音源の取り出し方と任意のThe Run変換は同梱ガイドをご覧ください。
4. ゲームを起動して生成し、完了通知に従って再起動してください。

0.4.7からの更新時はバックアップし、**既存のNFSMWEATraxExpansion.ini、曲別INI、Tracks、UG2MusicSFx、Pursuit、Cacheを保持**してください。同じ版のCoreバイナリ、RuntimeRequired.json、Runtime一式を更新し、ランタイムの版を混在させないでください。従来の配置・追跡データ・圧縮キャッシュ形式を継続します。入力や音量設定の変更時は必要に応じて再生成します。

既存INIはそのまま使用できます。純正曲基準の音量にする場合だけ[Main] VolumeMultiplierを1.0に変更してください。0.8のままならその設定を尊重します。旧INIにJapaneseMetadataがない場合は従来の英字表示を維持します。日本語フォントがある環境で日本語を有効にするには[Main]へJapaneseMetadata=1を追加してください。新規導入用INIでは0に設定しています。曲別INIは自動で上書きしません。

[Updates] Enabled=0でオンライン更新確認を無効化できます。Language=autoはゲーム設定に合わせ、日本語以外は英語の通知を表示します。JapaneseまたはEnglishで指定もできます。ランタイム不足の通知は更新確認を無効化しても表示されます。

Coreには変換アプリやダウンロード機能はありません。ボタンを押した場合だけGitHubをブラウザーで開き、それ以外は標準曲で続行します。更新確認は公開版情報の読み取りのみで、アカウント情報・ゲームファイル・楽曲情報は送信しません。オフラインでもゲームを継続でき、通知は起動ごとに一度、走行中には表示せず、60秒で自動的に閉じます。
