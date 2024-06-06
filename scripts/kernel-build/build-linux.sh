#!/bin/bash -e

# Set up the required environment variables
pushd ../
    source .env
popd

INSTALL_CVM=""

if [[ $1 == "native" ]]; then
    LINUXFOLDER=$LINUXFOLDER_NATIVE
elif [[ $1 == "tdx" ]]; then
    INSTALL_CVM="tdx"
fi

log_info "Build CVM=$INSTALL_CVM"

# Check that the folder exists
if [ ! -f "../.env" ] || [ ! -d $LINUXFOLDER ]; then
    echo "Linux folder: $LINUXFOLDER"
    echo "Please get a kernel source first using ./obtain-linux.sh"
    exit 0
fi

CURDIR=$(pwd)
CURBRANCH=`git status | head -1 | cut -d ' ' -f3`

# Build the kernel executable
pushd $LINUXFOLDER
    # copy the saved config
    cp $CURDIR/.config.saved.nokvm-perf-nolivepatch-nospec-noloadmod-nohp-no5level-tdx .config
    make -j`nproc`
popd

# Export required environment variables
export VMDISKMOUNT
export VMDISK
export VMDISKFOLDER
export LINUXFOLDER
export LINUXVERSION

if [[ $INSTALL_CVM == "tdx" ]]; then
    export VMDISKFOLDER_TDX
    export VMDISKMOUNT_TDX
    export VMDISK_TDX
fi

# Install the kernel modules and image
pushd ../common
    {
        ./install-modules.sh $INSTALL_CVM
        ./install-image.sh $INSTALL_CVM
    } |& tee -a $CURDIR/build.kern.log
popd

./copy-source.sh $1 |& tee -a $CURDIR/build.kern.log
