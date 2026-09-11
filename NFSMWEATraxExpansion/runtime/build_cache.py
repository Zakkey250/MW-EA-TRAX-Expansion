"""Automatically prepare a local native bank. Never distribute Cache or user audio."""
import argparse,configparser,hashlib,json,math,msvcrt,os,re,shutil,struct,subprocess,sys,tempfile,time
from pathlib import Path
import native_bank as n
VERSION=4
# Conservative native streaming boundary. This is a compatibility guard, not
# a claim that every native read path has a proven signed 2 GiB limit.
MAX_BANK_BYTES=0x7fffff80

def source_key(job, volume):
    return (job['kind'], job['hash'], job['subsong'], volume)

def copy_stream(source, destination, offset):
    """Copy a previously encoded SCHl stream without decoding or remeasuring."""
    source.seek(offset)
    first=True
    while True:
        header=source.read(8)
        if len(header)!=8:raise ValueError('Truncated cached stream')
        tag,size=struct.unpack('<4sI',header)
        if size<8 or size>1048576 or (first and tag!=b'SCHl'):raise ValueError('Invalid cached stream')
        body=source.read(size-8)
        if len(body)!=size-8:raise ValueError('Truncated cached block')
        destination.write(header);destination.write(body);first=False
        if tag==b'SCEl':return
def sha(path):
    with path.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest().upper()
def config(path):
    c=configparser.ConfigParser(interpolation=None);c.optionxform=str
    if path.exists():c.read(path,encoding='utf-8-sig')
    return c
