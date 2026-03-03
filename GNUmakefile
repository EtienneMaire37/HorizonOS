MAKE_DIR:=$(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))

export SYSROOT_DIR := ${MAKE_DIR}/root
export TOOLCHAIN_DIR := ${MAKE_DIR}/hostoolchain
export PATH := "$(MAKE_DIR)/pkg-config:$(SYSROOT_DIR)/usr/bin:$(SYSROOT_DIR)/usr/include:${PATH}"
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

SH_DIR := ${MAKE_DIR}/src/tasks/src/bash
GNU_FLAGS := bash_cv_getcwd_malloc=yes bash_cv_func_strchrnul_works=yes bash_cv_getenv_redef=no cf_cv_wcwidth_graphics=no

override KERNEL_SRC := $(shell find src/kernel -name '*.c')
override KERNEL_OBJ := $(KERNEL_SRC:src/kernel/%.c=bin/%.o)

override ASM_SRC := $(shell find src/kernel -name '*.asm')
override ASM_OBJ := $(ASM_SRC:src/kernel/%.asm=bin/%.asm.o)

override KERNEL_ELF := bin/kernel.elf

override MLIBC_STAMP := mlibc/.built
override NCURSES_STAMP := ncurses/.built

override BASH_DL_STAMP := src/tasks/src/bash/.downloaded

QEMU_FLAGS := -accel kvm -cpu host -debugcon file:debug/latest.log -m 256 -drive file=horizonos.iso,index=0,media=disk,format=raw -smp 8

.PHONY: all run rmbin clean

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

mlibc: $(MLIBC_STAMP)

