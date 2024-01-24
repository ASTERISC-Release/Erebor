#!/bin/bash -e 

# Load the vm image (if not currently loaded)
./load-vmdisk.sh

# Install the kernel to the vm image
pushd $LINUXFOLDER
  sudo env PATH=$PATH make INSTALL_PATH=$VMDISKMOUNT/boot install
popd

IFS='.'
read -a strarr <<< "$LINUXVERSION"
IFS=''

# Solving Linux versioning issues
if [ ${#strarr[@]} == 2 ]; then
  LINUXVERSION=$LINUXVERSION.0
fi
echo "Proper Linux version: $LINUXVERSION" 

# Install the initramfs
# note: we must chroot to its directory and then
sudo chroot $VMDISKMOUNT sudo update-initramfs -c -k $LINUXVERSION
sudo chroot $VMDISKMOUNT sudo update-grub

# Unload the vm image
./unload-vmdisk.sh