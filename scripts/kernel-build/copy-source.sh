#!/bin/bash -e

# Set up the required environment variables
pushd ../
    source .env
popd

# Check that the folder exists
if [ ! -f "../.env" ] || [ ! -d $LINUXFOLDER ]; then
    echo "Please get a kernel source first using ./obtain-linux.sh"
    exit 0
fi

# Export required environment variables
export VMDISKMOUNT
export VMDISK
export VMDISKFOLDER
export LINUXFOLDER
export LINUXVERSION

# Install the kernel modules and image
pushd ../common
    ./copy-build-source.sh
popd