$(MLIBC_STAMP): mlibc/src/* $(HOSGCC) GNUmakefile
	cp -r mlibc/src/* mlibc/mlibc/sysdeps/horizonos/
	mkdir -p mlibc/mlibc/build
	cd mlibc/mlibc && meson \
		setup \
		--cross-file=.cross_file \
		--prefix=/usr \
		-Ddefault_library=static \
		build --reconfigure

	cd mlibc/mlibc && DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
	cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR} -f

	cd mlibc/mlibc && meson \
		setup \
		--cross-file=.cross_file \
		--prefix=/usr \
		-Ddefault_library=shared \
		build --reconfigure

	cd mlibc/mlibc && DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
	cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR} -f
	touch $@

$(NCURSES_STAMP): $(MLIBC_STAMP) ncurses/ncurses-6.6/config.sub
	cd ncurses/ncurses-6.6 && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS) --disable-widec --with-libtool-opts=-static
	cd ncurses/ncurses-6.6 && $(MAKE) -j$(nproc)
	cd ncurses/ncurses-6.6 && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	touch $@

bin/%.o: src/kernel/%.c GNUmakefile src/kernel/link.ld limine/limine
	mkdir -p $(dir $@)
	$(HOSGCC) -c $< -o $@ \
	-MMD -MP \
	-Wall -Werror -Wno-address-of-packed-member -fpie -fpic -fno-lto -Iroot/usr/include \
	-O2 -ffunction-sections -fdata-sections -mabi=sysv \
	-std=gnu11 -nostdlib -ffreestanding -masm=intel -m64 -mno-ms-bitfields -mlong-double-80 -fno-omit-frame-pointer -fstack-protector-strong -march=x86-64 \
	-mno-red-zone \
	-Wno-stringop-overflow -Wno-unused-variable -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-unused-function -Wno-format-zero-length \
	-mgeneral-regs-only \
	${USER_CFLAGS} -DBUILDING_KERNEL -I limine-protocol/include
bin/%.asm.o: src/kernel/%.asm GNUmakefile src/kernel/link.ld
	mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@
$(KERNEL_ELF): $(KERNEL_OBJ) $(ASM_OBJ) GNUmakefile
	$(HOSGCC) -nostdlib -T src/kernel/link.ld -ffreestanding -pie -static \
	-o $@ \
	$(KERNEL_OBJ) $(ASM_OBJ) \
	-lgcc
# 	$(CROSSSTRIP) $@

run:	uefi-run
debug: 	uefi-debug

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

horizonos.iso: $(HOSGCC) resources/pci.ids src/tasks/bin/init $(KERNEL_ELF) limine/limine
	rm -f $@

	mkdir -p bin/initrd_contents

	mkdir -p bin/initrd_contents/sbin
	mkdir -p bin/initrd_contents/bin
	mkdir -p bin/initrd_contents/boot
	mkdir -p bin/initrd_contents/usr/lib
	mkdir -p bin/initrd_contents/etc
	mkdir -p bin/initrd_contents/root

	echo "root:x:0:0:root:/root:/bin/bash" > bin/initrd_contents/etc/passwd

	cp root/usr/lib/ld.so bin/initrd_contents/usr/lib/ld.so
	cp root/usr/lib/libc.so bin/initrd_contents/usr/lib/libc.so

	mv src/tasks/bin/init bin/initrd_contents/sbin/init
	cp src/tasks/bin/* bin/initrd_contents/bin/ -r
	cp bin/initrd_contents/sbin/init src/tasks/bin/init

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

src/tasks/bin/init: src/tasks/src/init/* src/tasks/bin/bash src/tasks/bin/setkbl $(MLIBC_STAMP) $(HOSGCC) GNUmakefile
	mkdir -p src/tasks/bin
	$(HOSGCC) src/tasks/src/init/main.c -o $@ -O3 -static
	$(CROSSSTRIP) $@

src/tasks/bin/bash: $(BASH_DL_STAMP) $(MLIBC_STAMP) $(NCURSES_STAMP) $(HOSGCC)
	cd src/tasks/src/bash && CC=x86_64-horizonos-gcc CC_FOR_BUILD=gcc ./configure --host=x86_64-horizonos --prefix=/usr $(GNU_FLAGS) --enable-static-link --without-bash-malloc --disable-nls --with-curses
	cd src/tasks/src/bash && $(MAKE) -j$(nproc)
	cd src/tasks/src/bash && $(MAKE) DESTDIR=${SYSROOT_DIR} -j$(nproc) install
	cp ${SYSROOT_DIR}/usr/bin/bash src/tasks/bin/bash

src/tasks/bin/setkbl: src/tasks/src/setkbl/* $(MLIBC_STAMP) $(HOSGCC) GNUmakefile
	mkdir -p src/tasks/bin
	$(HOSGCC) src/tasks/src/setkbl/main.c -o $@ -O3 -static
	$(CROSSSTRIP) $@

$(HOSGCC):
	install-hos-toolchain.sh

resources/pci.ids:
	mkdir -p resources
	wget https://raw.githubusercontent.com/pciutils/pciids/refs/heads/master/pci.ids -O resources/pci.ids

$(BASH_DL_STAMP):
	rm -rf $(SH_DIR)
	git clone https://git.savannah.gnu.org/git/bash.git $(SH_DIR)
	git -C $(SH_DIR) apply $(MAKE_DIR)/diffs/bash/bash.diff
	touch $@

ncurses/ncurses-6.6/config.sub:
	rm -rf ncurses
	mkdir -p tmp
	mkdir -p ncurses
	cd tmp && wget https://ftp.gnu.org/gnu/ncurses/ncurses-6.6.tar.gz
	tar xf "tmp/ncurses-6.6.tar.gz" -C ncurses/
	rm -rf tmp/
	cd ncurses/ncurses-6.6 && patch config.sub < ..diffs/ncurses/ncurses.diff

rmbin:
	rm -rf bin/*
	rm -rf src/tasks/bin/*
	rm -rf src/libc/lib/*
	rm -rf initrd.tar
	rm -f horizonos.iso

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
	rm -rf src/tasks/src/bash
	rm -rf limine

-include $(KERNEL_OBJ:.o=.d)
