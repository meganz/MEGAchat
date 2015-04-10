#!/bin/bash

#Source this file in the current shell to setup the cross-compile build environment


# Envirnoment variables:
#=== User-set variables
#this is the same as the -sdk parameter to xcrun:
# 'iphoneos' for device
# 'iphonesimulator' for simulator
export IOSC_TARGET=iphoneos
export IOSC_BUILDROOT=~/ios-$IOSC_TARGET-buildroot

#=== End of user-set variables

if [ "$IOSC_TARGET" == "iphoneos" ]; then
    export IOSC_ARCH=armv7
    export IOSC_OS_VERSION=-mios-version-min=6.0

#suitable for --host= parameter to configure
    export IOSC_HOST_TRIPLET=arm-apple-darwin11
    export IOSC_PLATFORM_SDKNAME=iPhoneOS
else
    export IOSC_ARCH=x86_64
    export IOSC_OS_VERSION=
    export IOSC_HOST_TRIPLET=
    export IOSC_PLATFORM_SDKNAME=iPhoneSimiulator
fi

export IOSC_CMAKE_TOOLCHAIN="$IOSC_BUILDROOT/ios.$IOSC_TARGET.toolchain.cmake"
export IOSC_SYSROOT=`xcrun -sdk $IOSC_TARGET -show-sdk-path`

find="xcrun -sdk $IOSC_TARGET -find"
compileropts="-arch $IOSC_ARCH $IOSC_OS_VERSION -stdlib=libc++"

#export CFLAGS="-I$IOSC_BUILDROOT/usr/include"
#export CXXFLAGS="-I$IOSC_BUILDROOT/usr/include"
export LDFLAGS="--sysroot $IOSC_SYSROOT -L$IOSC_BUILDROOT/usr/lib $compileropts -lc++"
export CPPFLAGS="-isysroot$IOSC_SYSROOT -I$IOSC_BUILDROOT/usr/include $compileropts"

export CC="`$find clang`"
export CXX="`$find clang++`"
export CPP="$CC -E"
export LD="`$find clang++`"
export AR=`$find ar`
export LIBTOOL=`$find libtool`
export RANLIB=`$find ranlib`
export AS=`$find as`
export STRIP=`$find strip`

# Convenience variables
# CMake command to configure strophe build to use the android toolchain:
export CMAKE_XCOMPILE_ARGS="-DCMAKE_TOOLCHAIN_FILE=$IOSC_CMAKE_TOOLCHAIN -DCMAKE_INSTALL_PREFIX=$IOSC_BUILDROOT/usr"


# Typical configure command to build dependencies:
export CONFIGURE_XCOMPILE_ARGS="--prefix=$IOSC_BUILDROOT/usr --host=$IOSC_HOST_TRIPLET"

echo "============================================"
echo "Envirnoment set to use the following compilers:"
echo "CC=$CC"
echo "CXX=$CXX"
echo "SYSROOT(sdk root)=$IOSC_SYSROOT"
echo "BUILDROOT(user libs and headers)=$IOSC_BUILDROOT"
echo
echo -e "You can use\n\033[1;31meval\033[0m ./configure \$CONFIGURE_XCOMPILE_ARGS [your-args]"
echo -e "to configure scripts. This also sets up the install prefix to the BUILDROOT/usr directory"
echo -e "You can use \n\033[1;31meval\033[0m cmake \$CMAKE_XCOMPILE_ARGS [your-args]\nto CMake command."
echo -e "This also sets up the install prefix to the BUILDROOT/usr directory"
