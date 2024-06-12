#!/bin/bash -e

source .env

if [ "$1" == "snp" ]; then
    # Start the VM with SNP and security monitor enabled
    sudo ./sev-qemu.sh -hda $VMDISK -sev-snp -svsm ../monitor/svsm.bin -allow-debug
else
    # Start the VM with normal SEV-ES (no SNP)
    sudo ./sev-qemu.sh -cpu-features "-pdpe1gb" -smp 8 -mem 16G -hda $VMDISK -sev
fi
