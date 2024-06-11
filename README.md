# enclave-crossarch

Since Intel PKS is only implemented in a very few CPUs right now, we shall explore
its use in implementing intra-kernel isolation through software emulation (QEMU).

1. If your machine has a QEMU version lower than 6.0.0, please download and build QEMU using `pre-req.sh`.
    - After the script completes, please run `make install` in the build folder.

2. Then, create a VM using `create-vm.sh`.
    - U/P is 'pks'

3. Finally, run the VM using `run-vm.s`.
    - Once the VM starts, you can run `cpuid | grep PKS` to check if PKS is available. 

## Init the repo

```bash
git fetch
git checkout arch-x86
git submodule update --init --recursive
```

## Useful Links

- QEMU documentation for full-system emulation
    - https://www.qemu.org/docs/master/system/index.html 

- QEMU documentation for TCG-based emulation
    - https://www.qemu.org/docs/master/devel/index-tcg.html 

- Understanding QEMU
    - https://airbus-seclab.github.io/qemu_blog/ 
