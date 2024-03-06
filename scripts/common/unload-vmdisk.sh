#!/bin/bash -e

# source environment variables
pushd ../ && source .env && popd

# check if folder is empty
if [ -z "$(ls -A $VMDISKMOUNT)" ]; 
then
   echo "Empty mount point for vmdisk; hence, exiting"
   exit
fi

# unmount the disk
sudo umount $VMDISKMOUNT/boot/efi || true
sudo umount $VMDISKMOUNT/dev/pts || true
sudo umount $VMDISKMOUNT/dev || true
sudo umount $VMDISKMOUNT/sys || true
sudo umount $VMDISKMOUNT/proc || true
sudo umount $VMDISKMOUNT/run || true
sudo umount $VMDISKMOUNT

# unmount the nbd
sudo qemu-nbd -d /dev/nbd0
