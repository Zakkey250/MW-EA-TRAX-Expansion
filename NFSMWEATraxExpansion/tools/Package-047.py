"""Build audited 0.4.7 player and optional converter ZIPs without audio or nested archives."""
from pathlib import Path
import hashlib,zipfile,json
ROOT=Path(__file__).resolve().parents[2];PROJECT=ROOT/'NFSMWEATraxExpansion'
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
prefix='scripts/NFSMWEATraxExpansion/'
files={}
# Explicit source inputs: no dependency on an older music-containing release.
for p in (PROJECT/'distribution/Licenses').iterdir():
 if p.is_file():files[prefix+'Licenses/'+p.name]=p.read_bytes()
files[prefix+'libmpg123-0.dll']=(PROJECT/'third_party/mpg123/bin/Win32/libmpg123-0.dll').read_bytes()
files[prefix+'UG2MusicSFx/MusicSFx.metadata.ini']=(PROJECT/'config/MusicSFx.metadata.ini').read_bytes()
for destination,source in {
 'README.md':'docs/Package-README.md','Release-Notes.md':'docs/Release-0.4.7.md',prefix+'README.md':'docs/User-Guide.md',
 prefix+'NFSMWEATraxExpansion.ini':'config/NFSMWEATraxExpansion.ini',
 prefix+'Tracks/README.txt':'docs/Tracks-README.txt',prefix+'UG2MusicSFx/README.txt':'docs/UG2-README.txt',
 'scripts/NFSMWEATraxExpansion.asi':'bin/Release/NFSMWEATraxExpansion.asi',
 prefix+'Runtime/NFSMWEATraxProbe.exe':'bin/Release/NFSMWEATraxProbe.exe',
 prefix+'Runtime/libmpg123-0.dll':'bin/Release/libmpg123-0.dll',
}.items():files[destination]=(PROJECT/source).read_bytes()
runtime=ROOT/'staging/eatrax-047-runtime/BuildCache'
assert (runtime/'BuildCache.exe').is_file(), 'Build the runtime first'
for p in runtime.rglob('*'):
 if p.is_file():files[prefix+'Runtime/'+p.relative_to(runtime).as_posix()]=p.read_bytes()
ff=ROOT/'staging/eatrax-046-ffmpeg/ffmpeg-8.1.2-essentials_build'
files[prefix+'Runtime/ffmpeg.exe']=(ff/'bin/ffmpeg.exe').read_bytes()
files[prefix+'Licenses/FFmpeg.txt']=(ff/'LICENSE').read_bytes()
files[prefix+'Licenses/FFmpeg-build.txt']=(PROJECT/'docs/FFmpeg-build-046.txt').read_bytes()
files[prefix+'Licenses/ThirdParty-Sources.md']=(PROJECT/'docs/ThirdParty-Sources.md').read_bytes()
files[prefix+'Pursuit/README.txt']='''The Run audio is optional and is not included.
Install it using the separately supplied TheRunPursuitConverter.exe.
Until then, pursuits use stock MW music. See ../README.md.

The Run音源は任意の追加機能で、本体には含みません。
別配布のTheRunPursuitConverter.exeで導入してください。
導入するまではMW標準の追跡BGMを使用します。詳細はひとつ上のREADME.mdをご覧ください。
'''.encode('utf8')
def package(name,items):
 assert not any(Path(n).suffix.lower() in {'.zip','.7z','.rar','.mp3','.wav','.sps','.mus','.mpf'} for n in items)
 assert not any('/Cache/' in n or n.endswith(('State.ini','.log','.pdb','.obj')) for n in items)
 items=dict(items);items['SHA256SUMS.txt']=(''.join(sha(b)+'  '+n+'\n' for n,b in sorted(items.items()))).encode()
 out=ROOT/'dist'/name;out.mkdir(parents=True,exist_ok=True)
 for n,b in items.items():
  p=out/n;assert p.resolve().is_relative_to(out.resolve());p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(b)
 # Refuse stale output files instead of distributing an accidental previous build.
 assert {p.relative_to(out).as_posix() for p in out.rglob('*') if p.is_file()}==set(items)
 zip_path=out.parent/(out.name+'.zip')
 with zipfile.ZipFile(zip_path,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
  for n,b in sorted(items.items()):z.writestr(n,b)
 with zipfile.ZipFile(zip_path) as z:
  assert z.testzip() is None
  for n,b in items.items():assert z.read(n)==b
 digest=sha(zip_path.read_bytes());zip_path.with_suffix('.zip.sha256').write_text(digest+'  '+zip_path.name+'\n',encoding='ascii')
 return dict(name=name,files=len(items),bytes=zip_path.stat().st_size,sha256=digest,noAudio=True,noNestedArchives=True)
reports=[package('NFSMWEATraxExpansion-v0.4.7',files)]
reports.append(package('TheRunPursuitConverter-v0.4.7',{
 'TheRunPursuitConverter.exe':(PROJECT/'converter/TheRunPursuitConverter.exe').read_bytes(),
 'README.md':(PROJECT/'docs/Converter-README.md').read_bytes(),
 'Licenses/vgmstream.txt':files[prefix+'Licenses/vgmstream.txt'],
}))
(ROOT/'dist/packages-0.4.7.json').write_text(json.dumps(reports,indent=2))
case_pointer=ROOT/'diagnostics/eatrax-047-case.txt'
if case_pointer.exists():
 case=Path(case_pointer.read_text().strip());(case/'packages.json').write_text(json.dumps(reports,indent=2))
print(json.dumps(reports,indent=2))
