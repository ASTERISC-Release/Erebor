#!/bin/bash

# Set up the required environment variables
pushd ../
    source .env
popd

# # argument check
reconfigure=""
native=""
if [ "$1" == "reconfigure" ]; then
        reconfigure="1"
fi
if [ "$1" == "native" ]; then
        native="1"
        GRAMINEDIR="/home/pks/gramine-native-dir/gramine"
        echo "Native folder=$GRAMINEDIR"
fi

# Check that the folder exists
if [ ! -f "../.env" ] || [ ! -d $GRAMINEDIR ]; then
    echo "Gramine folder: $GRAMINEDIR does not exist."
    echo "Please get a gramine source first using ./obtain-gramine.sh"
    exit 0
fi

CURDIR=$(pwd)
# Build the kernel executable
pushd $GRAMINEDIR
    # configure
    if [ ! -d ./build ]; then
        if [ -n "$native" ]; then
        meson setup build/ \
                --buildtype=release \
                -Ddirect=enabled -Dsgx=disabled
        else
        meson setup build/ \
            --buildtype=debug \
            -Dencos=enabled \
            -DENCOS_DEBUG=enabled \
            -Ddirect=enabled -Dsgx=disabled
        fi
    fi
    # reconfigure
    if [ -n "$reconfigure" ]; then
        meson configure build/ \
            --buildtype=debug \
            -Dencos=enabled \
            -DENCOS_DEBUG=enabled \
            -Ddirect=disabled -Dsgx=disabled
    fi
    # native
    # native
    if [ -n "$native" ]; then
        if [ ! -d build/  ]; then
        meson setup build/ \
          --buildtype=release \
          -Ddirect=enabled -Dsgx=disabled
        else
        meson configure build/ \
           --buildtype=release \
           -Ddirect=enabled -Dsgx=disabled
        fi
    fi

    # build
    ninja -C build/
    # install
    sudo ninja -C build/ install
popd