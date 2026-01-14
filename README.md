# Erebor - EuroSys'25 (Actively WIP)

<!-- Since Intel PKS is only implemented in a very few CPUs right now, we shall explore its use in implementing intra-kernel isolation through software emulation (QEMU). -->

In this repository, we provide steps to deploy and test the functionalities of our research prototype "Erebor: A Drop-In Sandbox Solution for Private Data Processing in Untrusted Confidential Virtual Machines".

> We conducted our experiments inside a guest CVM on an Intel TDX machine provided by Intel. As we no longer have access to that machine, we are now running our experiments on a standard VM equipped with Intel PKS and Intel CET, which still allows us to achieve functional equivalence. Therefore, there are two optional settings:

- **(Default) Normal VM testing.** This merely requires a host Intel machine (without the need to support Intel TDX). Only Xeon 5th servers have Intel TDX support now. This setting uses normal guest VMs to mimic the CVM model, for only functionality tests and development.

- **(Optional) Full CVM testing.** This requires a host Intel server machine with Intel TDX supported. This setting follows the full CVM system model as mentioned in the paper.


> [!NOTE] 
> To deployment on PC for personal testing, please simply use **Default Normal VM Testing**.

> [!IMPORTANT] 
> For **Artifact Evaluation** reviewers:
> It is not necessary to run the following steps from scratch. We provide you with a server machine that already has host environment set up (finished Step 1. to 3.2. below). 
> Please check the paper's **Appendix-A** to set up the environment.

> [!IMPORTANT]
> Please make sure that your **Intel host physical machine** has *Protection Keys Supervisor (PKS)* support. Use run the following commands to check whether PKS is supported:
```bash
cd scripts/check-pks/
./check.sh
```

## Host hardware prerequisite

* Minimum
    - OS distribution: Ubuntu-22.04/24.04
    - Processor: 12-core Intel 12th gen CPU
    - Memory: 32GB memory
    - Graphics: None
    - Hardware features: Intel PKS, Intel CET
    - Storage: 256GB disk

* Optional
    - OS distribution: Ubuntu-22.04/24.04
    - Processor: 56-core Intel Xeon Platinum CPU
    - Memory: 1TB memory
    - Graphics: None
    - Hardware features: Intel PKS, Intel CET, Intel TDX
    - Storage: 1TB disk

*The following instructions assume a minimum hardware spec (**default normal VM test setting**)*.

## 1. Clone and initialize the repository

Please clone this repository to your host *physical* machine.

```bash
$ git clone git@github.com:ASTERISC-Release/Erebor.git
```

## 2. Prepare the host machine (hypervisor patch), EST 30 mins

While Erebor does not require host-side hypervisor/OS changes, we indeed have to patch today's Linux/KVM to support *PKS virtualization* (for both settings) and support *PKS within TDX's TD guests* (for TDX settings).

Please `cd host-os/` and follow the instructions in [host-os/](host-os/) to set up the host kernel/hypervisor.

## 3. Prepare the environment for the guest VM, EST 20 mins

Enter the [scripts/](scripts/) folder (`cd scripts/`) and execute the following steps.

### 3.1. Install the project dependencies 

```bash
$ pwd
~/Erebor/scripts
$ ./pre-req.sh
```

### 3.2. Create the guest VM disk image

```bash
$ ./create-vm.sh
```

After this, you will see a folder called `vmdisk/` under the `scripts/` directory. The VM's filesystem/image is created. An account is also created for the guest VM:
- Username: `pks`
- Password: `pks`

### 3.3. Build the Erebor-enabled VM kernel

We have to build the VM guest with Erebor's security monitor enabled in the kernel.

```bash
$ pwd
~/Erebor/scripts
$ cd kernel-build/
# Use the option -c with None to indicate a normal VM
$ ./build-linux.sh -c None
```

After building the the kernel, it will be installed into the VM's disk image.
You are now able to login to the VM with the installed kernel.

> [!NOTE]
(Optional) If you want to play with the vmdisk image (to mount it to the host filesystem), there is a way:

```bash
$ pwd
~/Erebor/scripts
$ pushd common/
    # mount the vmdisk
$   ./load-vmdisk.sh -c None
    # unmount the vmdisk
$   ./unload-vmdisk.sh -c None
$ popd
```
Once you mounted the vmdisk image to the host filesystem, you should be able to see the mounted content under `vmdisk/mnt/`.

## 3.4. Start and login the guest VM

