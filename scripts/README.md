## Building for AMD SEV-ES

- Install a QEMU/OVMF compatible with SEV
```
cd <project-dir>/host-os
./build.sh qemu ovmf
```

- Create an Ubuntu cloud image (native version)
```
./create-image.sh
```

- Install your customized to the kernel image (TODO)
