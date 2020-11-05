# Overview

This document provides the guideline to build UEFI firmware for Ampere Computing's arm64 reference platforms.

Platform code is located under Platform/Ampere/{Platform Name}Pkg.

Silicon code is located under Silicon/Ampere/Ampere{SoC Name}Pkg.

# How to build (Linux Environment)

## Build machines
The buid instructions provided in this document are supported on the following operating systems:
- Ubuntu 18.04 (x86)
- CentOS 8.2 (x86)

However, it does not mean this guideline is not applicable for other Ubuntu/CentOS versions, or other Linux distributions.
You may, at your own risk, try it on other Linux distributions if all steps below are performed correctly.

## Essential development tools
The following is the list of tools needed for development: `bison, build-essential, bzip2, default-jre, flex
gawk, gpg, libc6:i386, libgcc1:i386, openssl, libssl-dev, m4, make, python, python3-distutils, tar, uuid-dev wget`

They can be installed using standard OS distro's 'apt get' (for Ubuntu) or 'yum' (for CentOS).

## Toolchain

### GCC for aarch64
Ampere provides GCC toolchains
[aarch64-ampere-linux-gnu](https://cdn.amperecomputing.com/tools/compilers/cross/8.3.0/ampere-8.3.0-20191025-dynamic-nosysroot-crosstools.tar.xz)

Linaro provides GCC toolchains for
[aarch64-linux-gnu](https://releases.linaro.org/components/toolchain/binaries/latest/aarch64-linux-gnu/)

**Linaro toolchain is not supported at this time**

Throughout this document, Ampere toolchain is assumed to be installed at location /opt/toolchain/.

When cross compiling, it is necessary to provide the appropriate compilation prefix depending on whether Ampere toolchain or Linaro toolchain is used.

Target architecture | Cross compilation prefix
--------------------|-------------------------
AARCH64             | aarch64-linux-gnu-  (Linaro toolchain)
AARCH64             | aarch64-ampere-linux-gnu- (Ampere toolchain)

### Intel ASL+ Optimizing Compiler/Disassembler
Download and install the ISAL compiler as follows:
   ```
   $ wget https://acpica.org/sites/acpica/files/acpica-unix2-20200110.tar.gz
   $ tar xzf acpica-unix2-20200110.tar.gz
   $ cd acpica-unix2-20200110
   $ make && sudo make install
   $ iasl -v
   ```

## Obtaining source code
1. Create a new folder (directory) on your local development machine
   for use as your workspace. This example uses `/work/git/tianocore`, modify as
   appropriate for your needs.
   ```
   $ export WORKSPACE=/work/git/tianocore
   $ mkdir -p $WORKSPACE
   $ cd $WORKSPACE
   ```
2. Into that folder, clone:
   ```
   $ git clone --recurse-submodules https://github.com/AmpereComputing/edk2.git
   $ git clone --recurse-submodules https://github.com/AmpereComputing/edk2-platforms.git
   ```
Set up a PACKAGES_PATH to point to the locations of these three repositories:
   `$ export PACKAGES_PATH=$PWD/edk2:$PWD/edk2-platforms`

## Manual building

### Additional environment setup
   ```
   $ export CROSS_COMPILER_PATH=/opt/toolchain/ampere-8.3.0-20191025-dynamic-nosysroot/bin
   $ export PATH=${CROSS_COMPILER_PATH}:${PATH}
   $ export CROSS_COMPILE=aarch64-ampere-linux-gnu-
   $ export GCC5_AARCH64_PREFIX=${CROSS_COMPILE}
   ```

1. Set up the build environment (this will modify your environment variables)

   `$ . edk2/edksetup.sh`

   (This step _depends_ on **WORKSPACE** being set as per above.)

2. Build BaseTools

   `make -C edk2/BaseTools`

   (BaseTools can currently not be built in parallel, so do not specify any `-j`
   option, either on the command line or in a **MAKEFLAGS** environment
   variable.)

### Build options
There are a number of options that can (or must) be specified at the point of
building. Their default values are set in `edk2/Conf/target.txt`. If we are
working only on a single platform, it makes sense to just update this file.

target.txt option | command line | Description
------------------|--------------|------------
ACTIVE_PLATFORM   | `-p`         | Description file (.dsc) of platform
TARGET            | `-b`         | One of DEBUG, RELEASE or NOOPT.
TARGET_ARCH       | `-a`         | Architecture to build for. In our case, use AARCH64.
TOOL_CHAIN_TAG    | `-t`         | Toolchain profile to use for building. In our case, use GCC5.

There is also MAX_CONCURRENT_THREAD_NUMBER (`-n`), roughly equivalent to
`make -j`.

When specified on command line, `-b` can be repeated multiple times in order to
build multiple targets sequentially.

After a successful build, the resulting images can be found in
`Build/{Platform Name}/{TARGET}_{TOOL_CHAIN_TAG}/FV`.

For exampe,
`Build/Jade/RELEASE_GCC5/FV`

### Build a platform
The main build process _can_ run in parallel - so figure out how many threads we
have available.
```
$ getconf _NPROCESSORS_ONLN
8
```
Set up the build to use a little more than that:
```
$ NUM_CPUS=$((`getconf _NPROCESSORS_ONLN` + 2))
```
Now build the UEFI image:
```
$ cd edk2-platforms && build -a AARCH64 -t GCC5 -b RELEASE -D SECURE_BOOT_ENABLE -p Platform/Ampere/JadePkg/Jade.dsc
```
(Note that the description file gets resolved by the build command through
searching in all locations specified in **PACKAGES_PATH**.)

## Additional build tools
Ampere provides additional tools and documentation for automating the manual process described above, and for building a final
Tianocore UEFI image that can be flashed on the target system.

To use these tools, clone the following to the **WORKSPACE** location:
```
$ git clone https://github.com/AmpereComputing/edk2-ampere-tools.git
```
