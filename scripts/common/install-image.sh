#!/bin/bash -e 

pushd ../ && source .env && popd

log_info "Executing install-image.sh $@"

# Load the vm image (if not currently loaded)
./load-vmdisk.sh $@

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
./unload-vmdisk.sh $@