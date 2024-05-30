#!/bin/bash -e

# Set up the required environment variables
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

sudo unlink $VMDISKMOUNT/lib/modules/$LINUXVERSION/build || true
sudo rm -rf $VMDISKMOUNT/lib/modules/$LINUXVERSION/build
sudo mkdir $VMDISKMOUNT/lib/modules/$LINUXVERSION/build
sudo rsync -av --info=progress2 $LINUXFOLDER/* $VMDISKMOUNT/lib/modules/$LINUXVERSION/build
# sudo cp -r $LINUXFOLDER/* $VMDISKMOUNT/lib/modules/$LINUXVERSION/build

./unload-vmdisk.sh $1