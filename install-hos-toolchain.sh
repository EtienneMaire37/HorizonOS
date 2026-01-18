export SYSROOT_DIR="$(pwd)/root"
export TOOLCHAIN_DIR="$(pwd)/hostoolchain"
export PATH="${TOOLCHAIN_DIR}/usr/bin:$PATH"

set -x -e

rm -rf ./mlibc/mlibc
rm -rf ./mlibc/headers-build
cd mlibc
git clone https://github.com/managarm/mlibc mlibc
cd mlibc
git checkout ccc93dd
git apply -p2 ../../diffs/mlibc/mlibc.diff

meson \
    setup \
    --cross-file=../cross_file \
    --prefix=/usr \
    -Dheaders_only=true \
    ../headers-build

cd ..

DESTDIR=${TOOLCHAIN_DIR} ninja -C headers-build install

cd ..

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
    --with-sysroot="${TOOLCHAIN_DIR}" \
    --disable-werror \
    --enable-default-execstack=no

make -j$(nproc)

DESTDIR="${TOOLCHAIN_DIR}" make install

cd ../../gcc-15.2.0

patch -p1 < ../../diffs/binutils-2.45.1_gcc-15.2.0/gcc.diff
./contrib/download_prerequisites

cd ..

mkdir gcc-15.2.0-build
cd gcc-15.2.0-build

export PATH="${TOOLCHAIN_DIR}/usr/bin:$PATH"

../gcc-15.2.0/configure \
    --target=x86_64-horizonos \
    --prefix=/usr \
    --with-sysroot="${TOOLCHAIN_DIR}" \
    --enable-languages=c,c++ \
    --enable-threads=posix \
    --disable-multilib \
    --enable-shared \
    --enable-host-shared

make -j$(nproc) all-gcc all-target-libgcc
DESTDIR="${TOOLCHAIN_DIR}" make install-gcc install-target-libgcc

cd ../..

rm -rf ./tmp