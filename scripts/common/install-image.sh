#!/bin/bash -e 

pushd ../ && source .env && popd

log_info "Executing install-image.sh $1"

# Load the vm image (if not currently loaded)
./load-vmdisk.sh $1

if [[ $1 == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
fi

# Install the kernel to the vm image
pushd $LINUXFOLDER
  log_info "Installing image in $VMDISKMOUNT/boot"
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
sudo chroot $VMDISKMOUNT sudo update-initramfs -c -k $LINUXVERSION || true

# I prefer to update the grub manually, because we may not 
# using the latest kernel version...
# sudo chroot $VMDISKMOUNT sudo update-grub || true

# Unload the vm image
# ./unload-vmdisk.sh $1