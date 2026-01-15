# HorizonOS

<div align="center">
   
   ![GPL License](https://img.shields.io/badge/license-GPL-yellow.svg) 
   ![x86-64](https://img.shields.io/badge/arch-x86_64-informational) 
   ![GitHub Contributors](https://img.shields.io/github/contributors/EtienneMaire37/HorizonOS-v5?color=blue)
   ![Monthly Commits](https://img.shields.io/github/commit-activity/m/EtienneMaire37/HorizonOS-v5?color=orange)
   ![GitHub Actions Workflow Status](https://img.shields.io/github/actions/workflow/status/EtienneMaire37/HorizonOS-v5/.github%2Fworkflows%2Fmakefile.yml)
</div>

HorizonOS is a hobby 64-bit monolithic kernel for the x86-64 architecture. It aims at simplicity and readability.

## Building HorizonOS

These instructions assume a Debian-like environment. Feel free to adapt those instructions to other platforms.

### Prerequisites

Install dependencies:
```bash
sudo apt-get install -y build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo
sudo apt install -y nasm xorriso mtools mkbootimg util-linux dosfstools mtools qemu-system qemu-utils unzip autoconf2.69 zip meson
```

### Building

Make sure /usr/sbin is in your PATH (to use disk utilities):
```bash
export PATH="/usr/sbin:${PATH}"
```
Then simply run: 
```bash
make all
```
To build without logs. 
Or:
```bash
make all USER_CFLAGS=-DLOG_LEVEL=INFO
```
To build with E9 port logs. 
A `horizonos.iso` disk image file will be created in the root of the repository.

### Running HorizonOS

To run HorizonOS in QEMU:
```bash
make run
```

## Third-Party Code

HorizonOS uses the following third-party libraries and resources:

- [liballoc](https://github.com/blanham/liballoc) - For libc memory allocation (Public domain)
- [BOOTBOOT](https://gitlab.com/bztsrc/bootboot) - A UEFI bootloader (MIT license)
- [pci.ids](https://raw.githubusercontent.com/pciutils/pciids/refs/heads/master/pci.ids) - List of PCI IDs (GPLv3)

## Memory map

| Range     | Mapping         |
| --------- | --------------- |
| 0-128TB | Process segments, heap and stack    |
| 128TB-[-64TB] | Noncanonical addresses |
| [-64TB]-[-63TB] | Mapped to physical 0-1TB |
| [-63TB]-[-512GB] | Unused addresses |
| [-512GB]-[-128MB] | Hole |
| [-128MB]-0 | mmio, framebuffer, bootboot data and kernel code segment |

## Contributing

You can submit issues [here](https://github.com/EtienneMaire37/HorizonOS-v5/issues).
Feel free to contribute and submit pull requests !

## License

HorizonOS is licensed under the GNU GPLv3 License. See the `LICENSE` file for more details.
BOOTBOOT (downloaded upon build) is licensed under the MIT license. See the `bootboot/LICENSE` file for more details.