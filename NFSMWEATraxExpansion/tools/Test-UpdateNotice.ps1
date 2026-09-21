param([switch]$Live,[switch]$Dialog)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install=& $vswhere -latest -products '*' -requires Microsoft.Component.MSBuild -property installationPath
if(!$install){throw 'Build tools not found'}
$devcmd=Join-Path $install 'Common7\Tools\VsDevCmd.bat'
Push-Location $project
try {
    New-Item -ItemType Directory -Path artifacts/Tests -Force | Out-Null
    $cmd='"'+$devcmd+'" -no_logo -arch=x86 -host_arch=x64 && cl /nologo /std:c++17 /utf-8 /EHsc /O2 /MD /W4 /WX /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Iinclude tools\UpdateNoticeTests.cpp /Foartifacts\Tests\UpdateNoticeTests.obj /Feartifacts\Tests\UpdateNoticeTests.exe /link /SUBSYSTEM:CONSOLE /MANIFEST:EMBED /MANIFESTUAC:"level=''asInvoker'' uiAccess=''false''"'
    & cmd /d /c $cmd
    if($LASTEXITCODE){throw 'Update notice test build failed'}
    & artifacts/Tests/UpdateNoticeTests.exe
    if($LASTEXITCODE){throw 'Update notice offline tests failed'}
    if($Live){& artifacts/Tests/UpdateNoticeTests.exe --live;if($LASTEXITCODE){throw 'Live metadata test failed'}}
    if($Dialog){& artifacts/Tests/UpdateNoticeTests.exe --dialog;if($LASTEXITCODE){throw 'Native dialog tests failed'}}
}finally{Pop-Location}
