# Streamer Mode / ストリーマーモード

Keep a separate selection of music for streaming. Add music you can use in your stream to `StreamerTracks` (MP3, WAV or M4A; subfolders supported).

With the game closed, edit `NFSMWEATraxExpansion.ini`:

```ini
[Main]
StreamerMode = true
```

- `true`: play only songs from `StreamerTracks` in EA TRAX. Stock songs, normal `Tracks` songs and the UG2 import are excluded. Stock-song previews are silent.
- `false` (default): use the normal music selection and its saved settings.
- The change takes effect on the next game launch. There is no in-game menu switch.
- Each profile generates its cache on first use. When generation finishes, acknowledge the notice and restart. Once both caches are ready, changing only this switch does not regenerate either cache. Changing source music, volume or the common pursuit pack can require a rebuild of the affected profile.
- Normal music uses `Tracks`, `Cache` and `State.ini`; streamer music uses `StreamerTracks`, `StreamerCache` and `StreamerState.ini`. Do not exchange or rename the caches.
- Stock-song OFF settings in streamer mode are temporary. They are not written to your normal save. Normal stock-song settings return when streamer mode is disabled. Added-song playback settings are saved separately for each profile.
- Stock and custom pursuit music use the same existing `[Pursuit]` settings and `Pursuit` source folder in both modes. Put only ordinary songs in `StreamerTracks`; do not put a pursuit pack there. Pursuit bank data is incorporated into each prepared bank for the native game player.
- If streamer music is missing, preparation is cancelled, or the selected bank cannot be verified, a notice closes the game instead of falling back to stock songs. Check `StreamerCache/Build.log`, or disable `StreamerMode` to return to normal mode.

---

配信用の音楽を普段の曲とは別に管理します。配信で利用できる音源を `StreamerTracks` に配置してください。MP3・WAV・M4Aとサブフォルダーに対応しています。

ゲームを終了してから、`NFSMWEATraxExpansion.ini` の `[Main]` にある `StreamerMode` を変更します。

- `true`：EA TRAXでは `StreamerTracks` の曲だけを再生します。純正曲・通常の `Tracks`・UG2の追加曲は選曲対象外です。純正曲の試聴も無音になります。
- `false`（初期値）：通常用の音楽と保存済み設定を使用します。
- 次回起動時に反映します。ゲーム内メニューからの切り替えはありません。
- 各モードの初回使用時に、それぞれキャッシュを生成します。完了通知に従って再起動してください。両方の生成が済んでいれば、スイッチだけの変更では再生成しません。音源・音量・共通の追跡パックを変更した場合は、対象キャッシュの更新が必要になることがあります。
- 通常用は `Tracks`・`Cache`・`State.ini`、配信用は `StreamerTracks`・`StreamerCache`・`StreamerState.ini` を使用します。キャッシュを交換・改名しないでください。
- 配信中の純正曲OFFは一時的な状態で、通常セーブには保存しません。通常モードへ戻すと元の純正曲設定を使用します。追加曲の再生設定もモード別に保存します。
- 純正／カスタム追跡BGMは、両モードとも既存の `[Pursuit]` 設定と `Pursuit` フォルダーを共用します。`StreamerTracks` には通常曲だけを置いてください。ゲーム標準の再生機構を使うため、共通の追跡バンクデータは各生成済みバンクに組み込まれます。
- 専用曲がない場合、生成を中止した場合、キャッシュ検証に失敗した場合は、通知してゲームを終了します。純正曲へ自動復帰はしません。`StreamerCache/Build.log` を確認するか、`StreamerMode=false` に戻してください。
