#  Building karere-native #

## Get the code ##

Checkout the Karere repository:  
`git clone --recursive https://code.developers.mega.co.nz/messenger/karere-native`    
Note the `--recursive` switch - the repository contains git submodules that need to be checked out as well.

## Toolchains ##

* Android  
Install the android NDK, and create a standalone CLang toolchain, using the `make-standalone-toolchain.sh` scrpt included in the NDK, using following example commandline:  
`./android-ndk-r11b/build/tools/make-standalone-toolchain.sh --toolchain=arm-linux-androideabi-clang --install-dir=/home/user/android-dev/toolchain --stl=libc++ --platform=android-21`
Then, you need to prepare a cross-compile environment for android. For this purpose, you need to use the `/platforms/android/env-android.sh` shell script. Please read the Readme.md in the same directory on how to use that script.

*Notes on building webrtc with Android*    
Building of webrtc is supported only on Linux. This is a limitation of Google's webrtc/chromium build system.  
Although webrtc comes with its own copy of the NDK, we are going to use the standard one for building the karere-native code, and let webrtc build with its own NDK. Both compilers are binary compatible. Forcing webrtc to build with an external NDK will not work. For some operations, like assembly code transformations, a host compiler is used, which is the clang version that comes with webrtc. To use an external NDK, we would need to specify explicitly specify the `--sysroot` path of the external NDK, which also gets passed to the clang host compiler, causing errors. 

* iOS  
Building of webrtc and karere is supported only on MacOS. XCode is required.
Then, you need to prepare a cross-compile environment for android. For this purpose, you need to use the `/platforms/ios/env-ios.sh` shell script. Please read the Readme.md in the same directory no how to use that script.

* Windows
Microsoft Visual Studio 2015 or later (only C++ compiler), and the Cygwin environment are required. You need to setup the start shortcut/batfile of the Cygwin shell to run the vcvars32.bat with the cmd.exe shell inside the cygwin shell, in order to add the compiler and system library paths to the Cygwin shell. Building of Karere, webrtc and dependencies will be done under the Cygwin shell.


## Build dependencies ##
For Linux and MacOS, you need to install all dependencies using a package manager (the operating system's package manager for Linux, and MacPorts or Homebrew on MacOS). For cross-compiled targets (android, ios), and for Windows, an automated system is provided to download, build and install all dependencies under a specific prefix. See below for details.

### List of dependencies
 - `cmake` and `ccmake` (for a config GUI).
 * Windows
Under Windows, the cygwin version of cmake will not be sufficient, you need to install the native cmake for Windows from the official site, and instead of ccmake use cmake-gui. You will also need to add the path to Cmake to Cygwin.
 - `libevent2.1.x`  
Version 2.0.x will **not** work, you need at least 2.1.x. You may need to build it from source, as 2.1.x is currently considered beta (even though it has critical bugfixes) and system packages at the time of this writing use 2.0.x. For convenience, libevent is added as a git submodule in third-party/libevent, so its latest revision is automatically checked out from the official libevent repository.  
 - `openssl` - Needed by the SDK, webrtc, strophe and Karere itself.  
 - Native WebRTC stack from Chrome. See below for build instructions for it.  
 - The Mega SDK - Check out the repository, configure the SDK with --enable-chat, and in a minimalistic way - without image libs etc. Only the crypto and HTTP functionality is needed.  
 - `libcrypto++` - Needed by MegaSDK and Karere.
 - `libsodium` - Needed by MegaSDK and Karere.
 - `libcurl` - Needed by MegaSDK and Karere.  
 - `Qt5` - QtCore and QtWidgets required only, needed only for the desktop example app.  

### Automated dependency build system ###
This is supported only for android, ios, and Windows (for now). The script works in tandem with the cross-compile build environment when building for mobile, so you need that environement set up, as described in the previous sections. You just need to run `/platforms/setup-deps.sh` script without arguments to get help on how to use it, and then run it with arguments to download, build and install all dependencies.

*IMPORTANT for iOS*  
As Apple does not allow dynamic libraries in iOS apps, you must build all third-party dependencies as static libs, so you need to tell the dependency build system to build everything as static libs.

## Build webrtc ##
Karere provides an autmated system for building webrtc for any of the supported desktop and mobile platforms. This is made very easy by using the `/webrtc-build/build-webrtc.sh` script. Run it without arguments to see help on usage. This system is generally an addon to the stock webrtc (actually chromium) build sustem, but it strips it down to download only a few hundred megabytes of source code and tools instead of 10-12GB. It also patches webrtc to fix several issues (use normal openssl instead of its own included boringssl lib, replace macos capturer that uses obsolete API and problematic threading model with modified iOS capturer, etc).

### Verify the build ###
* Cd to the directory you passed to build-webrtc.sh, and then:  
   - non-iOS  
  `cd src/out/Release|Debug`  
  - iOS  
  `cd src/out/Release-iphoneos|Debug-iphoneos`  

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
 to configure the code). Rather, it creates a lot of smaller static libs that can be used only from within the chromium/webrtc build system, which takes care of include paths, linking libs, setting proper defines (and there are quite a few of them). So we either need to build our code within the webrtc/chrome build system, rewrite the build system to something more universal, or do a combination of both. That's what we do currently. Fortunately, the Chrome build system generates a webrtc test app that links in the whole webrtc stack - the app is called `peerconnection_client`. We can get the ninja file generated for this executable and translate it to CMake. The file is located at `src/out/Release|Debug/obj/talk/peerconnection_client.ninja`. The webrtc-build/CMakeLists.txt file is basically a translation of this ninja for different platforms, and allows linking webrtc in a higher level CMake file with just a few lines. You do not need to run that cmake script directly, but rather include it from the actual application or library that links to it. This is already done by the rtcModule build system. This will be described in more detail in the webrtc module build procedure.

