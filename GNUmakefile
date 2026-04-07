export MAKE_DIR := $(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))

export SYSROOT_DIR := ${MAKE_DIR}/root
export TOOLCHAIN_DIR := ${MAKE_DIR}/hostoolchain
export PATH := "${PATH}:${MAKE_DIR}/pkg-config:$(SYSROOT_DIR)/usr/bin:$(SYSROOT_DIR)/usr/include"
export PKG_CONFIG := x86_64-horizonos-pkg-config
export PKG_CONFIG_FOR_BUILD := pkg-config

DATE := `date +"%Y-%m-%d"`
CROSSLD := $(SYSROOT_DIR)/usr/bin/x86_64-horizonos-ld
CROSSNM := $(SYSROOT_DIR)/usr/bin/x86_64-horizonos-nm
CROSSAR := $(SYSROOT_DIR)/usr/bin/x86_64-horizonos-ar
CROSSSTRIP := $(SYSROOT_DIR)/usr/bin/x86_64-horizonos-strip
HOSGCC := $(SYSROOT_DIR)/usr/bin/x86_64-horizonos-gcc
USER_CFLAGS :=

MAKE := make

override BASH_DIR := ${MAKE_DIR}/src/tasks/src/bash
override COREUTILS_DIR := ${MAKE_DIR}/src/tasks/src/coreutils
override LESS_DIR := ${MAKE_DIR}/src/tasks/src/less
override CMATRIX_DIR := ${MAKE_DIR}/src/tasks/src/cmatrix

override GNU_FLAGS := bash_cv_getcwd_malloc=yes bash_cv_func_strchrnul_works=yes bash_cv_getenv_redef=no cf_cv_wcwidth_graphics=no \
		fu_cv_sys_mounted_getmntinfo=yes ac_cv_func_splice=no ac_cv_file__usr_lib_kbd_consolefonts=no ac_cv_file__usr_share_consolefonts=no \
		ac_cv_file__usr_X11R6_lib_X11_fonts_misc=no ac_cv_file__usr_share_X11_fonts_misc=no ac_cv_file__usr_share_fonts_misc=no
override KERNEL_SRC := $(shell find src/kernel -name '*.c')
override KERNEL_OBJ := $(KERNEL_SRC:src/kernel/%.c=bin/%.o)

override ASM_SRC := $(shell find src/kernel -name '*.asm')
override ASM_OBJ := $(ASM_SRC:src/kernel/%.asm=bin/%.asm.o)

override KERNEL_ELF := ${MAKE_DIR}/bin/kernel.elf

override MLIBC_STAMP := ${MAKE_DIR}/mlibc/.built
override NCURSES_STAMP := ${MAKE_DIR}/ncurses/.built

override BASH_DL_STAMP := $(BASH_DIR)/.downloaded
override LESS_DL_STAMP := $(LESS_DIR)/.downloaded
override LESS_BUILD_STAMP := $(LESS_DIR)/.built
override COREUTILS_DL_STAMP := ${MAKE_DIR}/src/tasks/src/coreutils/.downloaded
override COREUTILS_BUILD_STAMP := ${MAKE_DIR}/src/tasks/src/coreutils/.built
override CMATRIX_DL_STAMP := $(CMATRIX_DIR)/.downloaded
override CMATRIX_BUILD_STAMP := $(CMATRIX_DIR)/.built

override LINUX_HEADERS_STAMP := ${MAKE_DIR}/linux-headers/.built

QEMU_FLAGS := -accel kvm -cpu host -debugcon file:debug/latest.log -m 1024 -drive file=horizonos.iso,index=0,media=disk,format=raw -smp 8

.PHONY: all run bios-run uefi-run debug bios-debug uefi-debug rmbin rmkernelbin clean

all: horizonos.iso

limine/limine:
	rm -rf limine
	git clone https://codeberg.org/Limine/Limine.git limine --branch=v10.x-binary --depth=1
	$(MAKE) -C limine \
		CC=gcc \
		CFLAGS="-O2 -pipe" \
		CPPFLAGS="" \
		LDFLAGS="" \
		LIBS=""
	./get-deps

