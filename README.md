# Erebor

<!-- Since Intel PKS is only implemented in a very few CPUs right now, we shall explore its use in implementing intra-kernel isolation through software emulation (QEMU). -->

In this repository, we provide steps to deploy and test the functionalities of our research prototype.

Due to guest CVMs are only available to specific server hardware with Intel TDX support, we provide two settings for ease of deployment:

- **Setting-1: Full CVM functional testing.** This requires a host Intel server machine with Intel TDX supported. This setting follows the full CVM system model as mentioned in the paper.

- **Setting-2: Normal VM functional testing.** This merely requires a host Intel machine (without the need to support Intel TDX). Only Xeon 5th servers have Intel TDX support now. This setting uses normal guest VMs to mimic the CVM model, for only functionality tests and development.

**Note:** To deployment on PC for personal testing, please simply use **Setting-2**.

> [!IMPORTANT]
> Please make sure that your Intel machine has *Protection Keys Supervisor (PKS)* support. Use this [script](https://github.com/Icegrave0391/check-pks) to check whether PKS is supported.

3. Finally, run the VM using `run-vm.s`.
    - Once the VM starts, you can run `cpuid | grep PKS` to check if PKS is available. 

## 1. Init the repo

Please run the following shell commands to initialize the source.

```bash
# fetch and switch to the latest branch
git fetch
git checkout tdx-eval-pks-no-wp
# fetch all required submodule (gramine)
git submodule update --init --recursive
```

## 2. Prepare the host machine

While Erebor does not require host-side hypervisor/OS changes, we indeed have to patch today's Linux/KVM to support *PKS virtualization* (for both **Setting-1 and Setting-2**) and support *PKS within TDX's TD guests* (for **Setting-1**).

Please `cd host-os` and follow the instructions in [host-os/](host-os/) to set up the host kernel/hypervisor.

## 3. Prepare the environment for the guest (C)VM

Now it is time to prepare the environments for guest CVM (normal VMs in case of **Setting-2**). Please `cd scripts` and follow the instructions in [scripts/](scripts/) to create the build and execution environment for guest CVMs.

## Useful Links

- QEMU documentation for full-system emulation
    - https://www.qemu.org/docs/master/system/index.html 

- QEMU documentation for TCG-based emulation
    - https://www.qemu.org/docs/master/devel/index-tcg.html 

- Understanding QEMU
    - https://airbus-seclab.github.io/qemu_blog/ 
