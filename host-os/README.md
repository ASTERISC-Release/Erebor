## Build host-OS

To support both deployment settings, our host Linux/KVM should be extended to support PKS virtualization.

If you want to deploy **Setting-1** (on a TDX machine with CVM):

Please execute the scripts in this [link](https://github.com/Icegrave0391/TDX-PKS-KVM/tree/main).

If you want to deploy **Setting-2** (on a normal PC with normal-VM; this is recommended for personal testing and development):

```bash
# compile and install the kernel
./build-host-os.sh
# Once the host OS is built 
# Please then use the following boot images
/boot/vmlinuz-5.18.0-rc3+
/boot/initrd.img-5.18.0-rc3+
```

Please reboot with the new boot configurations (by setting `/etc/default/grub`).

> [!Note]
> You may have to disable the *secure boot* in UEFI/BIOS to enable the customized kernel boot. 
> Otherwise, please follow this instruction to sign the kernel: [Signing a Linux Kernel for Secure Boot](https://gloveboxes.github.io/Ubuntu-for-Azure-Developers/docs/signing-kernel-for-secure-boot.html).