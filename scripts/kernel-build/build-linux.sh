#!/bin/bash -e

# Set up the required environment variables
pushd ../
    source .env
popd

# Check that the folder exists
if [ ! -f "../.env" ] || [ ! -d $LINUXFOLDER ]; then
    echo "Linux folder: $LINUXFOLDER"
    echo "Please get a kernel source first using ./obtain-linux.sh"
    exit 0
fi

# Build the kernel executable
pushd $LINUXFOLDER
    # Make the default configuration
    make defconfig

    # Update the configuration for KVM/QEMU guests
    # NOTE: In some older kernels, it was called "make kvmconfig"
    make kvm_guest.config

    # Enable full tickless kernel
    sed -i "s/# CONFIG_NO_HZ_FULL is not set/CONFIG_NO_HZ_IDLE=y/g" .config
    
    # Premption settings for hv-instrument
    sed -i "s/CONFIG_PREEMPT_VOLUNTARY=y/# CONFIG_PREEMPT_VOLUNTARY is not set/g" .config
    sed -i "s/# CONFIG_PREEMPT_NONE is not set/CONFIG_PREEMPT_NONE=y/g" .config
    sed -i "s/CONFIG_PREEMPT_NONE=n/CONFIG_PREEMPT_NONE=y/g" .config
    sed -i "s/CONFIG_PREEMPT_DYNAMIC=y/# CONFIG_PREEMPT_DYNAMIC is not set/g" .config
    sed -i "s/CONFIG_PREEMPT_BUILD=y/CONFIG_PREEMPT_BUILD=n/g" .config

    # Completely disable network card if needed (helps with bareflank)
    # sed -i "s/CONFIG_WATCHDOG=y/CONFIG_WATCHDOG=n/g" .config
    # sed -i "s/CONFIG_CLOCKSOURCE_WATCHDOG=y/CONFIG_CLOCKSOURCE_WATCHDOG=n/g" .config
    # sed -i "s/CONFIG_E1000=y/CONFIG_E1000=n/g" .config

    # sed -i "s/# CONFIG_PREEMPT is not set/CONFIG_PREEMPT=y\nCONFIG_PREEMPT_BUILD=y\nCONFIG_PREEMPT_RCU=n\nCONFIG_PREEMPT_DYNAMIC=y\nCONFIG_SCHED_CORE=y\n/g" .config
    # # CONFIG_PREEMPT_NONE is not set\n
    # CONFIG_PREEMPT_VOLUNTARY=y\n
    # # CONFIG_PREEMPT is not set\n
    # CONFIG_PREEMPT_COUNT=y\n
    # CONFIG_PREEMPTION=y\n
    # CONFIG_PREEMPT_DYNAMIC=y\n
    # CONFIG_SCHED_CORE=y\n/g" .config

    cat .config | grep CONFIG_PREEMPT
    cat .config | grep CONFIG_PREEMPT_RCU || true

    # Start the build process
    make -j`nproc`
popd

# Export required environment variables
export VMDISKMOUNT
export VMDISK
export VMDISKFOLDER
export LINUXFOLDER
export LINUXVERSION

# Install the kernel modules and image
pushd ../common
    ./install-modules.sh
    ./install-image.sh
popd

./copy-source.sh
