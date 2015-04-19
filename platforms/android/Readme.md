# Android cross-compile environment #

To make it easy to do autotools and cmake builds with the android NDK, the `env-android.sh` shell script sets up a cross-compile environment, and a CMake toolchain
file `android-toolchain.cmake` is needed for CMake. This toolchain file is in the following repo, clone it:  
`https://github.com/taka-no-me/android-cmake.git`  

After obtaining the CMake toolchain, you need to edit the `env-android.sh` script and set two paths specific for your setup.
Find the section marked with the commend '===User-set variables':  
set `NDK_PATH` to the root of the Android NDK that you have installed on your system. This should look something like `/path/to/android-ndk-r10d`  
set `ANDROID_CMAKE_TOOLCHAIN` to the full path to the `android-toolchain.cmake` file inside the cmake toolchain repo you checked out.  
Then source this script in your shell:  
`source /path/to/karere/platforms/android/env-android.sh`   
It should print instructions how to use it with autotools and cmake.  
All autotools (and cmake in case the script supports install target) builds done in this environment should install in the
`$NDK_PATH/platforms/android-14/arch-arm/usr` directory inside the NDK tree.  
Because crypto++ build system is broken for android, a CMake file is provided to build it, in
`/path/to/karere/platforms/cryptopp_CMakeLists.txt`. Rename it to CMakeLists.txt and put it in the crypto++ source dir,
then build it.
