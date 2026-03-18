#!/bin/sh

set -x -e

mkdir -p ${SYSROOT_DIR}

rm -rf ./mlibc/mlibc
cd mlibc
git clone https://github.com/managarm/mlibc mlibc
cd mlibc
git checkout ccc93dd
mkdir -p mlibc/sysdeps/horizonos
git apply -p2 ../../diffs/mlibc/mlibc.diff
cp -r ../src/* sysdeps/horizonos/
mkdir -p sysdeps/horizonos/include/abi-bits sysdeps/horizonos/include/bits
cp -r sysdeps/linux/include/abi-bits/* sysdeps/horizonos/include/abi-bits
cp -r sysdeps/linux/include/bits/* sysdeps/horizonos/include/bits

meson \
    setup \
    --cross-file=../cross_file \
    --prefix=/usr \
    -Dheaders_only=true \
    headers-build  -Dlinux_kernel_headers="../../linux-kernel-headers/usr/include"

DESTDIR=${TOOLCHAIN_DIR} ninja -C headers-build install

cd ../..

cp -r ${MAKE_DIR}/linux-kernel-headers/usr/include/* ${TOOLCHAIN_DIR}/usr/include
cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}

rm -rf ./tmp
mkdir -p ./tmp
cd ./tmp

wget ftp://ftp.gnu.org/gnu/binutils/binutils-2.45.1.tar.gz
tar xf binutils-2.45.1.tar.gz

wget ftp://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.2.0.tar.gz
tar xf gcc-15.2.0.tar.gz

cd binutils-2.45.1

patch -p1 < ../../diffs/binutils-2.45.1_gcc-15.2.0/binutils.diff

mkdir build
cd build

../configure \
    --target=x86_64-horizonos \
    --prefix=/usr \
    --with-sysroot="${SYSROOT_DIR}" \
    --disable-werror \
    --enable-default-execstack=no

make -j$(nproc)

DESTDIR="${TOOLCHAIN_DIR}" make -j$(nproc) install

cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}

cd ../../gcc-15.2.0

patch -p1 < ../../diffs/binutils-2.45.1_gcc-15.2.0/gcc.diff
./contrib/download_prerequisites

cd libstdc++-v3
autoconf2.69
cd ..

cd ..

mkdir gcc-15.2.0-build
cd gcc-15.2.0-build

../gcc-15.2.0/configure \
    --target=x86_64-horizonos \
    --prefix=/usr \
    --with-sysroot="${SYSROOT_DIR}" \
    --enable-languages=c,c++ \
    --enable-threads=posix \
    --disable-multilib \
    --enable-shared \
    --enable-host-shared

make -j$(nproc) all-gcc all-target-libgcc
DESTDIR="${TOOLCHAIN_DIR}" make -j$(nproc) install-gcc install-target-libgcc
cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}

cd ../..

rm -rf ./tmp

cd mlibc/mlibc

meson \
    setup \
    --cross-file=../cross_file \
    --prefix=/usr \
    -Ddefault_library=static \
    build -Dlinux_kernel_headers="../../linux-kernel-headers/usr/include"

DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}

meson \
    setup \
    --cross-file=../cross_file \
    --prefix=/usr \
    -Ddefault_library=shared \
    build --reconfigure -Dlinux_kernel_headers="../../linux-kernel-headers/usr/include"

DESTDIR=${TOOLCHAIN_DIR} ninja -C build install
cp -r ${TOOLCHAIN_DIR}/* ${SYSROOT_DIR}