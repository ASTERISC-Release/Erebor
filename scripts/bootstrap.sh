#!/bin/bash -e

# source environment variables
source .env

pushd common && ./load-vmdisk.sh && popd

sleep 1

echo "1: Adding DHCP on reboot"
echo "@reboot sudo dhclient" | sudo chroot $VMDISKMOUNT crontab -u $USERNAME -

echo "2: Disabling cloud-init"
sudo touch $VMDISKMOUNT/etc/cloud/cloud-init.disabled
sudo chroot $VMDISKMOUNT sudo systemctl disable systemd-networkd-wait-online.service
sudo chroot $VMDISKMOUNT sudo systemctl mask systemd-networkd-wait-online.service

echo "3: Disable login password"
sudo chroot $VMDISKMOUNT sudo passwd -d $USERNAME

echo "4: Generate host keys"
sudo rm -rf $VMDISKMOUNT/etc/ssh/ssh_host*
sudo chroot $VMDISKMOUNT sudo ssh-keygen -A

echo "5: Disabling password on sudoers"
sudo chroot $VMDISKMOUNT sed -i '/^%sudo/c\%sudo\tALL=(ALL:ALL) NOPASSWD:ALL' /etc/sudoers

echo "6: Adding hostname on /etc/hosts"
sudo chroot $VMDISKMOUNT sed -i "/^127\\.0\\.0\\.1 /s/\$/ $VMNAME/" /etc/hosts

echo "7: Adding host public key as authorized in VM"
sudo mkdir -p $VMDISKMOUNT/home/$USERNAME/.ssh
sudo touch $VMDISKMOUNT/home/$USERNAME/.ssh/authorized_keys
cat ~/.ssh/id_rsa.pub | sudo tee $VMDISKMOUNT/home/$USERNAME/.ssh/authorized_keys
sudo chroot $VMDISKMOUNT sudo chown -R $USERNAME home/$USERNAME/.ssh
sudo chroot $VMDISKMOUNT sudo chgrp -R $USERNAME home/$USERNAME/.ssh

sleep 1

pushd common && ./unload-vmdisk.sh && popd
