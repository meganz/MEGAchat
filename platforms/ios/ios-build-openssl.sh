#!/bin/bash
set -e
export CROSS_TOP="`xcode-select --print-path`/Platforms/$IOSC_PLATFORM_SDKNAME.platform/Developer"
export CROSS_SDK="$IOSC_PLATFORM_SDKNAME.sdk"

./Configure iphoneos-cross shared --openssldir=$IOSC_BUILDROOT/usr
sed -ie "s/^\(CC=.*\)$/\\1 -arch armv7/" ./Makefile
sed -ie "s/^CFLAG=/CFLAG=-mios-version-min=6\.0/" ./Makefile

make -j16
make install