def run(args):
    p=subprocess.run(list(map(str,args)),capture_output=True,timeout=600,creationflags=subprocess.CREATE_NO_WINDOW)
    if p.returncode:raise RuntimeError(f'{Path(args[0]).name}: '+p.stderr.decode(errors='replace')[-1500:]+p.stdout.decode(errors='replace')[-500:])
    return p
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--mod-root',type=Path,required=True);args=ap.parse_args()
    mod=args.mod_root.resolve();game=mod.parents[1];cache=mod/'Cache';cache.mkdir(exist_ok=True)
    runtime=mod/'Runtime';probe=runtime/'NFSMWEATraxProbe.exe';ff=runtime/'ffmpeg.exe'
    with (cache/'build.lock').open('a+b') as lock:
        lock.seek(0);lock.write(b'0');lock.flush();lock.seek(0)
        msvcrt.locking(lock.fileno(),msvcrt.LK_NBLCK,1)
        log=(cache/'Build.log').open('a',encoding='utf-8',buffering=1)
        def say(s):log.write(time.strftime('%Y-%m-%d %H:%M:%S ')+s+'\n');print(s,flush=True)
        try:
            ini=config(mod/'NFSMWEATraxExpansion.ini');volume=ini.getfloat('Main','VolumeMultiplier',fallback=1)
            if not math.isfinite(volume) or not 0<=volume<=2:raise ValueError('VolumeMultiplier must be 0..2')
            jobs=json.loads(run([probe,'--export-native-jobs',mod]).stdout.decode('utf-8-sig'))
            if len(jobs)>94:raise ValueError('Native bank supports at most 94 added songs with this pursuit pack')
            hashfile=cache/'FileHashes.json'
            saved_hashes=json.loads(hashfile.read_text()) if hashfile.exists() else {}
            hashes={};hash_reads=0
            def stamp(path):
                st=path.stat();return [st.st_size,st.st_mtime_ns]
            def identity(path):
                nonlocal hash_reads
                path=path.resolve();key=str(path);stat=stamp(path)
                if key in hashes:return hashes[key]['hash']
                old=saved_hashes.get(key,{})
                h=old.get('hash') if old.get('stat')==stat else None
                if not h:h=sha(path);hash_reads+=1
                hashes[key]={'stat':stat,'hash':h};return h
            stock=game/'SOUND/PFDATA/MW_Music.mpf';stockmus=stock.with_suffix('.mus')
            basis={'mpf':identity(stock),'mus':identity(stockmus)}
            # Existing measured reference, valid only for this exact owned stock pair.
            if basis!={'mpf':'15C7FDAA626940319A74965D4D68DB1507475BA743D574C5AA6DE214482E785B','mus':'BB191D34C4C3AC3B5BD33A9E02049A08DF13014E4FB5C8D7D07320ABAE1C5811'}:
                raise ValueError('Unsupported vanilla music identity')
            for j in jobs:
                path=Path(j['path']);j['hash']=identity(path)
                if j['kind']=='musicsfx':j['hash']+='_'+identity(path.with_suffix('.mus'))
            packdir=mod/'Pursuit';packfile=packdir/'CustomPursuit.mpf';packmus=packfile.with_suffix('.mus');packini=packdir/'Pursuit.ini'
            present=[p.exists() for p in [packfile,packmus,packini]]
            if any(present) and not all(present):raise ValueError('Incomplete custom pursuit pack')
            packcfg=config(packini);packhash={}
            if not any(present):say('EA TRAX standalone: no optional The Run pursuit pack')
            if all(present):
                for name,path in [('Mpf',packfile),('Mus',packmus)]:
                    packhash[name]=identity(path)
                    if packhash[name]!=packcfg.get('PursuitPack',name+'SHA256'):raise ValueError('Custom pursuit pack hash mismatch')
                if volume!=packcfg.getfloat('PursuitPack','VolumeMultiplier'):say('Pursuit source gain remains authored at '+packcfg.get('PursuitPack','VolumeMultiplier'))
                packhash['ini']=identity(packini)
            signature=dict(version=VERSION,basis=basis,jobs=jobs,volume=volume,pursuit=packhash)
            if hashes!=saved_hashes:
                (cache/'FileHashes.tmp').write_text(json.dumps(hashes,indent=2),encoding='utf-8');os.replace(cache/'FileHashes.tmp',hashfile)
            fingerprint=hashlib.sha256(json.dumps(signature,sort_keys=True).encode()).hexdigest()
            record=cache/'Build.json'
            previous={};reusable={}
            if record.exists():
                previous=json.loads(record.read_text())
                valid_outputs=all((cache/name).exists() and stamp(cache/name)==previous.get('output_stats',{}).get(name) for name in previous.get('outputs',{})) and len(previous.get('outputs',{}))==3
                if previous.get('fingerprint')==fingerprint and valid_outputs:
                    say(f'CACHE HIT: {len(jobs)} songs; source_hash_reads={hash_reads}; bank_reads=0');return 0
                old=previous.get('signature',{})
                if valid_outputs and old.get('version') == VERSION:
                    old_samples=n.samples((cache/'EA_TRAX.mpf').read_bytes())
                    for job,measurement in zip(old.get('jobs',[]),previous.get('measurements',[])):
                        reusable[source_key(job,old.get('volume'))]=(old_samples[measurement['sample']-1][0]*128,measurement)
            say(f'Building native cache: {len(jobs)} songs. Stock files remain read-only.')
            with tempfile.TemporaryDirectory(prefix='build-',dir=cache) as td:
                work=Path(td);b=stock.read_bytes();ns=n.nodes(b);es=n.events(b);ss=n.samples(b)
                if len(ns)!=3681 or len(ss)!=3257 or len(es)!=70:raise ValueError('Stock layout mismatch')
                outmus=work/'EA_TRAX.mus';shutil.copyfile(stockmus,outmus)
                profile=config(Path('missing-profile'));profile['NativeMusic']={'Version':'2','LegacyPursuitSources':'0','VolumeMultiplier':str(volume),'TrackCount':str(len(jobs))}
                template=next(e for e in es if n.u32(e,12)&0xffffff==0xad947)
                templates=[bytearray(ns[i]) for i in [1490,1491,1492]]
                measurements=[]
                with outmus.open('ab') as dest:
                    for index,j in enumerate(jobs):
                        source=Path(j['path']);pcm=work/'source.wav';normalized=work/'normalized.wav';stream=work/'stream.asf'
                        reused=reusable.get(source_key(j,volume))
                        dest.write(bytes((-dest.tell())%128));offset=dest.tell()
                        if reused:
                            old_offset,measurement=reused
                            with (cache/'EA_TRAX.mus').open('rb') as prior:
                                copy_stream(prior,dest,old_offset)
                            duration=measurement['duration_ms'];lufs=measurement['lufs'];gain=measurement['gain_db']
                        else:
                            if j['kind']=='musicsfx':run([probe,'--dump-musicsfx',source,j['subsong'],work/'decoded.wav']);source=work/'decoded.wav'
                            run([ff,'-v','error','-y','-threads','1','-i',source,'-vn','-ac','2','-ar','36000','-c:a','pcm_s16le',pcm])
                            text=run([ff,'-hide_banner','-nostats','-threads','1','-i',pcm,'-af','loudnorm=I=-8.6:TP=-1:LRA=11:print_format=json','-f','null','-']).stderr.decode(errors='replace')
                            stats=json.loads(re.search(r'\{\s*"input_i".*?\}',text,re.S)[0]);lufs=float(stats['input_i'])
                            gain=max(-60,min(12,-8.6-lufs)) if math.isfinite(lufs) else 0
                            amplitude=10**(gain/20)*volume
                            run([ff,'-v','error','-y','-threads','1','-i',pcm,'-af',f'volume={amplitude:.9f},alimiter=limit=0.98:level=0:latency=1','-c:a','pcm_s16le',normalized])
                            duration=n.compressed_stream(normalized,stream,probe,run)
                            with stream.open('rb') as encoded:shutil.copyfileobj(encoded,dest)
                        ss.append((offset//128,duration));sample=len(ss);root=len(ns);event=0xe00000+index
                        for pos,t in enumerate(templates):
                            r=bytearray(t);n.p32(r,0,([0,-1,sample][pos]&65535)|(5<<21));struct.pack_into('<H',r,8,root if pos!=1 else root+1)
                            if pos!=1:struct.pack_into('<H',r,18,root+2 if pos==0 else root+1)
                            ns.append(r)
                        e=bytearray(template);n.p32(e,12,(n.u32(e,12)&0xff000000)|event);struct.pack_into('<H',e,40,root);es.append(e)
                        profile[f'Source_{j["hash"]}_{j["subsong"]}_{index}']={'EventID':hex(event),'Sample':str(sample)}
                        measurements.append(dict(path=j['path'],lufs=lufs if lufs is not None and math.isfinite(lufs) else None,gain_db=gain,sample=sample,duration_ms=duration))
                        say(f'{"Reused" if reused else "Encoded"} {index+1}/{len(jobs)}: {Path(j["path"]).name} subsong={j["subsong"]}')
                    if all(present):
                        pack=packfile.read_bytes();pn=n.nodes(pack);ps=n.samples(pack);nodebase=len(ns);samplebase=len(ss)
                        nm={i:nodebase+i for i in range(len(pn))};sm={i+1:samplebase+i+1 for i in range(len(ps))}
                        with packmus.open('rb') as packed:
                            for index,(off,duration) in enumerate(ps):
                                dest.write(bytes((-dest.tell())%128));start=dest.tell()
                                raw=work/'pursuit.pcm';stream=work/'pursuit.asf'
                                frames=n.extract_pcm_stream(packed,off*128,raw)
                                if frames is None:copy_stream(packed,dest,off*128)
                                else:
                                    run([probe,'--encode-eaxa',raw,0,frames,stream])
                                    with stream.open('rb') as encoded:shutil.copyfileobj(encoded,dest)
                                ss.append((start//128,duration))
                                if index%40==0:say(f'Compressed pursuit {index+1}/{len(ps)}')
                        ns.extend(n.relocate_node(r,nm,sm) for r in pn);es.extend(n.relocate_event(e,nm) for e in n.events(pack))
                        for section in packcfg.sections():
                            if section.startswith('Adaptive'):
                                profile[section]={k:str(nm[int(v)]) if k.startswith(('NativePart','NativeIntensityPart')) else v for k,v in packcfg[section].items()}
                # Retain native fades/FX. Remove graph-changing actions for custom scores.
                originals=n.events(b);wait=next(e[o:o+12] for e in originals for o in range(20,len(e),12) if (n.u32(e,o+4)>>8)&255==2)
                interactive={int(x,16) for x in '6DD6BB 9DF7DA E6FF17 7690D2 96E300 46EDA3 B2D374 9509F2 919B1B E40616 1BBC15 34209F 2F7671 C505B7 27205F 4C6876 E2814E B39639 518C15 E9222F C0DC6F EA0327 1B6B71 B29728 E391AF DF442E D874B1 D12570 32F37E 56667B 515634 580861 D2E818 9B7A50 7B768A 4F246E C3FA91 2BBA48 22E859 90B6DC 641F27 6E7282 531659'.split()}
                controls={}
                for original in originals:
                    eid=n.u32(original,12)&0xffffff
                    if eid not in interactive:continue
                    replacement=0xe10000+len(controls);e=bytearray(original);n.p32(e,12,(n.u32(e,12)&0xff000000)|replacement)
                    for o in range(20,len(e),12):
                        if (n.u32(e,o+4)>>8)&255==4:e[o+4:o+12]=wait[4:];n.p32(e,o+8,0)
                    controls[f'{eid:06X}']=hex(replacement);es.append(e)
                profile['PursuitControls']=controls
                if outmus.stat().st_size>MAX_BANK_BYTES:
                    raise ValueError('Compressed cache exceeds the 2 GiB safety limit. Reduce added tracks. / 圧縮後もキャッシュが2 GiBの安全上限を超えています。追加曲を減らしてください。')
                out=n.serialize(b,ns,es,ss);(work/'EA_TRAX.mpf').write_bytes(out)
                profile['NativeMusic']['MpfSHA256']=sha(work/'EA_TRAX.mpf');profile['NativeMusic']['MusSHA256']=sha(outmus)
                with (work/'NativeMusic.ini').open('w',encoding='ascii') as f:profile.write(f,space_around_delimiters=False)
                for source,identity_record in hashes.items():
                    if stamp(Path(source))!=identity_record['stat']:raise ValueError('Source changed during conversion: '+source)
                # Commit profile last; partial publication cannot pass the hash gate.
                outputs={name:sha(work/name) for name in ['EA_TRAX.mpf','EA_TRAX.mus','NativeMusic.ini']}
                for name in outputs:os.replace(work/name,cache/name)
                payload=dict(fingerprint=fingerprint,outputs=outputs,output_stats={name:stamp(cache/name) for name in outputs},signature=signature,measurements=measurements,nodes=len(ns),samples=len(ss),events=len(es))
                (cache/'Build.tmp').write_text(json.dumps(payload,indent=2),encoding='utf-8');os.replace(cache/'Build.tmp',record)
                say(f'COMPLETE: nodes={len(ns)} events={len(es)} samples={len(ss)}')
                return 2  # Successful generation; game must restart before activating this bank.
        except Exception as e:
            say('ERROR: '+str(e));raise

if __name__=='__main__':sys.exit(main())
