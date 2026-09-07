param([switch]$SkipRuntime)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot
$root=Split-Path $project
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -property installationPath
$msbuild=Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'
foreach($target in @('NFSMWEATraxExpansion.vcxproj','NFSMWEATraxProbe.vcxproj','StartupTests.vcxproj')) {
 & $msbuild (Join-Path $project $target) /m:2 /p:Configuration=Release /p:Platform=Win32 /v:minimal
 if($LASTEXITCODE){throw "Build failed: $target"}
}
$csc=Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$converter=Join-Path $project 'converter'
& $csc /nologo /target:winexe /platform:anycpu /optimize+ "/out:$converter\TheRunPursuitConverter.exe" /reference:System.Windows.Forms.dll /reference:System.Drawing.dll /reference:System.Web.Extensions.dll "/resource:$converter\recipe.json,recipe.json" "/resource:$converter\graph.mpf,graph.mpf" "/resource:$converter\profile.ini,profile.ini" "$converter\TheRunConverter.cs"
if($LASTEXITCODE){throw 'Converter build failed'}
if(-not $SkipRuntime) {
 & python -m PyInstaller --noconfirm --clean --distpath (Join-Path $root 'staging\eatrax-046-runtime') --workpath (Join-Path $root 'staging\eatrax-046-build') (Join-Path $project 'runtime\BuildCache.spec')
 if($LASTEXITCODE){throw 'Cache runtime build failed'}
}
