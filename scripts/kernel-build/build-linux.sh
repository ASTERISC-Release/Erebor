#!/bin/bash -e

# Set up the required environment variables
pushd ../
    source .env
popd

if [[ $1 == "native" ]]; then
    LINUXFOLDER=$LINUXFOLDER_NATIVE
fi

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
    cp $CURDIR/.config.saved .config

    {
        cat .config | grep HUGE
        cat .config | grep CONFIG_PREEMPT
        cat .config | grep CONFIG_PREEMPT_RCU || true

        # Start the build process
        make -j`nproc` 
    } |& tee $CURDIR/build.kern.log
popd

# Export required environment variables
export VMDISKMOUNT
export VMDISK
export VMDISKFOLDER
export LINUXFOLDER
export LINUXVERSION

# Install the kernel modules and image
pushd ../common
    {
        ./install-modules.sh
        ./install-image.sh
    } |& tee -a $CURDIR/build.kern.log
popd

# ./copy-source.sh |& tee -a $CURDIR/build.kern.log
