# Compressed cache investigation — 2026-09-11

## Confirmed defect / 確認した不具合

Version 0.4.6 generated PCM audio for imported EA TRAX and The Run pursuit
streams. In the measured installation this grew the combined bank to
2,421,528,788 bytes. Group 8's start sample (4104, graph node 4939) was at
2,411,410,432 bytes.

The verified NFSPatcher 4GB executable routes Pathfinder file playback through
0x82B4A0, 0x4BB183, 0x7EDB85 and 0x7EE912. The file backend's seek method at
0x7F00CC (vtable 0x8C2130 + 0x1C) calls SetFilePointer with a null high-distance
pointer. Its offset is therefore signed 32-bit. Repeating that API call against
the original cache, using a separate read-only handle, returned error 131
(ERROR_NEGATIVE_SEEK) for group 8. The compressed bank's corresponding offset
1,040,606,848 succeeded.

API reference: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-setfilepointer

これは4GB Patchで利用可能になるプロセスメモリ量とは別の、ファイル読み込み位置の互換性問題です。
0.4.6の非圧縮生成でこの上限を超える構成を作れてしまうことは重大な互換性不具合です。
攻撃や権限昇格を確認したという意味でのセキュリティ脆弱性ではありません。
追跡終了後に通常曲へ戻らないすべてのケースを、これだけで説明できたとは判断していません。

## Implementation

- Shared EA-XA v2 encoder in the existing NFSMWEATraxProbe helper; no new
  application dependency and no proprietary encoder is bundled.
- The converter writes compressed SCHl streams. Frame counts, graph records,
  events and authored gain remain unchanged.
- Cache format version 4 compresses both imported MP3/WAV/UG2 tracks and legacy
  PCM pursuit streams. Already compressed pursuit streams are copied directly.
- Version 2/3 PCM normal-track cache entries are regenerated once. Version 4
  compressed entries can be reused.
- Combined output above 0x7FFFFF80 bytes is rejected before publication.
- This is lossy audio compression, not transparent ZIP/NTFS compression. Original
  user audio and stock game banks remain unchanged.

## Offline validation

- 42 added normal tracks and 811 pursuit streams decoded completely by the
  independent vgmstream CLI as stereo 36 kHz EA-XA v2.
- Every frame count and MPF duration matched the previous PCM output. Graphs and
  events were byte-identical. Minimum measured SNR across 853 streams: 25.44 dB.
- Direct converter generation from the owned The Run source matched all 811
  legacy-cache-upgrade encoded streams byte for byte.
- CustomPursuit.mus: 529,604,564 -> 143,002,204 bytes.
- Combined EA_TRAX.mus: 2,421,528,788 -> 1,043,337,820 bytes.
- The Run-absent standalone configuration built successfully and hit its cache
  on the second run.
- An injected smaller output limit exercised the real failure path; all prior
  published cache outputs and Build.json stayed unchanged.
- Three Defender custom scans completed (runtime folder, probe and converter),
  with no new detections. This does not guarantee acceptance by other scanners.
- Existing vgmstream static library emits C4700 in maac.h during link-time code
  generation; no new encoder compile warning was emitted.

The reproduction artifacts and deployment rollback are in the directory named
by diagnostics/eatrax-compression-case.txt. Test-CompressedCache.py needs NumPy
for offline signal comparison; NumPy is not a runtime dependency.

## Runtime acceptance

On 2026-09-11 the user reported that the issues appeared resolved after gameplay
and would report any recurrence. This acceptance is separate from the API
reproduction and offline decoder checks above.
