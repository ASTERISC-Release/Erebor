#!/bin/bash -e

# Download QEMU 7.1
# Any QEMU version over 6.0 has TCG-based PKS emulation
wget https://download.qemu.org/qemu-7.1.0.tar.xz
tar xvJf qemu-7.1.0.tar.xz
cd qemu-7.1.0
./configure
make -j`nproc`