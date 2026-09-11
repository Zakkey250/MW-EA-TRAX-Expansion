"""Pathfinder v5.1 serialization and EA PCM streaming. No game assets embedded."""
import struct, wave

def compressed_stream(wav,destination,probe,run):
    raw=destination.with_suffix('.pcm')
    try:
        with wave.open(str(wav)) as w,raw.open('wb') as f:
            if w.getparams()[:3]!=(2,2,36000):raise ValueError('Expected stereo PCM16 36kHz')
            frames=w.getnframes()
            while data:=w.readframes(65536):f.write(data)
        run([probe,'--encode-eaxa',raw,0,frames,destination])
        return round(frames*1000/36000)
    finally:raw.unlink(missing_ok=True)

def extract_pcm_stream(source,offset,destination):
    """Return frame count for our legacy PCM stream, or None for compressed input."""
    source.seek(offset);head=source.read(8)
    if len(head)!=8 or head[:4]!=b'SCHl':raise ValueError('Invalid pursuit SCHl header')
    size=u32(head,4)
    if not 12<=size<=4096:raise ValueError('Invalid pursuit header size')
    h=source.read(size-8)
    if h[:4]!=b'PT\x00\x00':raise ValueError('Unsupported pursuit platform')
    patches={};i=4
    while i<len(h):
        tag=h[i];i+=1
        if tag==255:break
        if tag in (252,253):continue
        if i>=len(h):raise ValueError('Truncated pursuit patch')
        length=h[i];i+=1
        if i+length>len(h):raise ValueError('Truncated pursuit patch value')
        patches[tag]=int.from_bytes(h[i:i+length],'big');i+=length
    if patches.get(160,0)!=8:return None
    if patches.get(130)!=2 or patches.get(132)!=36000:raise ValueError('Unsupported legacy PCM format')
    total=0
    with destination.open('wb') as out:
        while True:
            head=source.read(8)
            if len(head)!=8:raise ValueError('Truncated pursuit stream')
            tag,size=struct.unpack('<4sI',head)
            if not 8<=size<=1048576:raise ValueError('Invalid pursuit block size')
            b=source.read(size-8)
            if len(b)!=size-8:raise ValueError('Truncated pursuit block')
            if tag==b'SCEl':break
            if tag!=b'SCDl':continue
            count,left,right=struct.unpack_from('<III',b)
            if 12+max(left,right)+count*2>len(b):raise ValueError('Invalid PCM offsets')
            data=bytearray(count*4)
            for ch,start in enumerate((left,right)):
                planar=b[12+start:12+start+count*2]
                data[ch*2::4]=planar[0::2];data[ch*2+1::4]=planar[1::2]
            out.write(data);total+=count
    if total!=patches.get(133):raise ValueError('PCM frame count mismatch')
    return total

def u16(b,o): return struct.unpack_from('<H',b,o)[0]
def u32(b,o): return struct.unpack_from('<I',b,o)[0]
def p32(b,o,v): struct.pack_into('<I',b,o,v)
FIELDS=dict(nodes=20,nodedata=24,events=28,eventdata=32,vars=36,routers=40,tracks=44,trackdata=48,samples=52,eof=56)
def layout(b):
    if b[:6]!=b'xDFP\x05\x01': raise ValueError('Unsupported Pathfinder bank')
    l={k:u32(b,o) for k,o in FIELDS.items()}
    if l['eof']!=len(b):raise ValueError('Truncated bank')
    return l
def nodes(b):
    l=layout(b);offs=[u16(b,l['nodes']+i*2)*4 for i in range(u16(b,18))]+[l['events']]
    return [bytearray(b[s:e]) for s,e in zip(offs,offs[1:])]
def events(b):
    l=layout(b);out=[]
    for i in range(b[15]):
        off=u16(b,l['events']+i*2)*4;out.append(bytearray(b[off:off+20+b[off+15]*12]))
    return out
def samples(b):
    l=layout(b);return [struct.unpack_from('<II',b,o) for o in range(l['samples'],l['eof'],8)]
