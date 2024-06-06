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
VMMEM=24G

# debug
GDB=""
if [[ $1 == "debug" ]]; then
    #GDB="-gdb tcp::1234"
    GDB="-s -S"
    echo "Enable GDB debugging."
fi

# launch the QEMU VM
# CPU features (including our needed PKS)
sudo qemu-system-x86_64 \
    -accel kvm \
    -cpu host,-pdpe1gb \
    -smp 32,maxcpus=32 \
    -m $VMMEM \
    -no-reboot \
    -nographic \
    -netdev user,id=vmnic,hostfwd=tcp::8001-:22 \
    -device e1000,netdev=vmnic,romfile= \
    -drive file=$VMDISK_TDX,if=none,id=disk0,format=qcow2 \
    -device virtio-scsi-pci,id=scsi0,disable-legacy=on,iommu_platform=true \
    -device scsi-hd,drive=disk0 \
    -monitor pty \
    -monitor unix:monitor,server,nowait\
    $GDB

# restore the mapping
stty intr ^c
