"""Decode every added stream independently and compare with a legacy PCM cache.
Private local diagnostic only: no audio is retained or included in packages.
"""
from pathlib import Path
import argparse,os,sys,struct,subprocess,tempfile,wave,json
import numpy as np
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'runtime'))
import native_bank as n
from build_cache import copy_stream

def main():
    ap=argparse.ArgumentParser();ap.add_argument('before',type=Path);ap.add_argument('after',type=Path)
    ap.add_argument('decoder',type=Path);ap.add_argument('dlls',type=Path);ap.add_argument('report',type=Path)
    a=ap.parse_args();old=(a.before/'EA_TRAX.mpf').read_bytes();new=(a.after/'EA_TRAX.mpf').read_bytes()
    assert n.nodes(old)==n.nodes(new),'Playback graph changed'
    assert n.events(old)==n.events(new),'Playback events changed'
    osamples,nsamples=n.samples(old),n.samples(new)
    assert len(osamples)==len(nsamples)
    assert [s[1] for s in osamples]==[s[1] for s in nsamples],'Durations changed'
    env=os.environ.copy();env['PATH']=str(a.dlls.resolve())+';'+env['PATH'];rows=[]
    with tempfile.TemporaryDirectory(prefix='codec-validation-',dir=a.report.parent) as td:
        td=Path(td)
        with (a.before/'EA_TRAX.mus').open('rb') as source,(a.after/'EA_TRAX.mus').open('rb') as encoded:
            for index in range(3257,len(nsamples)):
                raw=td/'reference.pcm';frames=n.extract_pcm_stream(source,osamples[index][0]*128,raw)
                assert frames is not None,'Reference must be legacy PCM'
                stream=td/'sample.asf'
                with stream.open('wb') as out:copy_stream(encoded,out,nsamples[index][0]*128)
                result=subprocess.run([str(a.decoder),'-o',str(td/'decoded.wav'),str(stream)],env=env,capture_output=True,timeout=60,creationflags=subprocess.CREATE_NO_WINDOW)
                assert result.returncode==0,result.stderr.decode(errors='replace')
                assert b'EA-XA 4-bit ADPCM v2' in result.stdout
                with wave.open(str(td/'decoded.wav')) as w:
                    assert (w.getnchannels(),w.getsampwidth(),w.getframerate(),w.getnframes())==(2,2,36000,frames)
                    samples=np.frombuffer(w.readframes(frames),dtype='<i2').astype(np.float64)
                reference=np.fromfile(raw,dtype='<i2').astype(np.float64)
                noise=np.mean((samples-reference)**2);power=np.mean(reference**2)
                snr=float(10*np.log10(power/noise)) if power and noise else None
                assert not power or np.any(samples),'Non-silent source became silent'
                if snr is not None:assert snr>20,('Poor signal/noise ratio',index+1,snr)
                rows.append(dict(sample=index+1,frames=frames,pcm_bytes=raw.stat().st_size,compressed_bytes=stream.stat().st_size,snr_db=snr))
                if len(rows)%40==0:print('Decoded',len(rows),'/',len(nsamples)-3257,flush=True)
    report=dict(graph_unchanged=True,events_unchanged=True,durations_unchanged=True,samples=len(nsamples),validated_added=len(rows),before_bytes=(a.before/'EA_TRAX.mus').stat().st_size,after_bytes=(a.after/'EA_TRAX.mus').stat().st_size,minimum_snr_db=min(r['snr_db'] for r in rows if r['snr_db'] is not None),rows=rows)
    a.report.write_text(json.dumps(report,indent=2));print({k:v for k,v in report.items() if k!='rows'})
if __name__=='__main__':main()