def serialize(base,ns,es,ss,minimal=False):
    if len(es)>255 or len(ns)>32767 or len(ss)>32767:raise ValueError('Native bank capacity exceeded')
    old=layout(base);out=bytearray(base[:72]);loc={}
    def table(key,count):
        loc[key]=len(out);out.extend(bytes((count*2+3)&~3))
    def records(key,tablekey,records):
        loc[key]=len(out)
        for i,r in enumerate(records):
            if len(out)//4>65535 or len(r)%4:raise ValueError('Native bank record limit')
            struct.pack_into('<H',out,loc[tablekey]+i*2,len(out)//4);out.extend(r)
    table('nodes',len(ns));records('nodedata','nodes',ns)
    table('events',len(es));records('eventdata','events',es)
    loc['vars']=len(out)
    if minimal:
        out[12]=0;out[13]=1;out[14]=1;out[16]=0;out[17]=0
        loc['routers']=len(out);out.extend(bytes(4))
        loc['tracks']=len(out);out.extend(bytes(4))
        loc['trackdata']=len(out);out.extend(bytes(20))
        p32(out,loc['tracks'],loc['trackdata']//4)
        p32(out,loc['routers'],loc['tracks']//4)
    else:
        delta=loc['vars']-old['vars'];out.extend(base[old['vars']:old['samples']])
        for k in ['routers','tracks','trackdata']:loc[k]=old[k]+delta
        for k,count in [('routers',base[16]+1),('tracks',base[13])]:
            for i in range(count):o=loc[k]+4*i;p32(out,o,u32(out,o)+delta//4)
    loc['samples']=len(out)
    for offset,duration in ss:out.extend(struct.pack('<II',offset,duration))
    loc['eof']=len(out)
    for k,o in FIELDS.items():p32(out,o,loc[k])
    struct.pack_into('<H',out,18,len(ns));out[15]=len(es)
    return out
def relocate_node(record,node_map,sample_map):
    r=bytearray(record);sample=struct.unpack_from('<h',r)[0]
    if sample>0:struct.pack_into('<H',r,0,sample_map[sample])
    struct.pack_into('<H',r,8,node_map[u16(r,8)])
    for i in range((u32(r,4)>>12)&31):
        off=18+4*i;struct.pack_into('<H',r,off,node_map[u16(r,off)])
    return r
def relocate_event(record,node_map):
    r=bytearray(record)
    for off in range(20,len(r),12):
        if (u32(r,off+4)>>8)&255==4:
            dest=struct.unpack_from('<h',r,off+8)[0]
            if dest>=0:struct.pack_into('<H',r,off+8,node_map[dest])
    return r
def pcm_stream(wav,destination):
    """EA SCHl v3 PCM16LE, two planar channels per SCDl block."""
    with wave.open(str(wav)) as w, open(destination,'wb') as f:
        if w.getparams()[:3]!=(2,2,36000):raise ValueError('Expected stereo PCM16 36kHz')
        frames=w.getnframes()
        def patch(tag,value):
            raw=value.to_bytes(max(1,(value.bit_length()+7)//8),'big');return bytes([tag,len(raw)])+raw
        def block(tag,data):f.write(tag+struct.pack('<I',8+len(data))+data)
        header=b'PT\x00\x00'+patch(0x80,3)+patch(0x81,16)+patch(0x82,2)+patch(0x84,36000)+patch(0x85,frames)+patch(0xA0,8)+b'\xff'
        header+=bytes((-len(header))%4);block(b'SCHl',header)
        chunk=4096;block(b'SCCl',struct.pack('<I',(frames+chunk-1)//chunk))
        while True:
            data=w.readframes(chunk)
            if not data:break
            count=len(data)//4
            # Byte slicing avoids endian-dependent native array interpretation.
            left=bytearray(count*2);right=bytearray(count*2)
            left[0::2]=data[0::4];left[1::2]=data[1::4]
            right[0::2]=data[2::4];right[1::2]=data[3::4]
            block(b'SCDl',struct.pack('<III',count,0,count*2)+left+right)
        block(b'SCEl',b'')
    return round(frames*1000/36000)

