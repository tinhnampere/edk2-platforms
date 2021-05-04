# flashkernel

The LinuxBoot image, named flashkernel, is required to build the final EDK2 image with LinuxBoot support.
The flashkernel image consists of [Linux](https://kernel.org) kernel and initramfs generated using [u-root](https://github.com/u-root/u-root).

## Overview

LinuxBoot is a firmware that replaces specific firmware functionality
like the UEFI DXE phase with a Linux kernel and runtime. It is built-in
UEFI image like an application as it will be executed at the end of DXE phase.

The flashkernel is built completely from the [linuxboot/mainboards](https://github.com/linuxboot/mainboards) repository.

## How to build

1. Clone the `linuxboot/mainboards` repository:

    ```Bash
    git clone https://github.com/linuxboot/mainboards.git
    ```

2. Build with the following command:

    ```Bash
    cd ampere/jade
    make fetch flashkernel
    ```

Once the build is done, copy the flashkernel into the edk2-platform under the `Platform/Ampere/LinuxBootPkg/AArch64` directory.
