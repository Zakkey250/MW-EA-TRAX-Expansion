"""Real tagged AAC/ALAC/MP3/WAV fixtures; no user music is required."""
from pathlib import Path
import configparser
import hashlib
import subprocess
import sys
import tempfile
import unittest

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT / 'runtime'))
import track_metadata as metadata

FF = PROJECT.parent / 'staging/eatrax-046-ffmpeg/ffmpeg-8.1.2-essentials_build/bin/ffmpeg.exe'
PROBE = PROJECT / 'bin/Release/NFSMWEATraxProbe.exe'


class MetadataTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='eatrax-metadata-')
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def fixture(self, name, codec, tagged=True):
        path = self.root / name
        args = [str(FF), '-v', 'error', '-f', 'lavfi', '-i',
                'sine=frequency=440:duration=0.2', '-c:a', codec]
        if tagged:
            args += ['-metadata', 'title=夜空の星', '-metadata', 'artist=テスト Artist',
                     '-metadata', 'album=音楽 Album']
        subprocess.run(args + [str(path)], check=True, capture_output=True)
        return path

    def read(self, path):
        ini = configparser.ConfigParser(interpolation=None)
        ini.read(path.with_suffix('.ini'), encoding='utf-16')
        return ini

    def test_aac_and_alac(self):
        for codec in ['aac', 'alac']:
            path = self.fixture(codec + '.m4a', codec)
            digest = hashlib.sha256(path.read_bytes()).digest()
            subprocess.run([str(PROBE), '--probe-external', str(path)], check=True,
                           capture_output=True)
            self.assertTrue(metadata.create_sidecar(path))
            ini = self.read(path)
            self.assertEqual(ini['OriginalMetadata']['Title'], '夜空の星')
            self.assertEqual(ini['OriginalMetadata']['Artist'], 'テスト Artist')
            self.assertTrue(ini['Track']['Title'].isascii())
            self.assertIn('tesuto', ini['Track']['Artist'].lower())
            self.assertEqual(ini['Track']['Mode'], 'ALL')
            self.assertEqual(hashlib.sha256(path.read_bytes()).digest(), digest)

    def test_existing_ini_is_byte_preserved(self):
        path = self.fixture('manual.mp3', 'libmp3lame')
        sidecar = path.with_suffix('.ini')
        original = '[Track]\r\nTitle=手動設定\r\nMode=FE\r\n'.encode('utf-16')
        sidecar.write_bytes(original)
        self.assertFalse(metadata.create_sidecar(path))
        self.assertEqual(sidecar.read_bytes(), original)

    def test_untagged_filename_fallback(self):
        path = self.fixture('テスト.m4a', 'aac', False)
        self.assertTrue(metadata.create_sidecar(path))
        self.assertEqual(self.read(path)['Track']['Title'], 'tesuto')

    def test_mp3_and_wav(self):
        from mutagen.wave import WAVE
        from mutagen.id3 import TIT2, TPE1, TALB
        for name, codec in [('tag.mp3', 'libmp3lame'), ('tag.wav', 'pcm_s16le')]:
            path = self.fixture(name, codec)
            if path.suffix == '.wav':
                self.assertEqual(metadata.read_tags(path)['Title'], '夜空の星')
                audio = WAVE(path)
                audio.add_tags()
                audio.tags.add(TIT2(encoding=3, text=['夜空の星']))
                audio.tags.add(TPE1(encoding=3, text=['テスト Artist']))
                audio.tags.add(TALB(encoding=3, text=['音楽 Album']))
                audio.save()
            self.assertEqual(metadata.read_tags(path)['Title'], '夜空の星')

    def test_invalid_audio_does_not_create_ini(self):
        path = self.root / 'bad.m4a'
        path.write_bytes(b'not audio')
        self.assertFalse(metadata.create_sidecar(path))
        self.assertFalse(path.with_suffix('.ini').exists())
        result = subprocess.run([str(PROBE), '--probe-external', str(path)], capture_output=True)
        self.assertNotEqual(result.returncode, 0)

    def test_ascii_punctuation_and_injection(self):
        self.assertEqual(metadata.romanize('Rock & Roll 100%', 'x'), 'Rock & Roll 100%')
        self.assertEqual(metadata.romanize('Beyoncé', 'x'), 'Beyonce')
        self.assertNotIn('\n', metadata.romanize('Song\nMode=FE', 'x'))

    def test_kana_sort_readings(self):
        self.assertEqual(metadata.artist_display('響咲リオナ', 'いさきりおな'), 'Isaki Riona')
        self.assertEqual(metadata.artist_display('戌神ころね', 'いぬがみころね'), 'Inugami Korone')
        self.assertEqual(metadata.artist_display('角巻わため', 'つのまきわため'), 'Tsunomaki Watame')
        self.assertEqual(metadata.artist_display('宝鐘マリン', 'ほうしょうまりん'), 'Houshou Marin')
        self.assertEqual(metadata.display_source('Ring my name', 'りんぐまいねいむ'), 'Ring my name')
        self.assertEqual(metadata.romanize(metadata.display_source('響咲リオナ', 'いさきりおな'), ''), 'isakiriona')
        self.assertEqual(metadata.display_source('DYES IWASAKI & 宝鐘マリン', 'だいず いわさき & ほうしょうまりん'),
                         'DYES IWASAKI & ほうしょうまりん')
        self.assertEqual(metadata.display_source('曲 - Single', 'きょく'), 'きょく - Single')


if __name__ == '__main__':
    unittest.main()
