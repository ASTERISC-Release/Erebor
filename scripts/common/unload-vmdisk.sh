#!/bin/bash -e

# source environment variables
pushd ../ && source .env && popd

if [[ $1 == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
fi

# check if folder is empty
if [ -z "$(ls -A $VMDISKMOUNT)" ]; 
then
   log_info "Empty mount point for vmdisk ($VMDISK); hence, exiting"
   exit
fi

log_info "Unmounting VMDISK=$VMDISKMOUNT"

# unmount the disk
sudo umount $VMDISKMOUNT/boot/efi || true
if [[ $1 == "tdx" ]]; then
	sudo umount $VMDISKMOUNT/boot || true
fi
sudo umount $VMDISKMOUNT/dev/pts || true
sudo umount $VMDISKMOUNT/dev || true
# tdx ubuntu 24.04
if [[ $1 == "tdx" ]]; then
	sudo umount $VMDISKMOUNT/sys/firmware/efi/efivars || true
fi
sudo umount $VMDISKMOUNT/sys || true
sudo umount $VMDISKMOUNT/proc || true
sudo umount $VMDISKMOUNT/run || true
sudo umount $VMDISKMOUNT

# unmount the nbd
sudo qemu-nbd -d /dev/nbd0
