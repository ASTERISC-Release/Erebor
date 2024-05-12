#!/bin/bash

# build the PKS-enabled KVM-supported kernel
LINUX_DIR="linux-pks-kvm"

# prepare kernel sources
if [ ! -d $LINUX_DIR ]; then
    git clone git@github.com:Icegrave0391/linux-pks-kvm.git
fi

# prepare kernel configs
cp config-pks-kvm ./$LINUX_DIR/.config

# build kernel
pushd $LINUX_DIR
make -j`nproc`
sudo make -j`nproc` INSTALL_MOD_STRIP=1 modules_install 
sudo make -j`nproc` install
popd
