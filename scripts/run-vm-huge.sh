#!/bin/bash -e

# Get environment variables
source .env

# Initial information
echo "----------------------------------"
echo "Function: This script restarts a stopped virtual machine"
echo ""
echo "  1. Mapping CTRL-C to CTRL-]"
echo "  2. Press CTRL-] to stop the VM"
echo "----------------------------------"
echo ""

# Map CTRL-C to CTRL-]
stty intr ^]

# memory (12GB)
# VMMEM=12288M
# memory (64GB)
VMMEM=24576M

# debug
GDB=""
if [[ $1 == "debug" ]]; then
    GDB="-gdb tcp::1234"
    echo "Enable GDB debugging."
fi
# launch the QEMU VM
# the 'max' version of the emulation provides all 
# CPU features (including our needed PKS)
qemu-system-x86_64 -cpu host -enable-kvm -smp 24,maxcpus=24\
    $GDB\
    -m $VMMEM -no-reboot -netdev user,id=vmnic,hostfwd=tcp::8000-:22\
    -device e1000,netdev=vmnic,romfile= -drive file=$VMDISK,if=none,id=disk0,format=qcow2\
    -device virtio-scsi-pci,id=scsi0,disable-legacy=on,iommu_platform=true \
    -device scsi-hd,drive=disk0 -nographic -monitor pty -monitor unix:monitor,server,nowait

# restore the mapping
stty intr ^c
