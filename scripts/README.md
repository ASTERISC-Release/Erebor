## Building for AMD SEV-ES

> [!DEPRECATED]
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
