export PREFIX="$(pwd)/crossgcc"
export TARGET=x86_64-elf
export PATH="$PREFIX/bin:$PATH"

set -x -e

mkdir -p "$PREFIX"

mkdir -p ./tmp
cd ./tmp

wget https://ftp.gnu.org/gnu/binutils/binutils-2.44.tar.gz
tar xf binutils-2.44.tar.gz

wget https://ftp.gnu.org/gnu/gcc/gcc-15.2.0/gcc-15.1.0.tar.gz
tar xf gcc-15.1.0.tar.gz

cd binutils-2.44
mkdir build

cd build
../configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make
make install

cd ../..

cd gcc-15.1.0
mkdir build
cd build 
../configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers --disable-hosted-libstdcxx
make all-gcc
make all-target-libgcc
make all-target-libstdc++-v3
make install-gcc
make install-target-libgcc
make install-target-libstdc++-v3

echo "Installed binaries:"
ls "$PREFIX/bin" -l