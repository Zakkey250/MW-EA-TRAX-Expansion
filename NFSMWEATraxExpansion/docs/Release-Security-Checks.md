# 配布前のセキュリティ確認 / Release security checks

開発・配布担当者向けの手順です。利用者向けの導入手順はUser-Guide.mdを参照してください。
This document is for maintainers. Player installation instructions are in User-Guide.md.

## 原則 / Principles

- 配布ZIPを確定してから、そのZIPと同じバイト列のEXEをスキャンし、SHA-256と検査日時・Defenderの定義バージョンを記録します。スキャン後にビルド・圧縮し直した場合は再確認します。
- Scan the finalized ZIPs and their exact executable files. Record SHA-256, scan time and Defender signature version; recheck changed artifacts.
- 配布物の検出と、開発用シェルコマンドの検出を区別します。検出履歴の対象リソースを確認してください。
- Distinguish a distributed file detection from a developer command-line detection by checking the recorded resource.
- PowerShellでアセンブリを動的ロードして非公開メソッドを呼ぶ検証方法は使用しません。コンバーターの通常の変換操作、または明示的なテストプログラムを使用します。
- Do not test by dynamically loading the assembly and invoking private methods through PowerShell. Use the converter's normal conversion operation or a dedicated test program.
- ゲーム音源、セーブ、キャッシュ、検証用コマンド、入れ子ZIPを利用者向けパッケージへ含めません。既存のPackage-047.pyの検査を維持します。
- Keep game audio, saves, caches, developer test commands and nested ZIPs out of player packages. Retain the checks in Package-047.py.
- ウイルス対策の無効化・除外設定・警告回避用のリネームは導入手順にしません。
- Do not instruct users to disable protection, create exclusions, or rename files to bypass warnings.

## 検査と記録 / Checks and records

1. 本体、任意コンバーター、ソースを用途別に用意し、通常のZIPを使用します。
2. 最終ファイルのSHA-256と署名状態を確認します。署名なしの場合、署名済み・発行元検証済みと表示しません。
3. Microsoft Defenderで対象ファイルのカスタムスキャンを行います。
4. スキャン開始・終了の記録と、その期間の検出履歴を確認します。コマンドの終了だけで「検査完了・検出なし」と判断しません。
5. 検出があった場合は対象、検出名、ハッシュ、製品・定義バージョンを記録し、未解決のまま公開用の合格扱いにしません。
6. ビルド手順、対応ソース、第三者コンポーネントのライセンスを添付・リンクします。

Prepare separate main, optional-converter and source downloads. Verify hashes and signing status; run file-specific scans; confirm scan-completion records and detections. Record any detection before proceeding with release review. Provide corresponding source, build instructions and third-party licenses.

スキャン結果は、その時点の環境・定義・対象ファイルについての結果です。全ての環境での非検出、SmartScreenの評価、Nexusの承認、ゲーム内動作を保証するものではありません。
Results apply to the checked files and definitions at the recorded time. They do not guarantee detection outcomes on every system, SmartScreen reputation, Nexus approval or correct in-game behavior.

## 検出の審査申請 / Detection review

実際に配布ファイルが誤検知される場合は、Microsoftへソフトウェア開発者として対象ファイルと情報を提出します。提出は公開・送信の承認を得た担当者が行ってください。独自のホワイトリストや無効化を要求しません。
If a distributed file is incorrectly detected, an authorized maintainer should submit the exact file and supporting information to Microsoft as its software developer.

- Microsoft developer FAQ: https://learn.microsoft.com/en-us/defender-xdr/developer-faq
- File submission: https://www.microsoft.com/en-us/wdsi/filesubmission

Nexusの隔離審査は別手続きです。Defenderの非検出をNexusの承認として扱わないでください。
Nexus quarantine review is a separate process; a clean Defender scan is not Nexus approval.