```bash
$ pwd
~/Erebor/scripts
$ ./run-kvm-vm.sh
# Now your terminal is inside the VM's session
----------------------------------
Function: This script restarts a stopped virtual machine
  1. Mapping CTRL-C to CTRL-]
  2. Press CTRL-] to stop the VM
----------------------------------
ubuntu-vm login: pks
password: pks
pks@ubuntu-vm:~$ whoami
pks
# Shutdown the VM by typing ctrl - ]
pks@ubuntu-vm:~$ ^]
# Return to the host now
$ pwd
~/Erebor/scripts
```

## 4. (In the guest VM) Build/run sandboxes, EST 20 mins

After the aforementioned Step-3, you are able to execute Erebor's guest (C)VM. Now please login to your VM.

### 4.1. Prepare and build the LibOS

The build scripts for the LibOS is under the repository: [ENCOS-LIBOS](https://github.com/Icegrave0391/ENCOS-LIBOS). Please clone and run them inside your guest VM:

```bash
pks@ubuntu-vm:~$ git clone https://github.com/Icegrave0391/ENCOS-LIBOS.git
pks@ubuntu-vm:~$ cd ENCOS-LIBOS
pks@ubuntu-vm:~$ git submodule update --init --recursive
pks@ubuntu-vm:~$ ./pre-req.sh && ./build-gramine.sh
```

After installing the LibOS, the command `gramine-encos` will be installed into your guest VM's `$PATH`.

### 4.2. Run a demo sandbox

Once finish cloning/installing the LibOS repository, there is a demo sandbox for you to execute:

```bash
pks@ubuntu-vm:~$ cd ENCOS-LIBOS/gramine/CI-examples/helloworld
```

For this demo sandbox program, it does not require any input, and will provide the output data `0x4141414141414141 ('AAAAAAAA')`. You can find detailed description at `ENCOS-LIBOS/gramine/CI-examples/helloworld/README.md`.

```bash
pks@ubuntu-vm:~$ pwd
/home/pks/ENCOS-LIBOS/gramine/CI-examples/helloworld
pks@ubuntu-vm:~$ make
pks@ubuntu-vm:~$ gramine-encos helloworld
```

After that, Erebor's security monitor will forward the output data.
As mentioned in the paper, to ease the implementation/evaluation testing, we use an untrusted debugfs file channel to `cat /sys/kernel/debug/encos-output-emulate/out` and see the plaintext output data.

## A. Project Structure

- `scripts/`: The scripts required to install dependencies, build the EREBOR kernel, and manage the VM.
- `host-os/`: PKS virtualization patch scripts required for the host-os
- `kernel/`: The kernel with the EREBOR security monitor implementation.
    - Core (untrusted) device drivers: `kernel/drivers/char/encos`
    - Core (trusted monitor): `kernel/nk`, `kernel/include/sva`
- [ENCOS-LIBOS](https://github.com/Icegrave0391/ENCOS-LIBOS)/: LibOS for the VM sandbox.

### A.1. Key sections/implementation 

All the secure Erebor monitor calls, like memory management functions (e.g., declaring and updating page tables), writes to control status registers, etc., are implemented in `kernel/nk/mmu.c`. The PKS functions to update the PKS protection key of a virtual page are located at `kernel/nk/pks.c`.

The secure monitor call (EMC) gates are defined in `kernel/include/sva/stack.h`. These are implemented as MACROS, encapsulating EREBOR monitor calls and kernel interrupt handlers. Moreover, Erebor-sandbox and kernel's syscall (u2k interfaces) are interposed in `kernel/arch/x86/entry/entry_64.S`. General sandbox management functionalities are under `kernel/nk/enc.c`.

### A.2. TODOs
- Erebor-monitor has to trap the kernel's `copy_from/to_user` and handle it, and this is under development. Copying data between user/kernel triggers page faults, and we are currently working on proper handling (WIP).

- Erebor-monitor's input/output channel is currently emulated by an untrusted kernel debugfs filesystem (registered in the VM's `/sys/kernel/debug/encos-output-emulate/out`) using plaintext data output.
This is to facilitate testing/evaluation. We leave a full proxied output to clients as future work.

## B. Useful Links

- Build scripts for Linux/KVM (v6.8), with Intel TDX and PKS virtualization support for TD CVMs.
    - https://github.com/Icegrave0391/Linux-KVM_Intel-TDX-PKS

- Intel TDX host configuration and guest image preparation
    - https://github.com/Icegrave0391/tdx

- Understanding QEMU
    - https://airbus-seclab.github.io/qemu_blog/ 

## C. Acknowledgements

In addition to all authors credited in the paper, we would also like to thank the authors of the paper *'Nested Kernel: An Operating System Architecture for Intra-Kernel Privilege Separation'*. 
We utilized Nested Kernel codebase in our work, and we encourage readers to refer to their [paper](https://dl.acm.org/doi/10.1145/2786763.2694386) and [repository](https://github.com/nestedkernel/PerspicuOS) as well.
