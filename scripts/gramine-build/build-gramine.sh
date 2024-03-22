#!/bin/bash

# Set up the required environment variables
pushd ../
    source .env
popd

# # argument check
reconfigure=""
if [ "$1" == "reconfigure" ]; then
	reconfigure="1"
fi
# elif [ "$1" == "release" ]; then
#     TARG="release"
# else
#     echo "Usage: $0 [debug | release]"
#     exit 1
# fi

# Check that the folder exists
if [ ! -f "../.env" ] || [ ! -d $GRAMINEDIR ]; then
    echo "Gramine folder: $LINUXFOLDER does not exist."
    echo "Please get a gramine source first using ./obtain-gramine.sh"
    exit 0
fi

CURDIR=$(pwd)
# Build the kernel executable
pushd $GRAMINEDIR
    # configure
    if [ ! -d ./build ]; then
        meson setup build/ \
            --buildtype=debug \
            -Dencos=enabled \
            -DENCOS_DEBUG=enabled \
            -Ddirect=disabled -Dsgx=disabled
    fi
    # reconfigure
    if [ -n "$reconfigure" ]; then
        meson configure build/ \
            --buildtype=debug \
            -Dencos=enabled \
            -DENCOS_DEBUG=enabled \
            -Ddirect=disabled -Dsgx=disabled
    fi
    # build
    ninja -C build/
    # install
    sudo ninja -C build/ install
popd
