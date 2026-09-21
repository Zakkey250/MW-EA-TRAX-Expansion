# EA TRAX Expansion 0.4.8

- Added M4A (AAC/ALAC) input and automatic metadata INI creation for tracks without an INI. Existing track INIs and source audio are preserved.
- Added optional Japanese metadata display. JapaneseMetadata defaults to 0; set it to 1 when using Japanese-capable game fonts. Sort/reading tags help preserve special artist-name readings.
- Set the default added-track volume multiplier to 1.0, matching the measured MW stock reference. Existing volume settings remain respected.
- Fixed pursuit selection: Random includes stock MW music and all installed pursuit scores regardless of song switches. List uses only entries explicitly set to true; an empty list falls back to stock. TestMode retains priority.
- Distribution packaging has changed.
- Added a localized missing-runtime notice and an optional startup update notice, serialized with compatible Zakkey MOD notices. Browsers open only when requested. No automatic download or installation.

## 日本語

- M4A（AAC/ALAC）対応、INIがない曲のメタタグからのINI自動作成を追加。既存の曲別INI・元音源は保持します。
- 任意の日本語表示を追加。JapaneseMetadataの初期値は0です。日本語対応フォントがある環境で1にすると有効になります。読みタグを優先して特殊なアーティスト名の読みを扱います。
- 追加曲の音量倍率の初期値を1.0へ変更し、計測済みのMW純正曲基準に合わせました。既存の音量設定は尊重します。
- 追跡曲の選択を修正。Randomは曲別設定にかかわらずMW標準と導入済みの全追跡曲が対象です。Listは明示的にtrueの曲だけが対象で、対象が空ならMW標準曲を使用します。TestModeの優先動作は維持します。
- 配布形態を変更しました。
- ランタイム不足の案内と、無効化可能な更新通知を追加。対応するZakkey MOD間で通知を順番に表示します。ブラウザーはボタン操作時だけ開き、自動ダウンロード・インストールは行いません。

## Installation / 導入

Install **Core + Runtime** from the same release. The Run Converter is optional and uses the installed Runtime. Keep existing INIs, Tracks, UG2MusicSFx, Pursuit and Cache when updating from 0.4.7. See Split-Install-0.4.8.md for instructions. No music is included.

同じ版の **CoreとRuntimeの両方** が必要です。The Run Converterは任意で、導入済みRuntimeを使用します。0.4.7からの更新では既存INI、Tracks、UG2MusicSFx、Pursuit、Cacheを保持してください。詳細はSplit-Install-0.4.8.mdをご覧ください。音源は含みません。

## Known issue / 既知の問題

The song HUD aspect correction may temporarily disappear after a pursuit ends, then recover. One report involved ORANGE PARADE with Japanese metadata; the cause is not confirmed. This issue is not fixed in 0.4.8.

追跡終了後に曲名HUDのアスペクト比補正が一時的に外れ、その後戻る場合があります。日本語を含むORANGE PARADEでの報告がありますが、原因は未特定です。0.4.8では未修正です。