## Build the Karere codebase, including a test app ##
Change directory to the root of the karere-native checkout  
`mkdir build`  
`cd build`  

### Invoke ccmake config menu ###

* Desktop OS-es  
If you installed dependencies in non-system prefixes (recommended on MacOS), you need to provide these prefixes to cmake. They will be searched before the system paths, in the order provided:
`ccmake ../examples/qt -DCMAKE_PREFIX_PATH="path/to/prefix1;path/to/prefix2..."`
If you don't need to provide custom prefixes, just issue:
`ccmake ../examples/qt`  

* iOS and Android  
You need to have `env-ios.sh/env-android.sh` sourced in the shell, as explained above. Then, run *ccmake* in cross-compile mode as per the instructions
For example:  
`eval ccmake $CMAKE_XCOMPILE_ARGS ../src`  
This (after configuring via the menu) will cross-compile the Karere SDK for Android or iOS, depending on the cross-compile environment you have set up.  
`eval ccmake -GXcode $CMAKE_XCOMPILE_ARGS ../examples/objc`  
This will not build the iOS app but generate a XCode project linking all dependencies (including webrtc).
You can build that project with XCode.  

### Configure Karere ###
In the ccmake menu that appeared in the previous step, first hit 'c'. The config parameters will get populated.
Then you need to setup the following paths:  
`webrtcRoot` - path to the `src` directory of the webrtc source tree, as you have specified to `build-webrtc.sh`  
`WEBRTC_BUILD_TYPE` - the build mode of the webrtc code, as specified to `build-webrtc.sh`  
This is either `Release` or `Debug` for non-iOS, or `Release-iphoneos` or `Debug-iphoneos` for iOS (to differentiate from simulator builds).  
`CMAKE_BUILD_TYPE` - Set the build mode for the example app, Karere and all its submodules - `Debug` or `Release`.
`optStropheSslLib` - Set to OpenSSL, if not already set.
`optStropheXmlLib` - Can be set to `EXPAT`, `LibXml2` or `Detect` for autodetecting. Tested mostly with expat.
`optStroheNoLibEvent` - make sure it's OFF! If it's ON this means that libevent (including development package) was not found on your system.  
For more info on Strophe build options, check the Readme: third-party/strophe-native/Readme.md  

* iOS  
You must set all options to build anything as shared library to OFF, as Apple doesn't allow dynamic linking on iOS.  

Hit 'c' again to re-configure, and then 'g'. After that ccmake should quit.

* Non-iOS  
In the console, just type  
`make`  
And if all is well, the test app will build.  

* iOS  
After ccmake has quit, you should have an xcode project in the build dir.

## Building the Doxygen documentation ##
From within the build directory of the previous step, provided that you generated a make build, type  
`make doc`  

