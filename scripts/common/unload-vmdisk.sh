#!/bin/bash -e

# source environment variables
pushd ../ && source .env && popd

PARAMS="$@"

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

INSTALL_CVM=$(echo "$INSTALL_CVM" | xargs)
if [[ $INSTALL_CVM == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
fi

#debug 
log_info "unload-vmdisk.sh ${PARAMS} (install_CVM=${INSTALL_CVM})"
log_info "Unmounting VMDISK=$VMDISKMOUNT"

# check if folder is empty
if [ -z "$(ls -A $VMDISKMOUNT)" ]; 
then
   log_info "Empty mount point for vmdisk ($VMDISK); hence, exiting"
   exit
fi


# unmount the disk
sudo umount $VMDISKMOUNT/boot/efi || true
if [[ $INSTALL_CVM == "tdx" ]]; then
	sudo umount $VMDISKMOUNT/boot || true
fi
sudo umount $VMDISKMOUNT/dev/pts || true
sudo umount $VMDISKMOUNT/dev || true
# tdx ubuntu 24.04
if [[ $INSTALL_CVM == "tdx" ]]; then
	sudo umount $VMDISKMOUNT/sys/firmware/efi/efivars || true
fi
sudo umount $VMDISKMOUNT/sys || true
sudo umount $VMDISKMOUNT/proc || true
sudo umount $VMDISKMOUNT/run || true
sudo umount $VMDISKMOUNT

# unmount the nbd
sudo qemu-nbd -d /dev/nbd0
