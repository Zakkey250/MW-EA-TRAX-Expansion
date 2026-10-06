"""Three transparent 7z packages; no media, nested archives or EXE in Core."""
from pathlib import Path
import hashlib,json,subprocess,tempfile,shutil
import argparse
parser=argparse.ArgumentParser()
parser.add_argument('--core-only',action='store_true',help='Rebuild Core while retaining the previously verified Runtime and Converter')
parser.add_argument('--seven-zip',type=Path,default=Path('C:/Program Files/7-Zip/7z.exe'),help='Path to 7z.exe or standalone 7zr.exe')
args=parser.parse_args()
ROOT=Path(__file__).resolve().parents[2]; P=ROOT/'NFSMWEATraxExpansion'
OUT=ROOT/'dist'; BASE='scripts/NFSMWEATraxExpansion/'
SEVEN=args.seven_zip.resolve()
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
read=lambda p:p.read_bytes()
runtime={}
for f in (ROOT/'staging/eatrax-050-runtime/BuildCache').rglob('*'):
    if f.is_file():runtime[BASE+'Runtime/'+f.relative_to(ROOT/'staging/eatrax-050-runtime/BuildCache').as_posix()]=read(f)
for name,path in {
    'NFSMWEATraxProbe.exe':P/'bin/Release/NFSMWEATraxProbe.exe',
    'libmpg123-0.dll':P/'bin/Release/libmpg123-0.dll',
    'ffmpeg.exe':ROOT/'staging/eatrax-046-ffmpeg/ffmpeg-8.1.2-essentials_build/bin/ffmpeg.exe',
}.items():runtime[BASE+'Runtime/'+name]=read(path)
inventory={'schema':1,'version':'0.5.0','files':[{'path':name[len(BASE):],'bytes':len(b),'sha256':sha(b)} for name,b in sorted(runtime.items())]}
core={}
for name,path in {
    'scripts/NFSMWEATraxExpansion.asi':P/'bin/Release/NFSMWEATraxExpansion.asi',
    BASE+'libmpg123-0.dll':P/'third_party/mpg123/bin/Win32/libmpg123-0.dll',
    BASE+'NFSMWEATraxExpansion.ini':P/'config/NFSMWEATraxExpansion.ini',
    BASE+'UG2MusicSFx/MusicSFx.metadata.ini':P/'config/MusicSFx.metadata.ini',
    BASE+'StreamerTracks/README.md':P/'docs/Streamer-Mode.md',
    BASE+'Tracks/README.txt':P/'docs/Tracks-README.txt',
    BASE+'UG2MusicSFx/README.txt':P/'docs/UG2-README.txt',
    BASE+'README.md':P/'docs/User-Guide.md',
    'README.md':P/'docs/Split-Install-0.5.0.md',
    'Release-Notes.md':P/'docs/Release-0.5.0.md',
}.items():core[name]=read(path)
core[BASE+'RuntimeRequired.json']=(json.dumps(inventory,indent=2)+'\n').encode()
core[BASE+'Pursuit/README.txt']=b'The Run music is optional. Obtain the converter separately from GitHub. See README.md.\n'
for f in (P/'distribution/Licenses').iterdir():
    if f.is_file():runtime[BASE+'Licenses/'+f.name]=read(f)
for name in ['minhook.txt','miniaudio.txt','mpg123.txt','vgmstream.txt']:
    core[BASE+'Licenses/'+name]=read(P/'distribution/Licenses'/name)
core[BASE+'Licenses/nlohmann-json.txt']=read(P/'third_party/nlohmann/LICENSE.MIT')
runtime[BASE+'Licenses/ThirdParty-Sources.md']=read(P/'docs/ThirdParty-Sources.md')
runtime['Runtime-Install.md']=read(P/'docs/Split-Install-0.5.0.md')
converter={'TheRunPursuitConverter.exe':read(P/'converter/TheRunPursuitConverter.exe'),
           'README.md':read(P/'docs/Converter-README.md'),
           'Split-Install.md':read(P/'docs/Split-Install-0.5.0.md'),
           'Licenses/vgmstream.txt':read(P/'distribution/Licenses/vgmstream.txt')}
def package(name,files,is_core=False):
    forbidden={'.zip','.7z','.rar','.mp3','.wav','.m4a','.mus','.mpf','.sps','.log','.pdb','.obj'}
    for name_,b in files.items():
        suffix=Path(name_).suffix.lower()
        assert suffix not in forbidden and '/Cache/' not in name_ and not name_.endswith('State.ini'),name_
        if is_core:
            assert suffix in {'.asi','.dll','.ini','.json','.md','.txt'},name_
            if b[:2]==b'MZ':
                pe=int.from_bytes(b[60:64],'little'); flags=int.from_bytes(b[pe+22:pe+24],'little')
                assert b[pe:pe+4]==b'PE\0\0' and flags&0x2000 and suffix in {'.asi','.dll'},name_
        assert not any(b.startswith(sig) for sig in (b'PK\x03\x04',b'7z\xbc\xaf\x27\x1c',b'Rar!')),name_
    files=dict(files);files['SHA256SUMS.txt']=''.join(sha(b)+'  '+n+'\n' for n,b in sorted(files.items())).encode()
    dest=OUT/name
    if dest.exists():raise RuntimeError(f'Refusing to reuse an existing package directory: {dest}')
    dest.mkdir(parents=True)
    for n,b in files.items():
        f=dest/n;f.parent.mkdir(parents=True,exist_ok=True);f.write_bytes(b)
    archive=OUT/(name+'.7z')
    if archive.exists():raise RuntimeError(f'Archive already exists: {archive}')
    subprocess.run([SEVEN,'a','-t7z','-mx=7','-ms=off',archive,'.'],cwd=dest,check=True,capture_output=True)
    subprocess.run([SEVEN,'t',archive],check=True,capture_output=True)
    with tempfile.TemporaryDirectory(prefix='eatrax050-audit-') as temp:
        subprocess.run([SEVEN,'x',archive,'-o'+temp,'-y'],check=True,capture_output=True)
        extracted={f.relative_to(temp).as_posix():f.read_bytes() for f in Path(temp).rglob('*') if f.is_file()}
        assert extracted==files
    digest=sha(archive.read_bytes());archive.with_suffix('.7z.sha256').write_text(digest+'  '+archive.name+'\n')
    return {'name':archive.name,'files':len(files),'bytes':archive.stat().st_size,'sha256':digest,'audit':'extracted bytes match; no media or nested archives','coreNoExe':is_core}
reports=[package('NFSMWEATraxExpansion-v0.5.0-Core',core,True)]
if args.core_only:
    previous=json.loads((OUT/'packages-0.5.0.json').read_text())
    reports.extend(p for p in previous if p['name']!='NFSMWEATraxExpansion-v0.5.0-Core.7z')
else:
    reports.extend([package('NFSMWEATraxExpansion-v0.5.0-Runtime',runtime),package('TheRunPursuitConverter-v0.5.0',converter)])
(OUT/'packages-0.5.0.json').write_text(json.dumps(reports,indent=2)+'\n')
print(json.dumps(reports,indent=2))
