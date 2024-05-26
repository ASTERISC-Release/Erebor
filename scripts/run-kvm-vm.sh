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

# Disable huge pages
sudo sh -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
sudo sh -c "echo never > /sys/kernel/mm/transparent_hugepage/defrag"

# Map CTRL-C to CTRL-]
stty intr ^]

# memory (32GB)
VMMEM=24576M

# debug
GDB=""
if [[ $1 == "debug" ]]; then
    #GDB="-gdb tcp::1234"
    GDB="-s -S"
    echo "Enable GDB debugging."
fi
# launch the QEMU VM
# the 'max' version of the emulation provides all 
# CPU features (including our needed PKS)
sudo qemu-system-x86_64 -enable-kvm -cpu host,+pks -smp 8,maxcpus=8\
    -m $VMMEM -no-reboot -netdev user,id=vmnic,hostfwd=tcp::8000-:22\
    -device e1000,netdev=vmnic,romfile= -drive file=$VMDISK,if=none,id=disk0,format=qcow2\
    -device virtio-scsi-pci,id=scsi0,disable-legacy=on,iommu_platform=true \
    -device scsi-hd,drive=disk0 -nographic -monitor pty -monitor unix:monitor,server,nowait\
    $GDB

# restore the mapping
stty intr ^c
