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

CURDIR=$(pwd)
# Build the kernel executable
pushd $LINUXFOLDER
    # Make the default configuration
    make defconfig

    # Update the configuration for KVM/QEMU guests
    # NOTE: In some older kernels, it was called "make kvmconfig"
    # make kvm_guest.config
   
    # copy the config
    cp $CURDIR/def_config .config

    # Huge page
    # sed -i "s/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD=y/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD=n/g" .config
    # sed -i "s/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE=y/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE=n/g" .config
    # sed -i "s/CONFIG_ARCH_ENABLE_HUGEPAGE_MIGRATION=y/CONFIG_ARCH_ENABLE_HUGEPAGE_MIGRATION=n/g" .config
    
    # sed -i "s/CONFIG_HAVE_ARCH_HUGE_VMAP=y/CONFIG_HAVE_ARCH_HUGE_VMAP=n/g" .config
    # sed -i "s/CONFIG_HAVE_ARCH_HUGE_VMALLOC=y/CONFIG_HAVE_ARCH_HUGE_VMALLOC=n/g" .config
    # sed -i "s/CONFIG_ARCH_WANT_HUGE_PMD_SHARE=y/CONFIG_ARCH_WANT_HUGE_PMD_SHARE=n/g" .config
    # sed -i "s/CONFIG_ARCH_WANT_OPTIMIZE_HUGETLB_VMEMMAP=y/CONFIG_ARCH_WANT_OPTIMIZE_HUGETLB_VMEMMAP=n/g" .config
    # sed -i "s/CONFIG_ARCH_WANT_GENERAL_HUGETLB=y/CONFIG_ARCH_WANT_GENERAL_HUGETLB=n/g" .config

    # sed -i "s/CONFIG_HUGETLBFS=y/CONFIG_HUGETLBFS=n/g" .config
    # sed -i "s/CONFIG_HUGETLB_PAGE=y/CONFIG_HUGETLB_PAGE=n/g" .config
    # sed -i "s/CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP=y/CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP=n/g" .config
    # sed -i "s/CONFIG_CGROUP_HUGETLB=y/CONFIG_CGROUP_HUGETLB=n/g" .config
    
    # Enable kernel debugging using GDB
	sed -i "s/CONFIG_RANDOMIZE_BASE=y/CONFIG_RANDOMIZE_BASE=n/g" .config 
    sed -i "s/CONFIG_DEBUG_INFO_NONE=y/CONFIG_DEBUG_INFO=y/g" .config 
    
    # CMA allocator
    sed -i "s/# CONFIG_CMA is not set/CONFIG_CMA=y/g" .config
    sed -i "s/# CONFIG_DMA_CMA is not set/CONFIG_DMA_CMA=y/g" .config
    
    # Enable full tickless kernel
    sed -i "s/# CONFIG_NO_HZ_FULL is not set/CONFIG_NO_HZ_IDLE=y/g" .config
    
    # Premption settings for hv-instrument
    sed -i "s/CONFIG_PREEMPT_VOLUNTARY=y/# CONFIG_PREEMPT_VOLUNTARY is not set/g" .config
    sed -i "s/# CONFIG_PREEMPT_NONE is not set/CONFIG_PREEMPT_NONE=y/g" .config
    sed -i "s/CONFIG_PREEMPT_NONE=n/CONFIG_PREEMPT_NONE=y/g" .config
    sed -i "s/CONFIG_PREEMPT_DYNAMIC=y/# CONFIG_PREEMPT_DYNAMIC is not set/g" .config
    sed -i "s/CONFIG_PREEMPT_BUILD=y/CONFIG_PREEMPT_BUILD=n/g" .config
    
    # Disabling the Indirect Branch Tracking, as it is giving compile errors with the secure stack switch
    sed -i "s/CONFIG_X86_KERNEL_IBT=y/CONFIG_X86_KERNEL_IBT=n/g" .config

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

./copy-source.sh |& tee -a $CURDIR/build.kern.log
