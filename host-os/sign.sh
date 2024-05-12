#!/bin/bash

pushd $HOME
VMLINUZ=/boot/vmlinuz-5.18.0-rc3+
sudo sbsign --key MOK.priv --cert MOK.pem $VMLINUZ --output $VMLINUZ.signed

INITRD=/boot/initrd.img-5.18.0-rc3+
sudo cp $INITRD{,.signed}
popd
