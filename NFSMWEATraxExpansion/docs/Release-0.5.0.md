# EA TRAX Expansion 0.5.0

Install Core and the matching Runtime with the game closed. Keep your existing INIs, music, pursuit data and caches. Missing main-INI settings are added automatically on startup. The Run Converter is optional.

Changes since 0.4.8:
- At startup, add missing main-INI settings and descriptions from the embedded defaults. Existing values (including blank values), comments and unknown keys are retained; repeat startups do not rewrite a complete INI. Existing file encoding and line endings are preserved. Read-only/invalid files are retained without a partial rewrite.
- Added profile-specific persistence for explicit stock-song playback settings. Normal mode restores these overrides on startup; streamer mode does not overwrite them. Profiles without an override keep their native saved setting.
- Added `[Main] SwapArtistAlbum=false`. Set it to `true` to exchange Artist/Album presentation for all added songs using one main INI switch. Changes made while running apply on the next native list/HUD refresh. Song INIs, tags and caches are unchanged. A reversed native field layout has not been confirmed.
- Added `[Main] Logging=false` (default). Enable and restart for troubleshooting. Playback context, native timer observations and suspected gaps are logged; these observations do not force playback or prove a stall cause.
- Added optional HEAT 1 to 3 EA TRAX Keeping, including deferred camera/slow-motion/start sound until HEAT 4, with low-heat colour effects retained. Event-pursuit ownership and escape/re-engagement handling were adjusted. Enable using `[Pursuit] Heat1To3EATraxKeeping=true`.
- Added restart-applied streamer mode with separate Tracks/cache/settings. Normal stock-song settings are protected; stock and custom pursuit music remain available. Add your own suitable music to StreamerTracks before enabling it.
- Corrected compressed cache header handling at the signed frame-count boundary; retained compressed music caching and the 2 GiB bank safety limit. This is intended to avoid missing completion/next-song transitions caused by malformed long-song headers.
- Added exact-executable allowlisting for Redux 3.04's 4GB-patched English executable; unrecognized executables remain rejected.

## If playback stops

The intermittent playback stop reported in GitHub Issue #1 could not be reproduced in the development environment, so a complete resolution has not been confirmed. If it happens again, enable `[Main] Logging=true` and `PlaybackDiagnostics=true`, restart the game, and try to reproduce it. Report a summary of the location/game mode, the preceding action, the last song, and whether “Play Next Song” restores playback, together with relevant `TRAX_DIAG` log excerpts. Save `scripts/NFSMWEATraxExpansion/NFSMWEATraxExpansion.log` before another logged launch replaces it. Audio files and saves are not needed. `suspected_gap` is a diagnostic hint, not a confirmed cause.

## 日本語

ゲームを終了し、Coreと対応するRuntimeを導入してください。既存INI・追加音源・追跡データ・キャッシュは保持してください。本体INIの不足設定は次回起動時に自動追加されます。The Runコンバーターは任意です。

0.4.8からの変更点:
- 起動時に本体INIの不足項目と説明を自動補完します。既存値（空欄を含む）・コメント・独自項目・文字コード・改行を保持し、補完済みなら書き直しません。読み取り専用や不正なファイルは途中まで書き換えず、そのまま保持します。
- 通常モードで変更した純正曲の再生設定を、プロファイル別にINIにも保存して復元します。ストリーマーモードの一時的なOFFは通常設定に保存しません。
- 本体INIの`[Main] SwapArtistAlbum=true`で追加曲すべてのArtist/Album表示を一括交換できます。起動中の変更は次回の一覧・HUD更新時に反映。曲別INI・メタタグ・キャッシュは変更しません。ゲーム側フィールドの逆転は未確定です。
- `[Main] Logging=false`を規定にしました。trueにして再起動すると再生状態・タイマー観測・無音が疑われる状態を記録します。強制再生は行いません。
- HEAT 1～3でEA TRAXを維持する任意機能と、HEAT 4で開始演出を再生する処理を追加。イベント追跡、逃走後の再追跡の処理を調整しました。
- 通常用と別の音源・キャッシュ・設定を使用するストリーマーモードを追加。切り替えは再起動反映、純正曲設定は保護され、追跡BGMは共通です。
- 圧縮キャッシュの長い曲でのヘッダー値の扱いを修正。圧縮処理と2 GiBの安全上限を維持します。
- Redux 3.04用4GBパッチ済み英語EXEの厳密な許可リストを追加しました。未対応の実行ファイルは引き続き拒否します。

GitHub Issue #1で報告された断続的な再生停止は、開発環境では再現できず、完全な解消を確認していません。再発した場合は、本体INIの`[Main] Logging=true`と`PlaybackDiagnostics=true`で診断用ログを有効にし、ゲームを再起動して再現を試みてください。発生場所・ゲームモード・直前の操作・最後の曲・「次の曲を再生」で復帰するかを要約し、ログの関連する`TRAX_DIAG`行とあわせてご報告ください。ログは`scripts/NFSMWEATraxExpansion/NFSMWEATraxExpansion.log`に記録されます。次回のログ有効起動で置き換わるため、先に保存してください。音源・セーブの共有は不要です。`suspected_gap`は調査の手掛かりであり、原因確定ではありません。
