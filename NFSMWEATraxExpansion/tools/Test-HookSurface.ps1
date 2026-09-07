[CmdletBinding()]
param(
    [string]$Executable = 'C:\Program Files (x86)\EA GAMES\Need for Speed Most Wanted\speed.exe'
)

$ErrorActionPreference = 'Stop'

$expectedSize = 6033408
$expectedHash = '05873CF968E0BDD021C1E67FF22E9350D22E7F433F1D749323FA6AE27F504700'
$stockIconHash = '6A1E41A449751241DE3653BE6BE9750A0B087E210011375C514872789CDE0BA8'
$resolvedExecutable = (Resolve-Path -LiteralPath $Executable).Path
$item = Get-Item -LiteralPath $resolvedExecutable
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedExecutable).Hash
$nfspatcher = $item.Length -eq 6029312 -and $hash -in @('80774C2E5D619B4F120B48D4462896FD504C263399D203A238769CFFDE1D253C','B248271BF8EAC8C9B283B8C95E3ADD672B713BF529B05F1780E58268493B9D06')
if (-not $nfspatcher -and ($item.Length -ne $expectedSize -or $hash -notin @($expectedHash, $stockIconHash))) {
    throw "Unsupported executable identity: size=$($item.Length) sha256=$hash"
}

$bytes = [IO.File]::ReadAllBytes($resolvedExecutable)
$peOffset = [BitConverter]::ToInt32($bytes, 0x3C)
if ([Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
    throw 'Not a PE image'
}
$coff = $peOffset + 4
$sectionCount = [BitConverter]::ToUInt16($bytes, $coff + 2)
$optionalSize = [BitConverter]::ToUInt16($bytes, $coff + 16)
$optional = $coff + 20
$imageBase = [BitConverter]::ToUInt32($bytes, $optional + 28)
$sectionTable = $optional + $optionalSize
$sections = @()
for ($index = 0; $index -lt $sectionCount; $index++) {
    $offset = $sectionTable + ($index * 40)
    $sections += [pscustomobject]@{
        VirtualSize = [BitConverter]::ToUInt32($bytes, $offset + 8)
        Rva = [BitConverter]::ToUInt32($bytes, $offset + 12)
        RawSize = [BitConverter]::ToUInt32($bytes, $offset + 16)
        RawOffset = [BitConverter]::ToUInt32($bytes, $offset + 20)
    }
}

function Get-OffsetForVa {
    param([UInt32]$Address)
    $rva = [UInt32]($Address - $imageBase)
    foreach ($section in $sections) {
        $extent = [Math]::Max($section.VirtualSize, $section.RawSize)
        if ($rva -ge $section.Rva -and $rva -lt ($section.Rva + $extent)) {
            $delta = $rva - $section.Rva
            if ($delta -ge $section.RawSize) { throw ('VA 0x{0:X8} is not file-backed' -f $Address) }
            return [int]($section.RawOffset + $delta)
        }
    }
    throw ('VA 0x{0:X8} is not mapped' -f $Address)
}

function Assert-Bytes {
    param([UInt32]$Address, [byte[]]$Expected, [string]$Name)
    $offset = Get-OffsetForVa -Address $Address
    for ($index = 0; $index -lt $Expected.Length; $index++) {
        if ($bytes[$offset + $index] -ne $Expected[$index]) {
            throw ('{0} guard failed at VA 0x{1:X8}+0x{2:X}' -f $Name, $Address, $index)
        }
    }
    Write-Output ('PASS bytes {0} VA=0x{1:X8} length={2}' -f $Name, $Address, $Expected.Length)
}

$guards = @(
    @{ A = 0x00688230; B = [byte[]](0x8B,0x41,0x54,0xC3); N = 'PressureAIInterface' },
    @{ A = 0x004AE340; B = [byte[]](0x51,0x56,0x8D,0x44,0x24,0x04,0x50,0x8B,0xF1); N = 'NativeAIPartMessage' },
    @{ A = 0x0065FAF0; B = [byte[]](0x56,0x57,0x8B,0xF9,0x51,0x8B,0x0F); N = 'NativeMessageDispatch' },
    @{ A = 0x005CC240; B = [byte[]](0x8B,0x44,0x24,0x04); N = 'NativeRecipientHash' },
    @{ A = 0x00409400; B = [byte[]](0xD9,0x41,0x1C,0xC3); N = 'PressureHeat' },
    @{ A = 0x00433C90; B = [byte[]](0x8B,0x41,0x20,0xC3); N = 'PressureCopCount' },
    @{ A = 0x00433B60; B = [byte[]](0x8B,0x81,0x18,0x02,0x00,0x00,0xC3); N = 'PressureStatus' },
    @{ A = 0x006881A0; B = [byte[]](0xD9,0x41,0x78,0xC3); N = 'PressureSpeed' },
    @{ A = 0x006880B0; B = [byte[]](0x8B,0x81,0x94,0x00,0x00,0x00,0xC3); N = 'PressureDriver' },
    @{ A = 0x005D59F0; B = [byte[]](0x83,0xEC,0x08,0x56,0x8B,0x71,0x08); N = 'PressureInterfaceLookup' },
    @{ A = 0x004DFA34; B = [byte[]](0xC7,0x41,0x38,0xA4,0x88,0x89,0x00,0xC7,0x41,0x3C,0x94,0x88,0x89,0x00); N = 'SeparateBankNamesGate' },
    @{ A = 0x004B6310; B = [byte[]](0xA1,0xE8,0x21,0x91,0x00); N = 'AdaptivePartLookup' },
    @{ A = 0x004DF2D0; B = [byte[]](0x56,0x8B,0xF1,0x68,0x00,0x00,0x00,0x0F); N = 'StopMusic' },
    @{ A = 0x004DFB80; B = [byte[]](0xA1,0xFC,0x86,0x8F,0x00,0x85,0xC0); N = 'AdaptiveMusicControl' },
    @{ A = 0x0071B1AE; B = [byte[]](0x8B,0x8F,0x30,0x01,0x00,0x00,0x85,0xC9); N = 'NativePursuitStatusSource' },
    @{ A = 0x0081D5F2; B = [byte[]](0x55,0x8B,0xEC,0x51,0x80,0x3D,0x88,0x21,0x9C,0x00,0x00); N = 'NativeGain' },
    @{ A = 0x0081BCCB; B = [byte[]](0xFF,0x15,0xE8,0xBC,0x90,0x00); N = 'AudioLock' },
    @{ A = 0x0081BCD9; B = [byte[]](0x80,0x2D,0x89,0x21,0x9C,0x00,0x01); N = 'AudioUnlock' },
    @{ A = 0x005AB450; B = [byte[]](0x6A,0xFF,0x68,0x77,0x3D,0x87,0x00); N = 'JukeboxInit' },
    @{ A = 0x004DF330; B = [byte[]](0xA1,0x90,0xCF,0x91,0x00,0x8B,0x48,0x10); N = 'RefreshJukebox' },
    @{ A = 0x004F4CD0; B = [byte[]](0x53,0x56,0x8B,0xF1,0x8B,0x86,0x50,0x01,0x00,0x00); N = 'SelectJukeboxTrack' },
    @{ A = 0x004F6C80; B = [byte[]](0x55,0x56,0x8B,0xF1,0x8B,0x86,0x14,0x01,0x00,0x00); N = 'EATraxEventHandler' },
    @{ A = 0x0082BFD0; B = [byte[]](0x55,0x8B,0xEC,0xA1,0x64,0x29,0x9C,0x00); N = 'PATHI_getevent' },
    @{ A = 0x0082B4A0; B = [byte[]](0x55,0x8B,0xEC,0x56,0x8B,0xF1); N = 'Play' },
    @{ A = 0x0082B5A0; B = [byte[]](0x56,0x57,0x8B,0xF1); N = 'Stop' },
    @{ A = 0x0082A580; B = [byte[]](0x55,0x8B,0xEC,0x56,0x8B,0xF1); N = 'Pause' },
    @{ A = 0x0082B460; B = [byte[]](0x55,0x8B,0xEC,0x8B,0x45,0x08); N = 'Timer' },
    @{ A = 0x0082B290; B = [byte[]](0x55,0x8B,0xEC,0x53,0x8B,0x5D,0x08); N = 'Volume' },
    @{ A = 0x004B24F0; B = [byte[]](0x56,0x8B,0xF1,0xE8,0xD3,0x97,0x36,0x00); N = 'PauseChannel' },
    @{ A = 0x004B2510; B = [byte[]](0x56,0x8B,0xF1,0xE8,0xB3,0x97,0x36,0x00); N = 'ResumeChannel' },
    @{ A = 0x004DF219; B = [byte[]](0xE8,0x12,0xD5,0x34,0x00); N = 'PursuitTransition' },
    @{ A = 0x004DF12E; B = [byte[]](0xE8,0xFD,0xD5,0x34,0x00); N = 'AmbienceTransition' },
    @{ A = 0x004DF373; B = [byte[]](0x8B,0x50,0x10,0x81,0xC2,0x24,0x03,0x00,0x00); N = 'SaveCave1' },
    @{ A = 0x0051E000; B = [byte[]](0xA1,0x90,0xCF,0x91,0x00,0x8B,0x40,0x10); N = 'SaveCave2' },
    @{ A = 0x005509D3; B = [byte[]](0xA1,0x90,0xCF,0x91,0x00,0x8B,0x70,0x10); N = 'SaveCave3' },
    @{ A = 0x004F4D32; B = [byte[]](0x8B,0x0D,0x90,0xCF,0x91,0x00,0x8B,0x51,0x10); N = 'SaveCave4' },
    @{ A = 0x0051E07C; B = [byte[]](0x0F,0xB6,0x46,0x04,0x83,0xF8,0x03); N = 'SaveWriter' }
)
foreach ($guard in $guards) {
    Assert-Bytes -Address $guard.A -Expected $guard.B -Name $guard.N
}

$vtableGuards = @(
    @{ Slot = 0x008C5F3C; Expected = 0x0082B5A0; Name = 'StopVtable' },
    @{ Slot = 0x008C5F40; Expected = 0x0082A580; Name = 'PauseVtable' },
    @{ Slot = 0x008C5F50; Expected = 0x0082B460; Name = 'TimerVtable' },
    @{ Slot = 0x008C5F58; Expected = 0x0082B4A0; Name = 'PlayVtable' },
    @{ Slot = 0x008C5F60; Expected = 0x0082B290; Name = 'VolumeVtable' }
)
foreach ($guard in $vtableGuards) {
    $offset = Get-OffsetForVa -Address $guard.Slot
    $actual = [BitConverter]::ToUInt32($bytes, $offset)
    if ($actual -ne $guard.Expected) {
        throw ('{0} failed: actual=0x{1:X8} expected=0x{2:X8}' -f $guard.Name, $actual, $guard.Expected)
    }
    Write-Output ('PASS vtable {0} slot=0x{1:X8} target=0x{2:X8}' -f $guard.Name, $guard.Slot, $actual)
}

Write-Output ('PASS executable size={0} sha256={1}' -f $item.Length, $hash)
Write-Output ('PASS hook_surface guards={0} vtables={1}' -f $guards.Count, $vtableGuards.Count)
