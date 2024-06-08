#!/bin/bash -e

# Get environment variables
source .env

# Initial information
echo "----------------------------------"
echo "Function: This script restarts a stopped TDX CVM"
echo ""
echo "  1. Mapping CTRL-C to CTRL-]"
echo "  2. Press CTRL-] to stop the VM"
echo "----------------------------------"
echo ""

# Disable huge pages (actually useless)
sudo sh -c "echo never > /sys/kernel/mm/transparent_hugepage/enabled"
sudo sh -c "echo never > /sys/kernel/mm/transparent_hugepage/defrag"

cleanup() {
    rm -f /tmp/tdx-guest-*.log &> /dev/null
    rm -f /tmp/tdx-demo-*-monitor.sock &> /dev/null
    if [ -f /tmp/tdx-demo-td-pid.pid ]; then
        PID_TD=$(cat /tmp/tdx-demo-td-pid.pid 2> /dev/null)
        [ ! -z "$PID_TD" ] && echo "Cleanup, kill TD with PID: ${PID_TD}" && kill -TERM ${PID_TD} &> /dev/null || true
        sleep 3
    fi
}

cleanup
if [ "$1" = "clean" ]; then
    exit 0
fi

#TD_IMG=${TD_IMG:-${PWD}/image/tdx-guest-ubuntu-24.04-generic.qcow2}
TD_IMG=${TD_IMG:-$VMDISK_TDX}
TDVF_FIRMWARE=/usr/share/ovmf/OVMF.fd

if ! groups | grep -qw "kvm"; then
    echo "Please add user $USER to kvm group to run this script (usermod -aG kvm $USER and then log in again)."
    exit 1
fi

set -e

###################### RUN VM WITH TDX SUPPORT ##################################
SSH_PORT=10022
PROCESS_NAME=td
VMMVM=24G

#stty intr ^]

# -name ${PROCESS_NAME},process=${PROCESS_NAME},debug-threads=on \
# approach 1 : talk to QGS directly
QUOTE_ARGS="-device vhost-vsock-pci,guest-cid=3"
qemu-system-x86_64 -D /tmp/tdx-guest-td.log \
		   -accel kvm \
		   -m $VMMVM \
           -cpu host,-pdpe1gb \
           -smp 32,maxcpus=32 \
		   -name ${PROCESS_NAME},process=${PROCESS_NAME}\
		   -object tdx-guest,id=tdx \
		   -machine q35,kernel_irqchip=split,confidential-guest-support=tdx,hpet=off \
		   -bios ${TDVF_FIRMWARE} \
		   -nographic\
		   -nodefaults -daemonize\
		   -no-reboot \
           -netdev user,id=nic0_td,hostfwd=tcp::${SSH_PORT}-:22 \
           -device virtio-net-pci,netdev=nic0_td \
		   -drive file=${VMDISK_TDX},if=none,id=virtio-disk0 \
		   -device virtio-blk-pci,drive=virtio-disk0 \
		   ${QUOTE_ARGS} \
		   -pidfile /tmp/tdx-demo-td-pid.pid

# Chuqi: They are for background -daemonize execution
PID_TD=$(cat /tmp/tdx-demo-td-pid.pid)
echo "TD, PID: ${PID_TD}, SSH : ssh -p 10022 root@localhost"

# restore the mapping
#stty intr ^c
