#!/bin/bash -e

source .env

# build dependencies
sudo apt update
sudo apt install pkg-config ninja-build libglib2.0-dev\
                 libgcrypt20-dev zlib1g-dev autoconf\
                 automake libtool bison flex libpixman-1-dev\
                 build-essential openssh-server\
                 cloud-image-utils virt-manager -y

# Chuqi: no longer need a specific QEMU right now
# since we rely on real PKS hardware

# # Download QEMU 7.1.0
# if [ ! -d $QEMU_DIR ]; then
#     # Any QEMU version over 6.0 has TCG-based PKS emulation
#     wget https://download.qemu.org/$QEMU_DIR.tar.xz
#     tar xvJf $QEMU_DIR.tar.xz && rm $QEMU_DIR.tar.xz
# fi

# # build qemu
# if [ ! -d $QEMU_DIR/build ]; then
#     pushd $QEMU_DIR
#     ./configure
#     make -j`nproc`
#     popd
# fi

# # install qemu
# pushd $QEMU_DIR/build
#     sudo make install
# popd

