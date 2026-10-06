"""Create missing track sidecars without modifying audio or existing settings."""
from pathlib import Path
import configparser
import os
import re
import struct
import sys
import tempfile
import unicodedata

vendor = Path(__file__).resolve().parent / 'vendor'
if vendor.is_dir():
    sys.path.insert(0, str(vendor))
import mutagen
from pykakasi import kakasi
from jaconv import kata2hira

_romanizer = None
EXTENSIONS = {'.mp3', '.wav', '.m4a'}


def clean(value):
    return ' '.join(str(value).replace('\x00', '').split())[:512]


def romanize(value, fallback):
    """Use romaji, not translation; keep native byte-to-wide display ASCII-safe."""
    global _romanizer
    value = unicodedata.normalize('NFKC', clean(value))
    if any(ord(c) > 127 for c in value):
        if _romanizer is None:
            _romanizer = kakasi()
        value = ''.join(part['hepburn'] for part in _romanizer.convert(value))
    value = unicodedata.normalize('NFKD', value).encode('ascii', 'ignore').decode()
    value = clean(value).replace('\\', '/')[:95]
    return value or fallback


def read_tags(path):
    audio = mutagen.File(path)
    if audio is None or not getattr(audio, 'info', None):
        raise ValueError('unsupported audio metadata/container')
    tags = audio.tags or {}
    def field(*names):
        for name in names:
            value = tags.get(name)
            if hasattr(value, 'text'):
                value = value.text
            if isinstance(value, (list, tuple)):
                value = ' / '.join(str(v) for v in value)
            if value and clean(value):
                return clean(value)
        return ''
    result = {'Title': field('\xa9nam', 'TIT2', 'title'),
            'Artist': field('\xa9ART', 'TPE1', 'artist', 'aART', 'TPE2'),
            'Album': field('\xa9alb', 'TALB', 'album'),
            'TitleReading': field('sonm', 'TSOT', 'titlesort'),
            'ArtistReading': field('soar', 'TSOP', 'artistsort'),
            'AlbumReading': field('soal', 'TSOA', 'albumsort')}
    if Path(path).suffix.lower() == '.wav':
        # WAV commonly uses RIFF INFO instead of ID3. Never scan audio bytes.
        with Path(path).open('rb') as source:
            header = source.read(12)
            if header[:4] == b'RIFF' and header[8:] == b'WAVE':
                end = min(Path(path).stat().st_size, struct.unpack('<I', header[4:8])[0] + 8)
                while source.tell() + 8 <= end:
                    kind, size = struct.unpack('<4sI', source.read(8))
                    start = source.tell()
                    if start + size > end:
                        break
                    if kind == b'LIST' and 4 <= size <= 1048576:
                        data = source.read(size)
                        if data[:4] == b'INFO':
                            pos = 4
                            while pos + 8 <= len(data):
                                key, count = struct.unpack_from('<4sI', data, pos)
                                pos += 8
                                if pos + count > len(data):
                                    break
                                target = {b'INAM': 'Title', b'IART': 'Artist', b'IPRD': 'Album'}.get(key)
                                if target and not result[target]:
                                    raw = data[pos:pos + count].rstrip(b'\x00')
                                    try:
                                        result[target] = clean(raw.decode('utf-8-sig'))
                                    except UnicodeDecodeError:
                                        result[target] = clean(raw.decode('mbcs', errors='replace'))
                                pos += count + (count & 1)
                    source.seek(start + size + (size & 1))
    return result


def display_source(original, reading):
    # Japanese sort tags often carry an authoritative kana reading. Do not
    # replace an already English title with its Japanese sorting pronunciation.
    if original.isascii() or not reading or not any('\u3040' <= c <= '\u30ff' for c in reading):
        return original
    if any('\u4e00' <= c <= '\u9fff' for c in reading):
        return original
    for delimiter in (' & ', ' - '):
        left, right = original.split(delimiter), reading.split(delimiter)
        if len(left) > 1 and len(left) == len(right):
            return delimiter.join(a if a.isascii() else b for a, b in zip(left, right))
    if ' - ' in original and original.rsplit(' - ', 1)[1].isascii() and ' - ' not in reading:
        return reading + ' - ' + original.rsplit(' - ', 1)[1]
    return reading


