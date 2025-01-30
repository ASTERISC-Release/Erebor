## Prerequisite

> [!IMPORTANT]
> You will need the **sudo** privilege for building/deployment.

```bash
./pre-req.sh
```

## For Setting-1: Build for Intel TDX (TD guest CVM)

TBC.

## For Setting-2: Build for Intel normal VMs (non-CVM)

1. Create the guest VM disk image.

```bash
./create-vm.sh
```

After this, you will see a folder called `vmdisk/` under this directory. The VM's filesystem/image is created.

An account is created for the guest VM:
- Username: `pks`
- Password: `pks`

2. Build the customized guest VM kernel.

 We have to build the VM guest with Erebor's security monitor enabled in the kernel.

```bash
pushd kernel-build/
    # Yup, use the option -c with no following parameter
    ./build-linux.sh -c
popd
```


> [!NOTE]
If you want to play with the vmdisk image (to mount it to the host filesystem), there is a way:

```bash
pushd common/
    # mount the vmdisk
    ./load-vmdisk.sh -c
    # unmount the vmdisk
    ./unload-vmdisk.sh -c
popd
```

Once you mounted the vmdisk image to the host filesystem, you should be able to see the mounted content under `vmdisk/mnt/`.

3. Start the guest VM.

```bash
./run-kvm-vm.sh
```

## Build for AMD SEV-ES

> :stop_sign:
> Please note that our paper/project no longer supports AMD SEV-ES.
> The below instructions are deprecated.

- Install a QEMU/OVMF compatible with SEV
```
cd <project-dir>/host-os/qemu
./build.sh qemu
./build.sh ovmf
```

- Create an Ubuntu cloud image (native version)
```
./create-image.sh
```

- Install your customized to the kernel image (TODO)