# Getting familiar with the codebase #
## Introduction to the threading model ##
The karere threading model is similar to the javascript threading model - everything runs in the main (GUI) thread, blocking is never allowed, and external events (network, timers etc, webrtc events etc) trigger callbacks on the main thread. For this to work, karere-native must be able to interact with the application's event loop - this is usually the event/message loop of the GUI framework in case of a GUI application, or a custom message loop in case of a console application. As this message loop is very platform-specific, it is the application developer's responsibility to implement the interface between it and karere-native. This may sound more complicated than it is in reality - the interface consists of two parts - the implementation of megaPostMessageToGui(void*) function posts an opaque void* pointer to the application's message loop. This function is normally called by threads other than the main thread, but can also be called by the GUI thread itself. The other part is the part of the application's message loop that recognizes this type of messages and passes them back to karere, by calling megaProcessMessage(void*) with that same pointer - this time in the context of the main (GUI) thread. All this is implemented in /src/base/gcm.h and /src/base/gcm.hpp. These files contain detiled documentation.    
Karere-native relies on libevent, running in its own dedicated thread, to monitor multiple sockets for raw I/O events, and to implement timers. It also relies on the higher-level I/O functionality of libevent such as DNS resolution and SSL sockets. A thin layer on top of libevent, called 'services' (/src/base/?services\*.\*)is implemented on top of libevent and the GCM to have simple, javascript-like async C++11 APIs for timers (src/base/timer.h), dns resolution (/src/base/services-dns.hpp), http client (/src/base/services-http.hpp). This layer was originally designed to have a lower-level component with plain C interface (cservices*.cpp/h files), so that the services can be used by several DLLs built with different compilers, and a high-level header-only C++11 layer that is the frontend and contains the public API - these are the .hpp files.    
All network libraries in karere-native (libstrophe, libws, libcurl) use libevent for network operation and timers (C libraries use libevent directly, C++ code uses the C++11 layer, i.e. timers.hpp). It is strongly recommended that the SDK user also does the same, although it is possible for example to have a dedicated worker thread blocking on a socket, and posting events to the GUI thread via the GCM.
The usage pattern is as follows: a callback is registered for a certain event (socket I/O event, timer, etc), and that callback is called by *the libevent thread* when the event occurs. If the event may propagate outside the library whose callback is called, and especially to the GUI, then, at some point, event processing must be marshalled to the GUI thread, using the GCM mechanism. However, if the event is internal and never propagates outside the library then it can be handled directly in the context of the libevent thread (provided that it never blocks it). This saves the performance cost of marshalling it to the GUI thread, and is recommended if the event occurs at a high frequency, e.g. an incoming data chunk event that only needs the data appended to a buffer. When the transfer is complete, a completion event can be marshalled on the GUI thread once per transfer, combining the advantages of both approaches.

## Logger ##
Karere has an advanced logging facility that supports file and console logging with color, log file rotation, multiple log channels, each with individual log level. Log levels are configured at runtime (at startup), and not at compile time (i.e. not by disabling log macros). This allows a release-built app to enable full debug logging for any channels. Log channels are defined and default-configured in src/base/loggerChannelConfig.h. The file contains detailed documentation. For convenience, dedicated logging macros for each channel are usually defined in the code that uses it - see the XXX_LOG_DEBUG/WARN/ERROR macros in karereCommon.h for examples. The SDK user is free to create additional log channels if needed. A GUI log channel is already  defined. Log channel configuration can be overriden at runtime by the KRLOG environment variable. Its format is as follows:    
        ```KRLOG=<chan name>=<log level>,<chan name2>=<log level2>...```    
    Log levels are 'off', 'error', 'warn', 'info', 'verbose', 'debug', 'debugv'.
    There is one special channel name - 'all'. Setting the log level of this channel sets the log levels of all channels. This allows for example to easily silence all channels except one (or few), by:
       ```KRLOG=all=warn,mychannel=debug```
The same channel can be configured multiple times, and only the last setting will be effective, which makes the above trick possible.
    Karere requires the function karere::getAppDir() to be defined by the application at compile time, in order to know where to create the log file and start logging as early as possible, before main() is entered. If karere is build as a static lib, this is not a problem. In case of dynamic lib, this function has to be a weak symbol, so that karere itself can compile without the function implementation, and the implementation to be linked when the karere shared lib is loaded at app startup. Weak symbols are not really portable across compilers, and this may be a problem. However they are supported by both gcc and clang. If no weak symbols are supported, karer ehas to be built as static lib.

## Files of interest ##

  * The Promise lib in base/promise.h and example usage for example in /src/test-promise.cpp
  * The setTimeout() and setInterval() timer functions in /src/base/timers.hpp  
  * The overall client structure in /src/chatClient.h;.cpp
  * The test app main.cpp file in examples/qt - shows how to implement megaPostMessageToGui(), how to start the 'services', and how to instantiate the karere client. Also shows how to implement the getAppDir() method, which is a weak symbol needed by the karere library in order to create the log file and start logging as early as possible, before main() is entered. 
  * The video module public interface in src/rtcModile/IRtcModule.h and related headers
## Video renderer widgets ##
Karere provides platform-specific video renderer widgets for Qt and iOS (probably will work also for MacOS with no or minimal changes).
These widgets are implemented as subclasses of standard widgets. Their code is in src/videoRenderer_xxx.cpp;mm;h. They can be used directly
in the platform's GUI designer by placing the corresponding standard widget and then selecting the VideoRenderer_xxx class name from the menu
to tell the GUI designer to use that subclass. You must include these files in your project, including the headers, to make the subclass visible
to the GUI designer.

## Test Applications ##
Karere provides several example apps, which, among other things, show how the GCM (Gui Call Marshaller) is implemented on the API user's side.
It also shows how to use the video renderer widgets and the IVideoRenderer interface they implement to do video playback.

The test apps are:
* examples/qt - a Qt application.
* examples/objc - an iOS app.

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
