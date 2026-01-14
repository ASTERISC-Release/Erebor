#!/bin/bash
LINUX_DIR="linux-pks-kvm"

if [ ! -d $LINUX_DIR ]; then
	exit 1
fi

pushd $LINUX_DIR
	for i in $(seq 1 44)
	do
		echo "patching file ../patches/pks_$i.patch"
		patch -p1 < ../patches/pks_$i.patch
	done
popd
