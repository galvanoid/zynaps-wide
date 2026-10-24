param()
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$repo=Split-Path $project -Parent
$leaf=Split-Path $project -Leaf
$containerProject="/tmp/$leaf"
$saved=@($env:ST_WORKING_FOLDER,$env:STCMD_NO_TTY,$env:STCMD_IMAGE_TAG)
try {
    $env:ST_WORKING_FOLDER=$repo;$env:STCMD_NO_TTY='1';$env:STCMD_IMAGE_TAG='zynaps'
    # PowerShell 5 treats compiler warnings on stderr as NativeCommandError.
    # The compiler's exit code decides whether a build failed.
    $ErrorActionPreference='Continue'
    stcmd env ZY_MODE=play ZY_NEXT_FIXES=record,boss,ground,preshift,terrain,pacing ZY_VBL1=0 ZY_FULL_GPU=1 ZY_PROFILE=0 ZY_GPU_DISABLE=0 ZY_ASYNC_BLITTER=0 ZY_SPARSE_EXPERIMENTAL=0 ZY_DIAGNOSTIC=0 ZY_RECREATE_ROOT=$containerProject/build/app/recreate ZY_BUILD_ROOT=$containerProject/build/app ZY_DIST_ROOT=$containerProject/build/app/disk bash $containerProject/scripts/build_atari.sh
    $compileExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
    if($compileExit){throw 'Application build failed.'}
    $program=Join-Path $project 'build/app/disk/ZYNAPS.PRG'
    python "$PSScriptRoot/check-build-flavor.py" $program
    if($LASTEXITCODE){throw 'Harness rejected.'}
    $ErrorActionPreference='Continue'
    docker image inspect zynaps-test *> $null
    $imageExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
    if($imageExit){
        docker build -f "$PSScriptRoot/Dockerfile.test" -t zynaps-test $project
        if($LASTEXITCODE){throw 'Test toolchain build failed.'}
    }
    docker run --rm --entrypoint /bin/bash -v "${repo}:/host" zynaps-test "/host/$leaf/scripts/check-helpers.sh" "/host/$leaf/build/app/atari/ZYNAPS.elf"
    if($LASTEXITCODE){throw '68000 guard failed.'}
    New-Item -ItemType Directory -Force -Path (Join-Path $project 'dist') | Out-Null
    Copy-Item -LiteralPath $program -Destination (Join-Path $project 'dist/ZYWIDE.PRG')
    [ordered]@{mode='play';assets_embedded=$false;physical_status='unverified';sha256=(Get-FileHash $program).Hash.ToLowerInvariant()} | ConvertTo-Json | Set-Content (Join-Path $project 'build/app/BUILD.json') -Encoding UTF8
} finally {$env:ST_WORKING_FOLDER,$env:STCMD_NO_TTY,$env:STCMD_IMAGE_TAG=$saved}
