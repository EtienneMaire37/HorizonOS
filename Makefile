MAKE_DIR:=$(strip $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST)))))
CFLAGS := -std=gnu11 -nostdlib -ffreestanding -masm=intel -m64 -mno-ms-bitfields -mlong-double-80 -fno-omit-frame-pointer -march=x86-64 # v4
DATE := `date +"%Y-%m-%d"`
CROSSLD := ./hostoolchain/usr/bin/x86_64-horizonos-ld
CROSSNM := ./hostoolchain/usr/bin/x86_64-horizonos-nm
CROSSAR := ./hostoolchain/usr/bin/x86_64-horizonos-ar
CROSSSTRIP := ./hostoolchain/usr/bin/x86_64-horizonos-strip
HOSGCC := ./hostoolchain/usr/bin/x86_64-horizonos-gcc
USER_CFLAGS := 
MKBOOTIMG := ./bootboot/mkbootimg/mkbootimg
SYSROOT_DIR := ${MAKE_DIR}/root
TOOLCHAIN_DIR := ${MAKE_DIR}/hostoolchain

KERNEL_SRC := $(shell find src/kernel -name '*.c')
KERNEL_OBJ := $(KERNEL_SRC:src/kernel/%.c=bin/%.o)

ASM_SRC := $(shell find src/kernel -name '*.asm')
ASM_OBJ := $(ASM_SRC:src/kernel/%.asm=bin/%.asm.o)

KERNEL_ELF := bin/kernel.elf

MLIBC_STAMP := mlibc/.built

.PHONY: all run rmbin clean

all: horizonos.iso

mlibc: $(MLIBC_STAMP)

$(MLIBC_STAMP): mlibc/src/* $(HOSGCC) Makefile
	cp -r mlibc/src/* mlibc/mlibc/sysdeps/horizonos/
	cd mlibc/mlibc && PATH="${SYSROOT_DIR}/usr/bin:${PATH}" DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
	cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}
	touch $@

bin/%.o: src/kernel/%.c Makefile
	mkdir -p $(dir $@)
	$(HOSGCC) -c $< -o $@ \
	-MMD -MP \
	-Wall -Werror -Wno-address-of-packed-member -fpic -I./bootboot/dist -I./root/usr/include \
	-O2 \
	$(CFLAGS) \
	-mno-red-zone \
	-Wno-stringop-overflow -Wno-unused-variable -Wno-unused-but-set-variable -Wno-maybe-uninitialized -Wno-unused-function -Wno-format-zero-length \
	-mno-80387 -mno-mmx -mno-sse -mno-avx \
	${USER_CFLAGS} -DBUILDING_KERNEL
bin/%.asm.o: src/kernel/%.asm Makefile
	mkdir -p $(dir $@)
	nasm -f elf64 $< -o $@
$(KERNEL_ELF): $(KERNEL_OBJ) $(ASM_OBJ) Makefile
	$(HOSGCC) -nostdlib -n -T src/kernel/link.ld -ffreestanding -flto \
	-o $@ \
	$(KERNEL_OBJ) $(ASM_OBJ) \
	-lgcc

run:	all
	mkdir debug -p
	qemu-system-x86_64                           		\
	-accel kvm 											\
	-cpu host											\
	-debugcon file:debug/latest.log						\
	-m 64                                        		\
	-drive file=horizonos.iso,index=0,media=disk,format=raw \
	-smp 8

horizonos.iso: $(HOSGCC) $(MKBOOTIMG) resources/pci.ids src/tasks/bin/init $(KERNEL_ELF)
	mkdir -p ./bin/initrd_contents

	cp src/tasks/bin/ ./bin/initrd_contents/ -r

	cp resources/* ./bin/initrd_contents/
	$(CROSSNM) -n --defined-only -C bin/kernel.elf > ./bin/initrd_contents/symbols.txt
	git log -n 1 --pretty=format:'%H' > ./bin/initrd_contents/commit.txt

	mkdir -p ./root
	
	cp ./bin/kernel.elf ./bin/initrd_contents/kernel.elf

	rm -f bin/horizonos.bin

# 	PATH="/usr/sbin:${PATH}" $(DIR2FAT32) bin/horizonos.bin 2048 ./root
	dd if=/dev/zero of=bin/horizonos.bin bs=1M count=1
	
	$(MKBOOTIMG) src/kernel/bootboot.json horizonos.iso

# 	qemu-img convert -O vdi horizonos.iso horizonos.vdi

src/tasks/bin/init: src/tasks/src/init/* src/tasks/bin/term src/tasks/bin/echo src/tasks/bin/ls src/tasks/bin/cat src/tasks/bin/clear src/tasks/bin/printenv $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/init/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/echo: src/tasks/src/echo/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/echo/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/ls: src/tasks/src/ls/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/ls/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/cat: src/tasks/src/cat/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/cat/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/clear: src/tasks/src/clear/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/clear/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/printenv: src/tasks/src/printenv/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/printenv/main.c -o $@ -O3
	$(CROSSSTRIP) $@

src/tasks/bin/term: src/tasks/src/term/* $(MLIBC_STAMP) $(HOSGCC) Makefile
	mkdir -p ./src/tasks/bin
	$(HOSGCC) ./src/tasks/src/term/main.c -o $@ -O3
	$(CROSSSTRIP) $@

$(HOSGCC):
	sh install-hos-toolchain.sh

$(MKBOOTIMG):
	git clone https://gitlab.com/bztsrc/bootboot.git bootboot
	cd bootboot/mkbootimg && make

resources/pci.ids:
	mkdir -p resources
	wget https://raw.githubusercontent.com/pciutils/pciids/refs/heads/master/pci.ids -O ./resources/pci.ids

rmbin:
	rm -rf ./bin/*
	rm -rf ./src/tasks/bin/*
	rm -rf ./src/libc/lib/*
	rm -rf ./initrd.tar

clean: rmbin
	rm -f ./horizonos.iso
	# rm -f ./horizonos.vdi
	rm -f ./resources/pci.ids
	rm -rf ./bootboot
	rm -rf ./root
	rm -rf ./tmp
	rm -rf ./mlibc/mlibc
	rm -rf ./mlibc/headers-build
	rm -rf ./crosstoolchain
	rm -rf ./hostoolchain
	rm -rf ./debug

-include $(KERNEL_OBJ:.o=.d)
