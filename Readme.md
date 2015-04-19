#  Building karere-native #
Checkout the `karere-native` git repository to a dir chosen by you.
## Toolchains ##
* Android  

Building of webrtc is supported only on Linux.This is a limitation of Google's webrtc/chromium build system.  
Install the android NDK. Although webrtc comes with its own copy of the NDK, we are going to use the standard one for building
the karere-native code, and let webrtc build with its own NDK. Both compilers are gcc 4.8 so they are binary compatible.  
Forcing webrtc to build with an external NDK will not work. For some operations, like assembly code transformations, a host
compiler is used, which is the clang version that comes with webrtc. To use an external NDK, we need to specify explicitly
specify the `--sysroot` path of the external NDK, which also gets passed to the clang host compiler, causing errors. 
* iOS  

Building of webrtc is supported only on MacOS. You need to install XCode.  

## Dependencies ##

 - `cmake` (and ccmake if a config GUI is required)  
 - Our version of strophe (https://code.developers.mega.co.nz/messenger/strophe-native, see readme for it's own dependencies).
 No need to explicitly build it, will be done by the Karere build system.  
 - `libevent2` - at least 2.0.x  
 - `openssl` - needed by the SDK, webrtc, strophe and Karere itself.
 - Native WebRTC stack from Chrome, see below for build instructions for it.  
 - The Mega SDK. Check out the repository, configure the SDK in a minimalistic way - without image libs etc.
 Only the crypto and HTTP functionality is needed. Install crypto++ and libcurl globally in the system, because the Karere
 build will need to find them as well and link to them directly. Install any other mandatory Mega SDK dependencies
 and build the SDK. It does not have to be installed with `make install`, it will be accessed directly in the checkout dir.  
 - Desktop OS-es  
     * Qt4 (for the test app): `libqtcore4 libqtgui4 libqt4-dev`
 - mpEnc. Check out the repository https://code.developers.mega.co.nz/messenger/mpenc_cpp, install `libsodium`.

* Android and iOS  
You need to set up a cross-compile environment that can works with both aututools amd cmake. Look at the instructions
in `platforms/android|ios/Readme.md`. Using these instructions, build and install the karere-native dependencies for Android/iOS.
Using this environment, build all third-party dependencies listed above. For crypto++, its makefile is broken for
cross-compilation, that's why a cmake build script is prtovided for it in `platforms/cryptopp_CMakeLists.txt`.
Use it for building crypto++.

* iOS  
IMPORTANT: As Apple does not allow dynamic libraries in iOS apps, you must build all third-party dependencies as static libs. Usually
they default to dynamic libs, so you must take care of that explicitly.

* MacOS  
Because MacOS has a built-in version of openssl, which is not compatible for some reason (causes Strophe login to stall),
we have to install a generic version of openssl via Homebrew or mac ports. To make sure the webrtc build does not pick the
system openssl headers, you can rename the /usr/include/openssl dir and the corresponding dir(s) in the XCode SDKs,
temporarily until you build webrtc. These header dirs are needed only for development, correspond to old versions of openssl
0.9.7 or 0.9.8, and the headers generate tons of depracation warnings, so you may want to consider keeping them renamed
and linking against an up to date version of openssl when building software.   

## Building webrtc ##

* Android  
Start a fresh new shell for building webrtc. You must NOT use a shell where `env-android.sh` has been sourced, because
that script sets the CC, CXX etc variables to the NDK compiler that you installed. However we don't want to build webrtc with
that compiler, but rather with its own version (reasons explained above). Therefore, you must not use here the shell that you
used to build the dependencies.However it will be used later to build the karere codebase.  
* iOS  
Start a fresh new shell for bulding webrtc.  You must NOT use a shell where `env-ios.sh` has been sourced.  

First, create a directory where all webrtc stuff will go, and `cd` to it. All instructions in this section assume that the
current directory is that one.  

### Install depot_tools ###
We need to install Google's `depot_tools`, as per these instructions:  
https://sites.google.com/a/chromium.org/dev/developers/how-tos/install-depot-tools  

Make sure the path to the `depot_tools` dir is at the start of the system path, because the tools provide custom versions
of commands that may already be available on the system, and the custom ones must be picked instead of the system ones.

### Checkout the code ###
For code checkout and configuration the tool `gclient` from depot tools is used, as it needs to checkout 100s of repositories
 and run a lot of config scripts. What is more specific here is that we need to checkout a specific revision of the source
 tree, as there are a lot of changes in the webRTC code and the most recent code will not work. Another reason not to use the
 most recent version is that Google changed the build process to require the whole Chromium source tree, which is about
 10GB of code, versus the older versions that require only a fraction of that. First, we need to tell gclient with what
 repository to work:  
`gclient config http://webrtc.googlecode.com/svn/trunk`  

* For Android, do:  
`echo "target_os = ['android', 'unix']" >> .gclient`  

* For MacOS, do:  
`echo "target_os = ['mac']" >> .gclient`  

This creates a .gclient file in the current dir. Next, checkout the code:  
`gclient sync --revision r6937 --force`  
This may take a long time.  
Then  
`cd trunk`

### Install dependencies ###
* Linux  
There are various packages required by the webrtc build, most of them are checked out by gclient, but there are
some that need to be installed on the system. To do that on linux, you can run:  
`build/install-build-deps.sh`  
Also you need Java JDK 6 or 7 (for something related to android, but seems to be run unconditionally):  
If you don't have JDK installed, install `openjdk-7-jdk`. Export `JAVA_HOME` to point to your JDK installation, on Ubuntu
is something like that:  
`export JAVA_HOME=/usr/lib/jvm/java-7-openjdk`   

* Mac  and iOS 
No Java is or any other dependencies are needed on these platforms.  

* Android  
JDK 7 will not work for this particular revision (some warnings are triggered and the build is
configured to treat warnings as errors). Therefore you need to install JDK 6 (unless already done).  
Export `JAVA_HOME` to point to the JDK installation:  
`export JAVA_HOME=/usr/lib/jvm/java-6-openjdk`   
Install dependencies by running  
`build/install-build-deps-android.sh`  
However this script seems to install far too many unnecessary packages such as apache2 or firefox localization packs.
So you may want to try to manually install dependencies, especially if you have done a native platform build before
on this machine, in which case you may already have most dependencies.

### Replace the openssl version ###

WebRTC supports two backends for ssl - `openssl` and `nss`, and it ships the source code of both. On all platfrorms except
for Android, it builds with `nss` as its ssl backend. Tha bad thing is that nss and openssl cannot be both included in one
application, because they have internal symbols with the same names which get mixed up resulting in e.g. openssl calling into nss
resulting in spectacular segfaults. On the other hand, we do not support nss for the rest of the codebase (SDK, strophe),
and of the two backends supported by webrtc we can use only openssl. Therefore, we must force webrtc to build with openssl on all
platforms. Then we hit another problem - webrtc wants to build with Google's own version of openssl, called boringssl
(shipped within the webrtc source tree). It is not binary compatible with the standard openssl and this will result again in
segfaults. So we need to force webrtc to build against the 'system' openssl, that will be used for the rest of the code as well.
To do that, we need to replace the .gyp file responsible for building and linking webrtc against the boringssl lib.
First, just in case, we will hide all boringssl stuff from the build system and create our own fake dir with the false .gyp file.
In order to do that:  
`mv third_party/boringssl third_party/boringssl_hide`  
`mkdir third_party/boringssl`  
`cp /path/to/karere/webrtc-build/boringssl.gyp ./third_party/boringssl/`

This file maps boringssl references to the openssl installed in your system/sysroot/buildroot.
For this to work, you need to have the `DEPS_SYSROOT` env variable set to the prefix where openssl is installed.
When not cross-compiling, this is usually `/usr` or `/usr/local`, and when cross-compiling, this is normally
the sysroot/buildroot directory where all depenencies are built.
Note that on cross-build environments you cannot assign this variable to a variable set by the cross-compile environment
env-xxx.sh script, because that script must not be sourced in this shell.
* Android  
This path would be:  
`export DEPS_SYSROOT="<path-to-android-ndk-you-installed>/platforms/android-14/arch-arm/usr"`  
* iOS  
This path would usually be:
`export DEPS_SYSROOT="/path/to/ios-iphoneos-buildroot/usr"`

Then, on non-Android platforms, we must tell webrtc that we want it to build with openssl. For that, we provide
`use_openssl=1` and `use_nss=0` to the GYP_DEFINES. This is reflected in the complete GYP_DEFINES string for each platform
below, so you don't need to to anything about that at this point. However, the webrtc build system is buggy in this regard and
does not check only the use_xxx flags when determining the backend, but it also decides based on the target operating system.
So we need to patch it.

* iOS  
Apply the patch to webrtc/base/base.gypi:
`svn patch /path/to/karere/webrtc-build/ios/webrtc.patch`

* Linux, MacOS  
TODO:
No patches are ready yet, you should edit webrtc/base/base.gyp yourself. However, practical experience on these platforms shows
that building webrtc against nss, when openssl is a dynamic library does not cause problems on these platforms.
So you can go that route for now, just make sure openssl is installed as a dynamic lib.
The boringssl changes that you just made do not hurt so you don't need to revert them if you decided to let webrtc build with nss.
However, you should remove `use_openssl=1 use_nss=0` from the GYP_DEFINES string provided here.

### Configure the build ###
We need to set some env variables before proceeding with running the config scripts.  
`export GYP_GENERATORS="ninja"`  

* Linux:  
Rewrite the clang download script to empty, as the download errors out because of self-signed https cert of the server,
and we don't actually need clang on Linux:  
`echo "" > tools/clang/scripts/update.py`  
To fix an issue with missing sanitizer_options.cc, described at `https://code.google.com/p/chromium/issues/detail?id=407183`  
apply a patch to the ./DEPS file to fix it:  
`svn patch path/to/karere/platforms/linux/webrtc.patch`  
Then set the GYP options with:  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 enable_tracing=1 clang=0 use_openssl=1 use_nss=0"`   

* Mac:  
We will want to build webrtc using the system clang compiler instead of the one provided by google with depot_tools. In this
way we will avoid linking problems with runtime, ABI issues etc. To do so, we first need to set the CC and CXX env variables:  
`export CC=/usr/bin/clang`  
`export CXX=/usr/bin/clang++`  
The build process uses a clang compiler plugin to do some automated code checks etc, and it will not work with the system
compiler, causing an error. So we need to disable this plugin via `clang_use_chrome_plugins=0` parameter in GYP_DEFINES (see below).
Also, we are going to force the use of libc++ instead of libstdc++ as a standard lib for clang. This would cause an error that
10.6 target mac platform is too old to require libc++, as it is relatively new. That's why we need to bump the target platform
version to 10.7, using `mac_deployment_target`.  
So, the final `GYP_DEFINES` looks like this:  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 OS=mac target_arch=x64 clang_use_chrome_plugins=0 mac_deployment_target=10.7 use_openssl=1 use_nss=0"`  
Having 10.7 as target however, will cause a deprecation warning, that will be treated as error, *if* compiling webrtc with `nss`.
So if you compile with nss, you need to modify `net/third_party/nss/ssl.gyp` and add to the `cflags`
`-Wno-deprecated-declarations`. If this does not work for some reason, or you have missed to do it before generating
and editing the ninja makefiles, you can edit the corresponding ninja file in out/Debug|Release/obj/net/third_party/nss/libssl.ninja
and add the flag to `cflags`.  

**Change camera capturer**  
The video capturer on macos is based on QTKit (QuickTime) and is practically unusable when capture creation and operations are
initiated from the main thread of the app (as is the case with Karere), because a deadlock occurs.  
*Details*  
The reason is that the main thread queues the operations on a worker thread and waits until the operation is executed in
order to return its result. However the qtkit capturer code does several performSelectorOnMainThread-s and waits for the result.
The main thread however is waiting for us, so a deadlock occurs.
It seems the the execution on the main thread is a workaround for some bugs that were found when using the QTKit API, although
the doc says QTKit API is thread-safe. The explicit calls on the main thread are solvable by replacing them with
performSelectorInBackground, which successfully starts camera capture, but stopping it calls the main thread internally from
within the framework, so it's not fixable. Moreover, QTKit is deprecated by Apple. So the capturer is completely replaced with
a modified version of the iOS capturer, which uses AVFoundation - the recommended way to do capture.  
*End Details*  
The code for the capturer is in `karere-native/webrtc-build/macos/mac-capturer`. You need to delete the following directory in
the webrtc tree:  
`rm webrtc/modules/video_capture/mac`  
and then copy `mac-capturer` at the place of the deleted dir, again with the name `mac`. Also, you need to patch the webrtc tree via:  
`svn patch /path/to/karere-native/webrtc-build/macos/webrtc.patch`  
This will do some modifications to the build system to use the new capturer and also a small modification to a source file.

* Android:  
Run a script to setup the environment to use the built-in android NDK:  
`build/android/envsetup.sh`  
We need to hack the webrtc build system to use the gnustl C++ runtime instead of stlport. This is important because we
have to use the same runtime at least in the webrtc module of Karere, and stlport does not have good support for C++11, exceptions
are disabled and we use them a lot. To apply the gnustl patch (to the build/common.gypi file),
verify that you are in the webrtc trunk directory:  
`cd build && svn patch /path/to/karere/webrtc-build/android/common.gypi.patch`  
Also, some small fixes need to be applies to the webrtc code to be able to build with gnustl.
To make these changes easy, apply the following patch, which and also fixes the sanitized_options build issue
(described below). Verify that you are in the webrtc trunk directory, and do:  
`svn patch /path/to/karere-native/webrtc-build/android/webrtc.patch`  
Note that the patches are valid only for the 6937 revision of webrtc. The reason why the patches are two and not one combined
is that these are two separate svn repos, and not one.  
Configure GYP:  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 enable_tracing=1 OS=android target_arch=arm arm_version=7"`  

* iOS  
No manual configuration is needed, this is done by the provided `build.sh` script, see below.

### Generate the makefiles ###

* Non-iOS  
Issue the command:  
`gclient runhooks --force`  
This will run the config scripts and generate ninja files from the gyp projects.

* Mac  
To force the use of libc++ std library, we need to provide the `-stdlib=libc++` flag to all C++ and ObjC++ compile commands,
by modifying the out/Release|Debug/build.ninja file that contains the basic rules for building the various types of source files.  
To the rules `rule cxx` and `rule objcxx`, in the `command = ` lines, just before `$cflags_pch_xxx`,  add the following:  
`-stdlib=libc++`  
and make sure it is surrounded with spaces from the adjacent parameters.  
Then, find the `rule link`, and in a similar way, add to the command, before `$libs$postbuilds`:  
`-stdlib=libc++ -lc++`  
This also instructs the linker to link against the libc++.

### Build ###
* Non-iOS  
Run:  
`ninja -C out/Release`  
or  
`ninja -C out/Debug`  
to build webrtc in the corresponding mode. Go get a coffee.  

* iOS  
The configure and build steps are automated by a shell script. Run:  
`/path/to/karere/webrtc-build/ios/build.sh`  
from within the trunk webrtc directory.

### Verify the build ###
* Cd to build directory
   - non-iOS  
  `cd out/Release|Debug`

  - iOS  
  `cd out/Release-iphoneos|Debug-iphoneos`

* Built executables  

  - Linux and Windows builds    
  Run `peerconnection_server` app to start a signalling server.  
  Run two or more `peerconnection_client` instances and do a call between them via the server.

  - Android  
  The build system generates a test application `WebRTCDemo-debug.apk`. Copy it to a device, install it and run it.

  - Mac and iOS 
  The build generates an AppRTCDemo.app that works with the apprtc web app at `https://apprtc.appspot.com`  

### Using the webrtc stack with CMake ###
Unfortunately the webrtc build does not generate a single lib and config header file (for specific C defines
 to configure the code). Rather, it creates a lot of smaller static libs that can be used only from within the chrome/webrtc
 build system, which takes care of include paths, linking libs, setting proper defines (and there are quite a few of them).
 So we either need to build our code within the webrtc/chrome build system, rewrite the build system to something more
 universal, or do a combination of both. That's what we do currently. Fortunately, the Chrome build system generates
 a webrtc test app that links in the whole webrtc stack - the app is called `peerconnection_client`. We can get the ninja file
 generated for this executable and translate it to CMake.
 The file is located at `trunk/out/Release|Debug/obj/talk/peerconnection_client.ninja`. The webrtc-build/CMakeLists.txt file
 is basically a translation of this ninja for different platforms, and allows linking webrtc in a higher level
 CMake file with just a few lines. You do not need to run that cmake script directly, but rather include it from the actual
 application that links to it. THis is already done by the rtcModule build system.
 This will be described in more detail in the webrtc module build procedure.

## Building the Karere codebase, including the test app ##
Change directory to the root of the karere-native checkout  
`mkdir build`  
`cd build`  
`ccmake ../src/rtctestapp`  
In the menu, first hit 'c'. The config parameters will get populated. Then you need to setup the following paths:  
`webrtcRoot` - path to the trunk directory of the webrtc source tree  
`WEBRTC_BUILD_TYPE` - the build mode of the webrtc code, as built with ninja. This is the dir name specified to ninja with
the -C option after `out/`. If you built with `-C opt/Release`, then specify `Release` here, similarly for Debug.
* iOS
This dir is normally `Release-iphoneos` or `Debug-iphoneos` to differentiate from simulator builds.  

`CMAKE_BUILD_TYPE` - Set this to Debug to build the webrtc module (not the webrtc stack itself) in debug mode 
`optMegaSdkPath` - Set up the path to the dir where you checked out and built the mega sdk repository (see dependencies).  
`optStrophePath` - Set the path to the dir where you checked out our verison of Strophe. No need to have built it,
 it will be built inside the Karere build tree. The below options come from the strophe build system which is included.
 For more info on these, check the Strophe Readme.md file i nthe Strophe repo.
`optStropheSslLib` - Set to OpenSSL, if not already set.
`optStropheXmlLib` - Can be set to `EXPAT`, `LibXml2` or `Detect` for autodetecting. Tested mostly with expat.
`optStroheNoLibEvent` - make sure it's OFF! If it's ON this means that libevent (including development package) was not found on your system.  
`MPENC_DIR` - the dir of the mpEnc checkout

* Mac  
You need to tell CMake to use the openssl version that you installed, because it would normally detect and use the system version.
To do that, set the `OPENSSL_CRYPTO_LIBRARY` and `OPENSSL_SSL_LIBRARY` to point to the `libcrypto.dylib` and `libssl.dylib` files
respectively of the openssl that you installed, and `OPENSSL_INCLUDE_DIR` to the dir containing
the /openssl dir containing the openssl headers. Note that these 3 CMake variables are 'advanced' so in ccmake you need to hit 't'
to show them.  

* iOS
You  must set all options to build as shared library to OFF.  

Hit 'c' again to re-configure, and then 'g'. After that ccmake should quit and in the console, just type  
`make`  
And if all is well, the test app will build.

## Building the Doxygen documentation ##
From withing the build directory of the previous step, type  
`make doc`  

# Getting familiar with the codebase #
## Basic knlowledge ##
  * Strophe: https://code.developers.mega.co.nz/messenger/strophe-native  
The public headers are:
    - The plain C interface is mstrophe.h
    - The C++ interface is mstrophepp.h and mstrophepp-conn.h 
    - study bot-libevent.cpp example in /examples
  * The Promise lib in base/promise.h and example usage for example in /src/test-promise.cpp
  * The Conversation/Chatroom management code in /src/chatRoom.h;.cpp
  * The overall client structure in /src/chatClient.h;.cpp
  * The setTimeout() and setInterval() timer functions in /src/base/timers.h  

## Test application ##
  * /src/rtctestapp is a Qt application that binds all together in a simple test app.
 Among other things, it shows how the GCM (Gui Call Marshaller) is implemented on the API user's side.
 It also shows how to use the IVideoRenderer interface to implement video playback.

## For application implementors ##
  * The rtctestapp above is the reference app. Build it, study it, experiment with it.
Note theat there is one critical and platform-dependent function that each app that uses Karere must provide, which will be
referenced as `megaPostMessageToGui()`, but it can have any name ,provided that the signature is `extern "C" void(void*)`.
This function is the heart of the message passing mechanism (called the Gui Call Marshaller, or GCM) that Karere relies on.
You must pass a pointer to this function to `services_init()`.  
For more details, read the comments in base/gcm.h, and for reference implementation study rtctestapp/main.cpp
  * IRtcModule, IEventHandler in /src/IRtcModule.h. These are used to initiate rtc calls and receive events.
  * IVideoRenderer in /src/IVideoRenderer.h is used to implement video playback in arbitrary GUI environments.
    Example implementation for Qt is in src/videoRenderer_Qt.h;.cpp.
    The example usage can be seen from the rtctestapp application.

## If Mega API calls are required ##
  * To integrate with the environment, a simple bridge class called MyMegaApi is implemented in /src/sdkApi.h.
    Example usage of it is in /src/chatClient.cpp and in /src/megaCryptoFuncs.cpp. 

## More advanced things that should not be needed but are good to know in order to understand the underlying environment better ##
  * The function call marshalling mechanims in /src/base/gcm.h and /src/base/gcmpp.h. The code is documented in detail.
    The mechanism marshalls lambda calls from a worker thread to the GUI thread. Examples of use of
    marshallCall() can be seen for example in /src/webrtcAdapter.h and in many different places.
    This mechanism should not be directly needed in high-level code that runs in the GUI thread.
