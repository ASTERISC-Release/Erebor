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

# launch the QEMU VM
# the 'max' version of the emulation provides all 
# CPU features (including our needed PKS)
qemu-system-x86_64 -cpu max -smp 1,maxcpus=1\
     -m 4096M -no-reboot -netdev user,id=vmnic,hostfwd=tcp::8000-:22\
    -device e1000,netdev=vmnic,romfile= -drive file=$VMDISK,if=none,id=disk0,format=qcow2\
    -device virtio-scsi-pci,id=scsi0,disable-legacy=on,iommu_platform=true \
    -device scsi-hd,drive=disk0 -nographic -monitor pty -monitor unix:monitor,server,nowait

# restore the mapping
stty intr ^c
