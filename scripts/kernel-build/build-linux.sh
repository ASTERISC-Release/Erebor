#!/bin/bash -e

# Set up the required environment variables
pushd ../
    source .env
popd


INSTALL_CVM=""
native=0

# Function to display help message
usage() {
  echo "Usage: $0 [-n] [-c CVM]"
  exit 1
}

PARAMS="$@"

# Parse command line arguments
while getopts ":nc:" opt; do
  case $opt in
    n)
      native=1
      ;;
    c)
      INSTALL_CVM="$OPTARG"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      usage
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      usage
      ;;
  esac
done
# Shift off the options and optional --.
shift $((OPTIND - 1))

log_info "Build CVM=$INSTALL_CVM"

# Check that the folder exists
if [ $native -eq 1 ]; then
    LINUXFOLDER=$LINUXFOLDER_NATIVE
fi

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
        ./install-modules.sh "$PARAMS"
        ./install-image.sh "$PARAMS"
    } |& tee -a $CURDIR/build.kern.log
popd

# Execute this script only when we need to build kernel module
# inside the guest.
#./copy-source.sh $PARAMS |& tee -a $CURDIR/build.kern.log
