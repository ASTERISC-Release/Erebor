# Erebor

<!-- Since Intel PKS is only implemented in a very few CPUs right now, we shall explore its use in implementing intra-kernel isolation through software emulation (QEMU). -->

In this repository, we provide steps to deploy and test the functionalities of our research prototype.

Due to guest CVMs are only available to specific server hardware with Intel TDX support, we provide two settings for ease of deployment:

- 1. **Full CVM-functional testing.** This requires a host Intel server machine with Intel TDX supported. This setting follows the full CVM system model as mentioned in the paper.
- 2. **Normal VM testing.** This merely requires a host Intel machine (without the need to support Intel TDX). In case of 

> [!IMPORTANT]
> Please make sure that your Intel machine has Protection Keys Supervisor (PKS) support. Use this [script](https://github.com/Icegrave0391/check-pks) to check whether PKS is supported.

3. Finally, run the VM using `run-vm.s`.
    - Once the VM starts, you can run `cpuid | grep PKS` to check if PKS is available. 

## Init the repo

```bash
# fetch and switch to the latest branch
git fetch
git checkout tdx-eval-pks-no-wp
# fetch all required submodule (gramine)
git submodule update --init --recursive
```

## Useful Links

- QEMU documentation for full-system emulation
    - https://www.qemu.org/docs/master/system/index.html 

- QEMU documentation for TCG-based emulation
    - https://www.qemu.org/docs/master/devel/index-tcg.html 

- Understanding QEMU
    - https://airbus-seclab.github.io/qemu_blog/ 
