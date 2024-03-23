#!/bin/bash

# Set up the required environment variables
pushd ../
    source .env
popd

COMM_SCRIPT_PATH=$(realpath $(pwd)/../common)
pushd $COMM_SCRIPT_PATH
    ./load-vmdisk.sh
popd

