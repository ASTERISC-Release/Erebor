#!/bin/bash -e

pushd ../ && source .env && popd

log_info "Executing install-modules.sh $1"

# Load the disk
./load-vmdisk.sh $1

if [[ $1 == "tdx" ]]; then
  VMDISK=$VMDISK_TDX
  VMDISKMOUNT=$VMDISKMOUNT_TDX
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
./unload-vmdisk.sh $1