#!/bin/bash -e

# source environment variables
pushd ../ && source .env && popd

INSTALL_CVM=""
native=0
# Function to display help message
usage() {
  echo "Usage: $0 [-n] [-c CVM]"
  exit 1
}

# Parse command line arguments
while getopts ":nc:" opt; do
  case $opt in
    n)
      native=1
      ;;
    c)
      INSTALL_CVM="$OPTARG"
      ;;
    \?)
      echo "Invalid option: -$OPTARG" >&2
      usage
      ;;
    :)
      echo "Option -$OPTARG requires an argument." >&2
      usage
      ;;
  esac
done
# Shift off the options and optional --.
shift $((OPTIND - 1))

if [ $native -eq 1 ]; then
    LINUXFOLDER=$LINUXFOLDER_NATIVE
fi

if [[ $INSTALL_CVM == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
fi

# unload first, if disk was loaded
./unload-vmdisk.sh $@ || true

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
