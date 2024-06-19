#!/bin/bash -e

# Set up the required environment variables
PARAMS="$@"
./load-vmdisk.sh $PARAMS

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

./unload-vmdisk.sh $PARAMS