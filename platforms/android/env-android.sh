#!/bin/bash

#Source this file in the current shell to setup the corss-compile build environment


# Envirnoment variables:

#=== User-set variables

export TOOLCHAIN_PATH=/home/loki/android-dev/toolchain
export ANDROID_CMAKE_TOOLCHAIN=/home/loki/android-dev/android-cmake/android.toolchain.cmake

#=== End of user-set variables

function exportTool
{
   local path="$TOOLCHAIN_PATH/bin/arm-linux-androideabi-$2"
   export $1="$path"
   if [ ! -f "$path" ]; then
      echo -e "\e[1;31mTool $1 does not exist at path '$path'\e[1;0m"
      return
   fi
}

if [ -z "$TOOLCHAIN_TYPE" ]; then
  if [ -f "$TOOLCHAIN_PATH/bin/clang" ]; then
     TOOLCHAIN_TYPE="clang"
  else
     TOOLCHAIN_TYPE="gcc"
  fi
fi

if [[ "$TOOLCHAIN_TYPE" == "clang" ]]; then
  export ANDROID_CC_NAME=clang
  export ANDROID_CXX_NAME=clang++
else
  export ANDROID_CC_NAME=gcc
  export ANDROID_CXX_NAME=g++
fi
export TOOLCHAIN_BIN=$TOOLCHAIN_PATH/bin
export SYSROOT=$TOOLCHAIN_PATH/sysroot
export CFLAGS=--sysroot=$SYSROOT
export CXXFLAGS=--sysroot=$SYSROOT

#Seems the crosscompiler has the C++ system include paths broken, so we need to
#give them manually with -isystem. Note that this is automatically done by CMake, so
#has to be taken care of only with autotools configure scripts
#export CPPFLAGS="--sysroot=$SYSROOT -isystem /home/dbserver/android-ndk-r10d/platforms/android-14/arch-arm/usr/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/libs/armeabi-v7a/include -isystem /home/dbserver/android-ndk-r10d/sources/cxx-stl/gnu-libstdc++/4.8/include/backward"
#export LDFLAGS=--sysroot=$SYSROOT

exportTool CC "$ANDROID_CC_NAME"
exportTool CXX "$ANDROID_CXX_NAME"
exportTool CPP "cpp"
exportTool LD "ld"
exportTool AR "ar"
exportTool LIBTOOL "libtool"
exportTool RANLIB "ranlib"
exportTool AS "as"
exportTool STRIP "strip"

# Convenience variables
# CMake command to configure strophe build to use the android toolchain:
export CMAKE_XCOMPILE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$ANDROID_CMAKE_TOOLCHAIN \
  -DANDROID_STANDALONE_TOOLCHAIN=$TOOLCHAIN_PATH \
  -DANDROID_TOOLCHAIN_NAME="standalone-$TOOLCHAIN_TYPE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DANDROID_ABI='armeabi-v7a with NEON' -DANDROID_NATIVE_API_LEVEL=21 \ 
  -DCMAKE_INSTALL_PREFIX=$SYSROOT/usr"

#  -DANDROID_TOOLCHAIN_NAME=arm-linux-androideabi-$ANDROID_COMPILER_NAME \ 

# Typical configure command to build dependencies:
export CONFIGURE_XCOMPILE_ARGS="--host=arm-linux-gnueabi --prefix=$SYSROOT/usr"

function xcmake
{
  eval cmake $CMAKE_XCOMPILE_ARGS $@
}

function xconfigure
{
  eval ./configure $CONFIGURE_XCOMPILE_ARGS $@
}

echo "============================================"
echo "Envirnoment set to use cross-compiler toolchain at"
echo "  $TOOLCHAIN_PATH"
echo "Toolchain type: $TOOLCHAIN_TYPE"
echo "sysroot: $SYSROOT"
echo "C Compiler: $CC"
echo "C++ Compiler: $CXX"
echo
echo -e "You can use\n\e[1;30meval\e[0m configure \$CONFIGURE_XCOMPILE_ARGS [your-args]"
echo -e "to configure scripts. This also sets up the install prefix to the SYSROOT/usr directory"
echo -e "You can use \n\e[1;30meval\e[0m cmake \$CMAKE_XCOMPILE_ARGS [your-args]\nto CMake command."
echo -e "This also sets up the install prefix to the SYSROOT/usr directory"
