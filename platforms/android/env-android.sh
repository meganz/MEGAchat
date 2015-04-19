#!/bin/bash

#Source this file in the current shell to setup the corss-compile build environment


# Envirnoment variables:
#=== User-set variables
export NDK_PATH=/home/dbserver/android-ndk-r10d
export ANDROID_GCC_VER=4.8
export ANDROID_CMAKE_TOOLCHAIN=/home/dbserver/android-cmake/android.toolchain.cmake
#=== End of user-set variables

export TOOLCHAIN_BIN=$NDK_PATH/toolchains/arm-linux-androideabi-$ANDROID_GCC_VER/prebuilt/linux-x86_64/bin
export SYSROOT=$NDK_PATH/platforms/android-14/arch-arm
export CFLAGS=--sysroot=$SYSROOT
export CXXFLAGS=--sysroot=$SYSROOT
#Seems the crosscompiler has the C++ system include paths broken, so we need to
#give them manually with -isystem. Note that this is automatically done by CMake, so
#has to be taken care of only with autotools configure scripts
export CPPFLAGS="--sysroot=$SYSROOT -isystem /home/dbserver/android-ndk-r10d/platforms/android-14/arch-arm/usr/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/include/backward"
export LDFLAGS=--sysroot=$SYSROOT

export CC=$TOOLCHAIN_BIN/arm-linux-androideabi-gcc-$ANDROID_GCC_VER
export CXX=$TOOLCHAIN_BIN/arm-linux-androideabi-g++
export CPP=$TOOLCHAIN_BIN/arm-linux-androideabi-cpp
export LD=$TOOLCHAIN_BIN/arm-linux-androideabi-ld
export AR=$TOOLCHAIN_BIN/arm-linux-androideabi-ar
export LIBTOOL=$TOOLCHAIN_BIN/arm-linux-androideabi-libtool
export RANLIB=$TOOLCHAIN_BIN/arm-linux-androideabi-ranlib
export AS=$TOOLCHAIN_BIN/arm-linux-androideabi-as
export STRIP=$TOOLCHAIN_BIN/arm-linux-androideabi-strip

# Convenience variables
# CMake command to configure strophe build to use the android toolchain:
export CMAKE_XCOMPILE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$ANDROID_CMAKE_TOOLCHAIN -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-$ANDROID_GCC_VER -DANDROID_NDK=$NDK_PATH -DCMAKE_BUILD_TYPE=Release -DANDROID_ABI='armeabi-v7a with NEON' -DANDROID_NATIVE_API_LEVEL=14 -DCMAKE_INSTALL_PREFIX=$SYSROOT/usr"


# Typical configure command to build dependencies:
export CONFIGURE_XCOMPILE_ARGS="--host=arm-linux-gnueabi --prefix=$SYSROOT/usr"

echo "============================================"
echo "Envirnoment set to use cross-compiler at"
echo "$NDK_PATH"
echo "with sysroot"
echo "$SYSROOT"
echo
echo -e "You can use\n\e[1;31meval\e[0m configure \$CONFIGURE_XCOMPILE_ARGS [your-args]"
echo -e "to configure scripts. This also sets up the install prefix to the SYSROOT/usr directory"
echo -e "You can use \n\e[1;31meval\e[0m cmake \$CMAKE_XCOMPILE_ARGS [your-args]\nto CMake command."
echo -e "This also sets up the install prefix to the SYSROOT/usr directory"
