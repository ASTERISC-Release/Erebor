#!/bin/bash -e

# source environment variables
pushd ../ && source .env && popd

if [[ $1 == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
fi

# unload first, if disk was loaded
./unload-vmdisk.sh $1 || true

log_info "Mounting VMDISK=$VMDISKMOUNT"

# install the nbd module
sudo modprobe nbd max_part=8

# create a folder to load the vm image
if [ ! -d $VMDISKMOUNT ]
then
  mkdir -p $VMDISKMOUNT
fi

# connect the qcow2 image
sudo qemu-nbd --connect=/dev/nbd0 $VMDISK --cache=unsafe --discard=unmap

# install the root directory (it is partition 2 usually)
sudo fdisk /dev/nbd0 -l

# mount the simple drives
sudo mount /dev/nbd0p1 $VMDISKMOUNT
# tdx ubuntu 24.04 image has to mount the /boot separately
if [[ $1 == "tdx" ]]; then
	sudo mount /dev/nbd0p16 $VMDISKMOUNT/boot || true
fi
sudo mount /dev/nbd0p15 $VMDISKMOUNT/boot/efi || true

# mount the /dev and /sys folders too. This is needed for update-grub command.
sudo mount -o bind /dev $VMDISKMOUNT/dev
sudo mount -o bind /dev/pts $VMDISKMOUNT/dev/pts
sudo mount -o bind /proc $VMDISKMOUNT/proc
sudo mount -o bind /run $VMDISKMOUNT/run
sudo mount -o bind /sys $VMDISKMOUNT/sys
