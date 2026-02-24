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
sudo apt install -y build-essential bison flex libgmp3-dev libmpc-dev libmpfr-dev texinfo nasm xorriso mkbootimg util-linux dosfstools mtools qemu-system qemu-utils unzip autoconf2.69 zip meson
```

### Building

run:
```bash
make USER_CFLAGS="${options}"
```
Here's a list of the supported options:
| Option | Value   | Description |
| ------ | ------- | ----------- |
| -DLOG_LEVEL | ={TRACE, DEBUG, INFO, WARNING, ERROR, CRITICAL} | Level from which logs are written to port 0xe9 |
| -DLOG_SYSCALLS | N/A | Whether to log syscalls |
| -DLOG_MEMORY | N/A | Whether to log page allocation |

For example to build with LOG_LEVEL=TRACE and LOG_SYSCALLS:
```bash
make USER_CFLAGS="-DLOG_LEVEL=TRACE -DLOG_SYSCALLS"
```

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

## Contributing

You can submit issues [here](https://github.com/EtienneMaire37/HorizonOS-v5/issues).
Feel free to contribute and submit pull requests !

## License

HorizonOS is licensed under the GNU GPLv3 License. See the `LICENSE` file for more details.
BOOTBOOT (downloaded upon build) is licensed under the MIT license. See the `bootboot/LICENSE` file for more details.