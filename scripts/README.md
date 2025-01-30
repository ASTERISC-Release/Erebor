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
cd kernel-build/
./build-linux.sh -c
```

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
