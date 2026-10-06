"""Execute unmodified MusicFlow scheduling bytes in an isolated x86 emulator.
Only the proposed entry gate is substituted; never attach to the game.
"""
from pathlib import Path
import sys, struct, re, hashlib
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_EAX
if len(sys.argv)!=2:raise SystemExit('Usage: TestNativeMusicFlow.py <owned speed.exe>')
game=Path(sys.argv[1]);raw=game.read_bytes()
assert hashlib.sha256(raw).hexdigest().upper() in {
 'B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06',
 '80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C',
 '0C5675A08CD71FD6D31CA87E992A915054BD8B80D268BFF0561D7ECC2067E342'}
pe=pefile.PE(data=raw);base=pe.OPTIONAL_HEADER.ImageBase
source=(Path(__file__).parents[1]/'src/HeatKeeping.inl').read_text(encoding='utf-8')
for address, values in re.findall(r'BytesEqual\((0x[0-9A-F]+),\s*\{([^}]+)\}',source):
    expected=bytes(int(v.strip(),16) for v in values.split(','))
    actual=pe.get_data(int(address,16)-base,len(expected))
    assert actual==expected, (address,actual.hex(),expected.hex())
print('PASS all new native hook guards against exact 4GB EXE')

def run(holding, channel_state, abort_start=False):
    u=Uc(UC_ARCH_X86,UC_MODE_32)
    u.mem_map(base,0x700000); u.mem_write(base,pe.get_memory_mapped_image())
    u.mem_map(0x1000000,0x20000)
    controller,snd,options,ai,race,channel=range(0x1000000,0x1006000,0x1000)
    def put(a,v):u.mem_write(a,struct.pack('<I',v))
    put(controller,0x899b80);put(controller+0x114,4)
    put(controller+0x128,0x01E0000E);put(controller+0x12c,0x01E0000E);put(controller+0x150,1);put(controller+0x148,0);put(controller+0x60,5)
    put(0x911fa8,snd);put(snd+0x70,6);put(snd+0x24,options)
    put(options,0x3f800000);put(options+0xc,0x3f800000)
    put(options+0x28,1);put(options+0x2c,1)
    put(0x993cc8,ai);put(ai+0x1d4,0)
    put(0x91e000,race);put(race+0x1968,0)
    put(0x8f2080+24,1);put(0x9121d8,channel);put(channel+0x38,channel_state)
    put(0x8f86fc,1)
    sentinel=0x101f000; sp=0x101e000
    put(sp,sentinel);put(sp+4,0x3c888889)
    u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_ECX,controller)
    seen=[]
    def hook(uc,a,size,data):
        if a==0x4e7500 and holding:
            seen.append('hold gate')
            stk=uc.reg_read(UC_X86_REG_ESP)
            ret=struct.unpack('<I',uc.mem_read(stk,4))[0]
            uc.reg_write(UC_X86_REG_EAX,0)
            uc.reg_write(UC_X86_REG_ESP,stk+4);uc.reg_write(UC_X86_REG_EIP,ret)
        elif a==0x4df1c0 and abort_start:
            seen.append('late start gate')
            stk=uc.reg_read(UC_X86_REG_ESP)
            ret=struct.unpack('<I',uc.mem_read(stk,4))[0]
            uc.reg_write(UC_X86_REG_ESP,stk+8);uc.reg_write(UC_X86_REG_EIP,ret)
        elif a in [0x4df1c0,0x4f6c80,0x4df110]:
            seen.append({0x4df1c0:'pursuit',0x4f6c80:'next EA TRAX',0x4df110:'ambience'}[a])
            uc.emu_stop()
        elif a==0x4df6d0:
            # Native volume-fader tick does not decide the music category.
            stk=uc.reg_read(UC_X86_REG_ESP)
            ret=struct.unpack('<I',uc.mem_read(stk,4))[0]
            uc.reg_write(UC_X86_REG_ESP,stk+8);uc.reg_write(UC_X86_REG_EIP,ret)
        elif a==sentinel:uc.emu_stop()
    u.hook_add(UC_HOOK_CODE,hook)
    u.emu_start(0x4e7770,sentinel+1,count=20000)
    print('native MusicFlow:',holding,channel_state,seen)
    event=struct.unpack('<I',u.mem_read(controller+0x128,4))[0]
    return seen,event


# Reproduce terminal-status mismatch: update resets holding, TryStart clears
# event, then the old startBoundary=True hook reacquires holding and returns.
assert run(False,3,True)==(['late start gate'],0)
assert run(False,1,True)==(['late start gate'],0)
print('REPRODUCED old bug: current event erased before late rejection; EOF branch skipped')
# Fixed tail holds the upstream gate until native music AI also leaves pursuit.
assert run(True,3)==(['hold gate'],0x01E0000E)
assert run(True,1)==(['hold gate','next EA TRAX'],0x01E0000E)
assert run(False,3)==(['pursuit'],0)
print('PASS fixed gate: event retained and next-track dispatch reached; HEAT 4 unchanged')
