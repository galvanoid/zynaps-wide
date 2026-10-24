param()
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$repo=Split-Path $project -Parent
$leaf=Split-Path $project -Leaf
$saved=@($env:ST_WORKING_FOLDER,$env:STCMD_NO_TTY,$env:STCMD_IMAGE_TAG)
try {
    if(-not (Test-Path "$project/dist/ZYWIDE.PRG")){throw 'Build the asset-free Atari program first.'}
    $env:ST_WORKING_FOLDER=$repo;$env:STCMD_NO_TTY='1';$env:STCMD_IMAGE_TAG='zynaps'
    $ErrorActionPreference='Continue'
    stcmd bash "/tmp/$leaf/scripts/build_target.sh"
    $targetExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
    if($targetExit){throw 'Cartridge target build failed.'}
    $ErrorActionPreference='Continue'
    docker image inspect stnext-sidecart-build *> $null
    $imageExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
    if($imageExit){
        docker build -f "$PSScriptRoot/Dockerfile.firmware" -t stnext-sidecart-build $project
        if($LASTEXITCODE){throw 'Firmware toolchain image failed.'}
    }
    $dependencyPath="$project/build/deps/md-devops/pico-sdk/pico_sdk_init.cmake"
    $mounts=@('-v',"${repo}:/host",'-v','zynaps-app-sc-native:/work')
    $defines=@('-e','ZY_BUILD_ROOT=/work','-e','ZY_FULL_GPU=1')
    if(Test-Path $dependencyPath){
        $defines+=@('-e',"ZY_DEPENDENCIES=/host/$leaf/build/deps/md-devops")
    } else {
        # Existing workspace dependency cache is read-only and never edited.
        $mounts+=@('-v','stnext-sc-native:/dependencies:ro')
        $defines+=@('-e','ZY_DEPENDENCIES=/dependencies/firmware/stnext-sc')
    }
    $ErrorActionPreference='Continue'
    docker run --rm --entrypoint /bin/bash @mounts @defines stnext-sidecart-build "/host/$leaf/scripts/build_firmware.sh"
    $compileExit=$LASTEXITCODE;$ErrorActionPreference='Stop'
    if($compileExit){throw 'Firmware build failed.'}
    python "$PSScriptRoot/check-firmware.py" "$project/dist/ZYNAPS-SC.uf2"
    if($LASTEXITCODE){throw 'UF2 overlaps reserved/private flash.'}
} finally {$env:ST_WORKING_FOLDER,$env:STCMD_NO_TTY,$env:STCMD_IMAGE_TAG=$saved}
