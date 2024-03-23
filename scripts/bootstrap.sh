#!/bin/bash -e

# source environment variables
source .env

pushd common && ./load-vmdisk.sh && popd

sleep 1
echo "Adding DHCP on reboot"
echo "@reboot sudo dhclient" | sudo chroot $VMDISKMOUNT crontab -u $USERNAME -

echo "Removing password"
sudo chroot $VMDISKMOUNT sudo passwd -d $USERNAME
sleep 1

pushd common && ./unload-vmdisk.sh && popd