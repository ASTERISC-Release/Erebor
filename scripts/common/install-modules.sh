#!/bin/bash -e

pushd ../ && source .env && popd

PARAMS="$@"
log_info "Executing install-modules.sh ${PARAMS}"

# Load the disk
./load-vmdisk.sh ${PARAMS}
log_info "HELLO params: ${PARAMS}"

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
  echo "vmdisk_tdx=$VMDISK_TDX"
  echo "VMDISK=$VMDISK"
  echo "VMDISKMOUNT=$VMDISKMOUNT_TDX"
fi


# Solving Linux versioning issues
IFS='.'
read -a strarr <<< "$LINUXVERSION"
IFS=''
if [ ${#strarr[@]} == 2 ]; then
  LINUXVERSION=$LINUXVERSION.0
fi
echo "Proper Linux version: $LINUXVERSION" 

# Install the kernel modules to the vm image
pushd $LINUXFOLDER
  log_info "Installing modules in $VMDISKMOUNT"
  sudo unlink $VMDISKMOUNT/lib/modules/$LINUXVERSION/build || true
  sudo rm -rf $VMDISKMOUNT/lib/modules/$LINUXVERSION/build || true
  sudo env PATH=$PATH make INSTALL_MOD_PATH=$VMDISKMOUNT modules_install || true
popd

# Unload the disk
./unload-vmdisk.sh ${PARAMS}