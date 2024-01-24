# Kernel Building from Source (for a QEMU VM)

1. Get the pre-requisite packages: `pre-req.sh`

2. Obtain the required linux version: `obtain-linux.sh`
    - example: `obtain-linux.sh 5 9.6` to get linux-v5.9.6

3. Build (and install) the kernel in VM: `./build-linux.sh`
    - Note that this assumes your VM is already created using the scripts
    in the `vm-create` folder.

## References 

- https://www.thegeekdiary.com/centos-rhel-7-how-to-configure-serial-getty-with-systemd/ 
