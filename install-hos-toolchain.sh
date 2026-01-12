export SYSROOT_DIR="$(pwd)/root"
export TOOLCHAIN_DIR="$(pwd)/hosgcc"
export PATH="${TOOLCHAIN_DIR}/usr/bin:$PATH"

set -x -e

mkdir -p ./tmp
cd ./tmp

wget https://ftpmirror.gnu.org/gnu/binutils/binutils-2.45.1.tar.gz
tar xf binutils-2.45.1.tar.gz

wget https://ftpmirror.gnu.org/gcc/gcc-15.2.0/gcc-15.2.0.tar.gz
tar xf gcc-15.2.0.tar.gz

cd binutils-2.45.1
mkdir build
cd build

../configure \
    --target=x86_64-horizonos \
    --prefix=/usr \
    --with-sysroot="${SYSROOT_DIR}" \
    --disable-werror \
    --enable-default-execstack=no

make -j$(nproc)

DESTDIR="${TOOLCHAIN_DIR}" make install