def artist_display(original, reading):
    """Use supplied phonetics, retaining a surname/kana-name boundary when known."""
    if ' & ' in original and ' & ' in reading:
        left, right = original.split(' & '), reading.split(' & ')
        if len(left) == len(right):
            return ' & '.join(artist_display(a, b) for a, b in zip(left, right))
    if original.isascii():
        return original
    source = display_source(original, reading)
    suffix = re.search(r'([\u3041-\u3096\u30a1-\u30fa\u30fc]+)$', original)
    if source == reading and suffix and suffix.start() > 0:
        kana = kata2hira(suffix.group()).replace('ゑ', 'え').replace('ゐ', 'い')
        phonetic = kata2hira(reading).replace('ゑ', 'え').replace('ゐ', 'い')
        if phonetic.endswith(kana) and len(phonetic) > len(kana):
            return romanize(phonetic[:-len(kana)], '').capitalize() + ' ' + romanize(kana, '').capitalize()
    rendered = romanize(source, 'Custom')
    return rendered[:1].upper() + rendered[1:]


def create_sidecar(path, log=print, artist_readings=None):
    path = Path(path)
    sidecar = path.with_suffix('.ini')
    if sidecar.exists():
        return False
    try:
        tags = read_tags(path)
    except Exception as error:
        log(f'Metadata skipped: {path.name}: {error}')
        return False
    originals = {'Title': tags['Title'] or path.stem,
                 'Artist': tags['Artist'] or 'Custom',
                 'Album': tags['Album'] or 'NFSMW EA TRAX Expansion'}
    defaults = {'Title': 'Untitled', 'Artist': 'Custom', 'Album': 'NFSMW EA TRAX Expansion'}
    settings = configparser.ConfigParser(interpolation=None)
    settings.optionxform = str
    display = {}
    for key, value in originals.items():
        replaced = value
        for artist, rendered in sorted((artist_readings or {}).items(), key=lambda pair: -len(pair[0])):
            replaced = replaced.replace(artist, rendered)
        if replaced != value:
            display[key] = romanize(replaced, defaults[key])
        elif key == 'Artist':
            display[key] = artist_display(value, tags.get('ArtistReading', ''))[:95]
        else:
            display[key] = romanize(display_source(value, tags.get(key + 'Reading', '')), defaults[key])
    settings['Track'] = display
    settings['Track']['Mode'] = 'ALL'
    settings['OriginalMetadata'] = originals
    readings = {key: tags[key] for key in ('TitleReading', 'ArtistReading', 'AlbumReading') if tags.get(key)}
    if readings:
        settings['TagReadings'] = readings
    # Stage UTF-16 with BOM, then rename without replacement on Windows.
    # A user-created INI wins even if it appeared during metadata extraction.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-16', dir=path.parent,
                                         prefix='.eatrax-', suffix='.tmp', delete=False) as output:
            temporary = Path(output.name)
            output.write('; Generated from audio tags. Edit Track fields to customize display.\n'
                         '; 音源タグから生成。表示名はTrack内を編集してください。\n'
                         '; OriginalMetadata preserves original text; it is not displayed.\n'
                         '; 日本語の読みは推定です。必要に応じてローマ字表記を修正してください。\n\n')
            settings.write(output, space_around_delimiters=False)
        try:
            os.rename(temporary, sidecar)
        except FileExistsError:
            return False
        log(f'Metadata INI created: {sidecar.name}')
        return True
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def prepare_sidecars(mod, enabled=True, log=print, tracks_directory="Tracks"):
    if not enabled:
        return
    tracks = Path(mod) / tracks_directory
    files = [p for p in sorted(tracks.rglob('*'), key=lambda p: str(p).lower())
             if p.is_file() and p.suffix.lower() in EXTENSIONS]
    if all(p.with_suffix('.ini').exists() for p in files):
        return
    candidates = {}
    for path in files:
        try:
            tags = read_tags(path)
            originals, readings = tags['Artist'].split(' & '), tags['ArtistReading'].split(' & ')
            if len(originals) == len(readings):
                for original, reading in zip(originals, readings):
                    if original and not original.isascii() and display_source(original, reading) == reading:
                        candidates.setdefault(original, set()).add(artist_display(original, reading))
        except Exception:
            pass
    known = {original: next(iter(values)) for original, values in candidates.items() if len(values) == 1}
    for path in files:
        try:
            create_sidecar(path, log, known)
        except OSError as error:
            log(f'Metadata INI could not be written: {path.name}: {error}')
