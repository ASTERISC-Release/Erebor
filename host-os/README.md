## Build host-OS

Our host OS (KVM) should be extended to support PKS virtualization.

Please run the following command to build and install the host OS:

```bash
# compile and install the kernel
./build-host-os.sh
# Please then use the following boot images
/boot/vmlinuz-5.18.0-rc3+
/boot/initrd.img-5.18.0-rc3+
```

**Note:**
You may have to disable the *secure boot* in UEFI/BIOS to enable the customized kernel boot. 

Otherwise, please follow this instruction to sign the kernel: [Signing a Linux Kernel for Secure Boot](https://gloveboxes.github.io/Ubuntu-for-Azure-Developers/docs/signing-kernel-for-secure-boot.html).