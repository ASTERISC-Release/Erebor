#!/bin/bash -e

# Load the disk
./load-vmdisk.sh

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
  echo "Installing modules in $VMDISKMOUNT"
  sudo unlink $VMDISKMOUNT/lib/modules/$LINUXVERSION/build || true
  sudo rm -rf $VMDISKMOUNT/lib/modules/$LINUXVERSION/build
  sudo env PATH=$PATH make INSTALL_MOD_PATH=$VMDISKMOUNT modules_install
popd

# Unload the disk
./unload-vmdisk.sh