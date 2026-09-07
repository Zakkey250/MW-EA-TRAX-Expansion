"""Create the separate developer source archive; no game audio is included."""
from pathlib import Path
import hashlib, json, zipfile

root=Path(__file__).resolve().parents[2]
project=root/'NFSMWEATraxExpansion'
items={}
def add(p, name):
    assert p.is_file(), p
    items[name]=p.read_bytes()
def tree(p, prefix, excluded=()):
    for f in p.rglob('*'):
        if f.is_file() and f.suffix.lower() not in excluded and not any(x in f.parts for x in ('.git','__pycache__')):
            add(f,prefix+'/'+f.relative_to(p).as_posix())

for directory in ('src','include','tests','distribution/Licenses','third_party'):
    tree(project/directory,'NFSMWEATraxExpansion/'+directory,('.pdb','.obj','.pyc'))
for name in ('README.md','NFSMWEATraxExpansion.sln','NFSMWEATraxExpansion.vcxproj','NFSMWEATraxProbe.vcxproj','StartupTests.vcxproj',
             'config/NFSMWEATraxExpansion.ini','config/MusicSFx.metadata.ini',
             'runtime/build_cache.py','runtime/native_bank.py','runtime/BuildCache.spec',
             'converter/TheRunConverter.cs','converter/recipe.json','converter/graph.mpf','converter/profile.ini',
             'tools/Build-046.ps1','tools/Package-046.py','tools/Package-Source-046.py','tools/Probe.cpp','tools/Test-HookSurface.ps1',
             'docs/BUILD-046.md','docs/Package-README.md','docs/User-Guide.md','docs/Converter-README.md',
             'docs/Tracks-README.txt','docs/UG2-README.txt','docs/FFmpeg-build-046.txt','docs/ThirdParty-Sources.md'):
    add(project/name,'NFSMWEATraxExpansion/'+name)
upstream=root/'downloads/vgmstream-src'
if not upstream.exists():upstream=root/'upstream/vgmstream'
for directory in ('src','ext_includes','cmake'):
    tree(upstream/directory,'upstream/vgmstream/'+directory,('.pdb','.obj','.lib','.exe','.dll'))
for name in ('CMakeLists.txt','COPYING','Directory.Build.props','version_auto.h','version-make.bat','version.h','vgmstream_msvc.props'):
    add(upstream/name,'upstream/vgmstream/'+name)
items['README.md']=b'# NFSMW EA TRAX Expansion 0.4.6 source\n\nStart with NFSMWEATraxExpansion/docs/BUILD-046.md.\nThe vgmstream source revision is e07a4a1f2d32658b8420e046276a7cfc425c9bf2.\nNo game audio or game executable is included. converter/graph.mpf is playback control metadata only.\n'
assert not any(Path(n).suffix.lower() in {'.mus','.wav','.mp3','.sps','.zip','.7z','.pdb','.obj'} for n in items)
items['SHA256SUMS.txt']=''.join(hashlib.sha256(b).hexdigest()+'  '+n+'\n' for n,b in sorted(items.items())).encode()
out=root/'dist/NFSMWEATraxExpansion-Source-v0.4.6.zip'
out.parent.mkdir(parents=True,exist_ok=True)
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
    for n,b in sorted(items.items()):z.writestr(n,b)
with zipfile.ZipFile(out) as z:assert z.testzip() is None
digest=hashlib.sha256(out.read_bytes()).hexdigest().upper()
out.with_suffix('.zip.sha256').write_text(digest+'  '+out.name+'\n',encoding='ascii')
report=dict(name=out.name,files=len(items),bytes=out.stat().st_size,sha256=digest)
out.with_suffix('.json').write_text(json.dumps(report,indent=2))
print(json.dumps(report,indent=2))
