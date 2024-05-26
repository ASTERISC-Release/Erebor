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


    # Huge page
    sed -i "s/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD=y/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD=n/g" .config
    sed -i "s/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE=y/CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE=n/g" .config
    sed -i "s/CONFIG_ARCH_ENABLE_HUGEPAGE_MIGRATION=y/CONFIG_ARCH_ENABLE_HUGEPAGE_MIGRATION=n/g" .config
    
    sed -i "s/CONFIG_HAVE_ARCH_HUGE_VMAP=y/CONFIG_HAVE_ARCH_HUGE_VMAP=n/g" .config
    sed -i "s/CONFIG_HAVE_ARCH_HUGE_VMALLOC=y/CONFIG_HAVE_ARCH_HUGE_VMALLOC=n/g" .config
    sed -i "s/CONFIG_ARCH_WANT_HUGE_PMD_SHARE=y/CONFIG_ARCH_WANT_HUGE_PMD_SHARE=n/g" .config
    sed -i "s/CONFIG_ARCH_WANT_OPTIMIZE_HUGETLB_VMEMMAP=y/CONFIG_ARCH_WANT_OPTIMIZE_HUGETLB_VMEMMAP=n/g" .config
    sed -i "s/CONFIG_ARCH_WANT_GENERAL_HUGETLB=y/CONFIG_ARCH_WANT_GENERAL_HUGETLB=n/g" .config

    sed -i "s/CONFIG_HUGETLBFS=y/CONFIG_HUGETLBFS=n/g" .config
    sed -i "s/CONFIG_HUGETLB_PAGE=y/CONFIG_HUGETLB_PAGE=n/g" .config
    sed -i "s/CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP=y/CONFIG_HUGETLB_PAGE_OPTIMIZE_VMEMMAP=n/g" .config
    sed -i "s/CONFIG_CGROUP_HUGETLB=y/CONFIG_CGROUP_HUGETLB=n/g" .config

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
