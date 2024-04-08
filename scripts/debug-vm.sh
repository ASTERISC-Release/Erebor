#!/bin/bash

source .env
VMLINUX=$LINUXFOLDER/vmlinux
gdb $VMLINUX -ex "target remote:1234"
