#!/bin/bash -e

# Assertions and usage
if [ $# -ne 2 ]; then
    echo "Usage: ./obtain-linux.sh <kern_major_ver> <kern_minor_ver>"
    echo "      e.g., ./obtain-linux.sh 5 9.6"
    exit
fi

# Print kernel source information 
echo "Kernel version to obtain is $1.$2"
echo "Major version --> $1"
echo "Minor version --> $2"

# Get the correct kernel source
wget https://cdn.kernel.org/pub/linux/kernel/v$1.x/linux-$1.$2.tar.xz

# Untar the kernel source
tar -xvf linux-$1.$2.tar.xz

# Save kernel version in file
echo "LINUXVERSION=$1.$2" > .env
echo "
LINUXFOLDER=`pwd`/linux-$1.$2
VMDISKFOLDER=`pwd`/../vm-create/vmdisk
VMDISKMOUNT=\$VMDISKFOLDER/mnt
VMDISK=\$VMDISKFOLDER/ubuntu-vm.qcow2
" >> .env