$(MLIBC_STAMP): mlibc/src/* $(HOSGCC)
	cp -r mlibc/src/* mlibc/mlibc/sysdeps/horizonos/
	mkdir -p mlibc/mlibc/build

	cd mlibc/mlibc && meson \
		setup \
		--cross-file=.cross_file \
		--prefix=/usr \
		-Ddefault_library=shared \
		build --reconfigure  -Dlinux_kernel_headers="../../linux-kernel-headers/usr/include"

	cd mlibc/mlibc && DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
	cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR} -f
	touch $@

$(NCURSES_STAMP): $(HOSGCC) ncurses/ncurses-6.6/config.sub
	cd ncurses/ncurses-6.6 && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS) --disable-widec
	cd ncurses/ncurses-6.6 && $(MAKE) -j$(nproc)
	cd ncurses/ncurses-6.6 && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	touch $@

bin/%.o: src/kernel/%.c src/kernel/link.ld limine/limine
	mkdir -p $(dir $@)
	$(HOSGCC) -c $< -o $@ \
	-MMD -MP \
	-Wall -Werror -Wno-address-of-packed-member -fpie -fpic -flto=auto -Iroot/usr/include \
	-O3 -ffunction-sections -fdata-sections -mabi=sysv \
	-std=gnu11 -nostdlib -ffreestanding -masm=intel -m64 -mno-ms-bitfields -mlong-double-80 -fstack-protector-strong -march=x86-64 \
	-mno-red-zone \
	-Wno-stringop-overflow -Wno-unused-variable -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-unused-function -Wno-format-zero-length \
	-mgeneral-regs-only \
	${USER_CFLAGS} -DBUILDING_KERNEL -I limine-protocol/include
bin/%.asm.o: src/kernel/%.asm src/kernel/link.ld
	mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@
$(KERNEL_ELF): $(KERNEL_OBJ) $(ASM_OBJ)
	$(HOSGCC) -nostdlib -T src/kernel/link.ld -ffreestanding -pie -static \
	-o $@ \
	$(KERNEL_OBJ) $(ASM_OBJ) \
	-lgcc
# 	$(CROSSSTRIP) $@

run:	bios-run
debug: 	bios-debug

uefi-run:	horizonos.iso
	mkdir debug -p
	qemu-system-x86_64 $(QEMU_FLAGS) -bios /usr/share/ovmf/OVMF.fd

uefi-debug: horizonos.iso
	mkdir debug -p
	qemu-system-x86_64 $(QEMU_FLAGS) -bios /usr/share/ovmf/OVMF.fd -s -S &
	gdb -x gdb-config.txt

bios-run:	horizonos.iso
	mkdir debug -p
	qemu-system-x86_64 $(QEMU_FLAGS)

bios-debug: horizonos.iso
	mkdir debug -p
	qemu-system-x86_64 $(QEMU_FLAGS) -s -S &
	gdb -x gdb-config.txt

horizonos.iso: $(shell find src/system -type f) $(MLIBC_STAMP) $(HOSGCC) resources/pci.ids resources/* src/tasks/bin/init $(KERNEL_ELF) src/boot/limine.conf limine/limine
	rm -f $@
	rm -rf bin/initrd_contents

	mkdir -p bin/initrd_contents

	mkdir -p bin/initrd_contents/sbin
	mkdir -p bin/initrd_contents/bin
	mkdir -p bin/initrd_contents/boot
	mkdir -p bin/initrd_contents/usr/lib
	mkdir -p bin/initrd_contents/etc
	mkdir -p bin/initrd_contents/root

	cp -r src/system/* bin/initrd_contents

	cp root/usr/lib/ld.so bin/initrd_contents/usr/lib/ld.so
	cp root/usr/lib/libc.so bin/initrd_contents/usr/lib/libc.so
	cp root/usr/lib/libdl.so bin/initrd_contents/usr/lib/libdl.so

	mv src/tasks/bin/init bin/initrd_contents/sbin/init
	cp src/tasks/bin/* bin/initrd_contents/bin/ -r
	cp bin/initrd_contents/sbin/init src/tasks/bin/init

	rsync -av --exclude 'x86_64-horizonos-*' ${SYSROOT_DIR}/usr/bin/* bin/initrd_contents/bin

	rm -f bin/initrd_contents/bin/sh
	ln -s bash bin/initrd_contents/bin/sh

	mkdir -p root/boot
	cp bin/kernel.elf bin/initrd_contents/boot/kernel.elf
	cp bin/kernel.elf root/boot/kernel.elf

	cp resources/* bin/initrd_contents/boot/
	$(CROSSNM) -n --defined-only -C bin/initrd_contents/boot/kernel.elf > bin/initrd_contents/boot/symbols.txt
	git log -n 1 --pretty=format:'%H' > bin/initrd_contents/boot/commit.txt

	tar -C bin/initrd_contents -cf root/boot/initrd.tar .

	mkdir -p root/boot/limine
	cp src/boot/limine.conf limine/limine-bios.sys limine/limine-bios-cd.bin limine/limine-uefi-cd.bin root/boot/limine/

	mkdir -p root/EFI/BOOT
	cp limine/BOOTX64.EFI root/EFI/BOOT/
	cp limine/BOOTIA32.EFI root/EFI/BOOT/
	xorriso -as mkisofs -R -r -J -b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table -hfsplus \
		-apm-block-size 2048 --efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image --protective-msdos-label \
		root -o horizonos.iso
	./limine/limine bios-install horizonos.iso

src/tasks/bin/init: src/tasks/src/init/* $(LESS_BUILD_STAMP) $(COREUTILS_BUILD_STAMP) $(CMATRIX_BUILD_STAMP) root/usr/bin/bash src/tasks/bin/setkbl $(HOSGCC)
	mkdir -p src/tasks/bin
	$(HOSGCC) src/tasks/src/init/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/setkbl: src/tasks/src/setkbl/* $(HOSGCC)
	mkdir -p src/tasks/bin
	$(HOSGCC) src/tasks/src/setkbl/main.c -o $@ -O3
	$(CROSSSTRIP) $@

$(COREUTILS_BUILD_STAMP):	$(COREUTILS_DL_STAMP) $(HOSGCC)
	cd $(COREUTILS_DIR) && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS)
	cd $(COREUTILS_DIR) && $(MAKE) -j$(nproc)
	cd $(COREUTILS_DIR) && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	touch $@

$(COREUTILS_DL_STAMP):
	rm -rf $(COREUTILS_DIR)
	mkdir -p $(COREUTILS_DIR)
	git clone git://git.sv.gnu.org/coreutils $(COREUTILS_DIR)
	cd $(COREUTILS_DIR) && git checkout e644eea122462aa7fa98cbe9b8f93088074588a0
	cd $(COREUTILS_DIR) && ./bootstrap
	cp build-aux/config.sub $(COREUTILS_DIR)/gnulib/build-aux/config.sub
	patch $(COREUTILS_DIR)/src/tail.c < diffs/coreutils/tail.diff
	patch $(COREUTILS_DIR)/gnulib/lib/getlocalename_l-unsafe.c < diffs/coreutils/getlocalename.diff
	touch $@

root/usr/bin/bash: $(BASH_DL_STAMP) $(NCURSES_STAMP) $(HOSGCC)
	cd $(BASH_DIR) && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS) --without-bash-malloc --disable-nls --with-curses
	cd $(BASH_DIR) && $(MAKE) -j$(nproc)
	cd $(BASH_DIR) && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install

$(BASH_DL_STAMP):
	rm -rf $(BASH_DIR)
	mkdir -p $(BASH_DIR)
	git clone https://git.savannah.gnu.org/git/bash.git $(BASH_DIR)
	cd $(BASH_DIR) && git checkout 637f5c8696a6adc9b4519f1cd74aa78492266b7f
	cp build-aux/config.sub $(BASH_DIR)/support/config.sub
	touch $@

$(LESS_BUILD_STAMP): $(LESS_DL_STAMP) $(HOSGCC) $(NCURSES_STAMP)
	cd $(LESS_DIR) && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS)
	cd $(LESS_DIR) && $(MAKE) -j$(nproc)
	cd $(LESS_DIR) && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	touch $@

$(LESS_DL_STAMP):
	rm -rf $(LESS_DIR)
	mkdir -p $(LESS_DIR)
	git clone https://github.com/gwsw/less $(LESS_DIR)
	cd $(LESS_DIR) && git checkout 7bd865254d3520ca6f2272ca37849d0de05fd7c7
	cd $(LESS_DIR) && $(MAKE) -f Makefile.aut distfiles
	touch $@

$(CMATRIX_BUILD_STAMP): $(CMATRIX_DL_STAMP) $(HOSGCC) $(NCURSES_STAMP)
	cd $(CMATRIX_DIR) && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS)
	cd $(CMATRIX_DIR) && $(MAKE) -j$(nproc)
	cd $(CMATRIX_DIR) && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	touch $@

$(CMATRIX_DL_STAMP):
	rm -rf $(CMATRIX_DIR)
	mkdir -p $(CMATRIX_DIR)
	git clone https://github.com/abishekvashok/cmatrix $(CMATRIX_DIR)
	cd $(CMATRIX_DIR) && git checkout 5c082c64a1296859a11bee60c8c086655953a416
	cd $(CMATRIX_DIR) && autoreconf -i
	patch -d src/tasks/src/cmatrix < diffs/cmatrix/cmatrix.diff
	cp build-aux/config.sub $(CMATRIX_DIR)/config.sub
	touch $@

$(HOSGCC): $(LINUX_HEADERS_STAMP)
	./install-hos-toolchain.sh

resources/pci.ids:
	mkdir -p resources
	wget https://raw.githubusercontent.com/pciutils/pciids/refs/heads/master/pci.ids -O resources/pci.ids

ncurses/ncurses-6.6/config.sub:
	rm -rf ncurses
	mkdir -p tmp
	mkdir -p ncurses
	cd tmp && wget https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz
	tar xf "tmp/ncurses-6.6.tar.gz" -C ncurses/
	rm -rf tmp/
	cd ncurses/ncurses-6.6 && patch config.sub < ../../diffs/ncurses/ncurses.diff

$(LINUX_HEADERS_STAMP):
	rm -rf linux-headers linux-kernel-headers
	git clone https://github.com/sabotage-linux/kernel-headers linux-headers
	cd linux-headers && git checkout 22bba01
	cd linux-headers && $(MAKE) ARCH=x86_64 prefix=/usr DESTDIR=${MAKE_DIR}/linux-kernel-headers install
	touch $@

rmkernelbin:
	rm -rf bin/*
	rm -f horizonos.iso

rmbin: rmkernelbin
	rm -rf src/tasks/bin/*
	rm -rf src/libc/lib/*

clean: rmbin
	# rm -f horizonos.vdi
	rm -f resources/pci.ids
	rm -rf root
	rm -rf tmp
	rm -rf mlibc/mlibc
	rm -rf mlibc/headers-build
	rm -rf crosstoolchain
	rm -rf hostoolchain
	rm -rf debug
	rm -rf $(BASH_DIR)
	rm -rf limine

-include $(KERNEL_OBJ:.o=.d)
