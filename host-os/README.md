## Build host-OS

To support both deployment settings, our host Linux/KVM should be extended to support PKS virtualization.

## A. Setting-1 (on a TDX machine with CVM):

Please clone and execute the scripts in this [repository](https://github.com/Icegrave0391/Linux-KVM_Intel-TDX-PKS).

## B. Setting-2 (on a normal PC with normal-VM):

> [!NOTE]
> This is recommended for personal testing and development.

Please execute the following commands:

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
