#  Building the webrtc module #

## Dependencies ##

 - `cmake` (and ccmake if a config GUI is required)  
 - Our version of strophe (https://code.developers.mega.co.nz/messenger/strophe-native, see readme for it's own dependencies).
 No need to explicitly build it, will be done by the Karere build system.  
 - `libevent2` - at least 2.0.x  
 - Qt4 (for the test app): `libqtcore4 libqtgui4 libqt4-dev`  
 - Native WebRTC stack from Chrome, see below for build instructions for it.  
 - The Mega SDK. Check out the repository, configure the SDK in a minimalistic way - without image libs etc.
 Only the crypto and HTTP functionality is needed. Install crypto++ and libcurl globally in the system, because the Karere
 build will need to find them as well and link to them directly. Install any other mandatory Mega SDK dependencies
 and build the SDK. It does not have to be installed with `make install`, it will be accessed directly in the checkout dir.  
 - Our high level CMake build system that allows CMake projects to build and link with webrtc:  
`git clone git@code.developers.mega.co.nz:messenger/webrtc-buildsys.git`

## Building webrtc ##
First, create a directory where all webrtc stuff will go, and cd to it. All instructions in this section assume that the
current directory is that one.  

### Install depot_tools ###
We need to install Google's depot_tools, as per these instructions:  
https://sites.google.com/a/chromium.org/dev/developers/how-tos/install-depot-tools  

Make sure they are at the before all other paths in the system's path, because they provide custom versions of commands
 that may already be available on the system, and the custom ones must be picked instead of the system ones.

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
`cd trunk/build && ./install-build-deps.sh`  
Also you need Java JDK 6 or 7 (for something related to android, but seems to be run unconditionally):  
If you don't have JDK installed, install `openjdk-7-jdk`. Export `JAVA_HOME` to point to your JDK installation, on Ubuntu
is something like that:  
`export JAVA_HOME=/usr/lib/jvm/java-7-openjdk`   

* Mac
Install Homebrew and use `brew install` to install java JDK 6 or 7.  
Export JAVA_HOME to point to your JDK installation:  
`export JAVA_HOME=/Library/Java/JavaVirtualMachines/jdk1.7.0_71.jdk/Contents/Home/`

* Android
JDK 7 will not work for this particular revision (some warnings are triggered and the build is
configured to treat warnings as errors). Therefore you need to install JDK 6 (unless already done).  
Export JAVA_HOME to point to the JDK installation:  
`export JAVA_HOME=/usr/lib/jvm/java-6-openjdk`   

### Configure the build ###
We need to set some env variables before proceeding with running the config scripts.  
`export GYP_GENERATORS="ninja"`  

* Linux:  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 enable_tracing=1"` 

* Mac:  
`export GYP_DEFINES="enable_tracing=1 build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 OS=mac target_arch=x64"`  
`export GYP_CROSSCOMPILE=1`  
`perl -0pi -e 's/gdwarf-2/g/g' tools/gyp/pylib/gyp/xcode_emulation.py`  
`perl -0pi -e 's/\$\(SDKROOT\)\/usr\/lib\/libcrypto\.dylib/-lcrypto/g' talk/libjingle.gyp`  
`perl -0pi -e 's/\$\(SDKROOT\)\/usr\/lib\/libssl\.dylib/-lssl/g' talk/libjingle.gyp`  

* Android:  
Run a script to setup the environment to use the built-in android NDK:  
`build/android/envsetup.sh`  
Configure GYP:  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 enable_tracing=1 OS=android target_arch=arm arm_version=7"`   

Now issue the command:  
`gclient runhooks --force`  
This will run the config scripts and generate ninja files from the gyp projects.

### Build ###
Run:  
`ninja -C out/Release`  
or  
`ninja -C out/Debug`  
to build webrtc in the corresponding mode. Go get a coffee.  

### Possible build problems ###
* If you get an error message about missing `sanitizer_options.cc` file, please add the following code to the `hooks`
section of the `trunk/DEPS` file:
``` 
  {
    "pattern": "tools/sanitizer_options/sanitizer_options.cc",
    "action" : ["svn", "update", "-r", Var("chromium_revision"), Var("root_dir") + "/tools/sanitizer_options/sanitizer_options.cc"],
  },
```
Note: this fix was taken from `https://code.google.com/p/chromium/issues/detail?id=407183`  

Then re-run `gclient runhooks --force`, and then the `ninja` command.

### Verify the build ##
`cd out/Release|Debug`
* Desktop OS builds  
run `peerconnection_server` app to start a signalling server.  
run two or more `peerconnection_client` instances and do a call between them via the server.
* Android  
The build system generates a test application `WebRTCDemo-debug.apk`. Copy it to a device, install it and run it.

### Using the webrtc stack with CMake ###
Unfortunately the webrtc build does not generate a single lib and config header file (for specific C defines
 to configure the code). Rather, it creates a lot of smaller static libs that can be used only from within the chrome/webrtc
 build system, which takes care of include paths, linking libs, setting proper defines (and there are quite a few of them).
 So we either need to build our code within the webrtc/chrome build system, rewrite the build system to something more
 universal, or do a combination of both. That's what we do currently. Fortunately, the Chrome build system generates
 a webrtc test app that links in the whole webrtc stack - the app is called `peerconnection_client`. We can get the ninja file
 generated for this executable and translate it to CMake.
 The file is located at `trunk/out/Release|Debug/obj/talk/peerconnection_client.ninja`. The webrtc-buildsys project that
 was listed in the dependencies is basically a translation of this ninja file and allows linking webrtc in a higher level
 CMake file with just a single line (with the help of a cmake module that allows propagation of defines,
 include dirs etc to dependent projects in a very simple and straightforward way).  
 You do not need to run cmake in webrtc-buildsys directly, but rather include it from the actual application that links to it.
 This will be described in the webrtc module build procedure.

## Building the Karere codebase, including the test app ##
Checkout the `karere-native` git repository and cd to the root of the checkout  
`mkdir build`  
`cd build`  
`ccmake ../src/rtctestapp`  
In the menu, first hit 'c'. The config parameters will get populated. Then you need to setup the following paths:  
`optWebrtcCmakeBuild` -  path to the directory where the webrtc-buildsys checkout is located  
`webrtcRoot` - path to the trunk directory of the webrtc source tree  
`WEBRTC_BUILD_TYPE` - the build mode of the webrtc code, as built with ninja. If you built with `-C opt/Release`,
then specify `Release` here, similarly for Debug.  
`CMAKE_BUILD_TYPE` - Set this to Debug to build the webrtc module (not the webrtc stack itself) in debug mode 
`optMegaSdkPath` - Set up the path to the dir where you checked out and built the mega sdk repository (see dependencies).  
`optStrophePath` - Set the path to the dir where you checked out our verison of Strophe. No need to have built it,
 it will be built inside the Karere build tree. The below options come from the strophe build system which is included.
 For more info on these, check the Strophe Readme.md file i nthe Strophe repo.
`optStropheSslLib` - Set to OpenSSL, if not already set.
`optStropheXmlLib` - Can be set to `EXPAT`, `LibXml2` or `Detect` for autodetecting. Tested mostly with expat.
`optStropheBuildShared` - set it to ON.
`optStropheExportDlsyms` - set it to OFF.
`optStroheNoLibEvent` - make sure it's OFF! If it's ON this means that libevent (including development package) was not found on your system.  
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
  * The Conversation/Chatroom management code in /src/ChatRoom.h;.cpp
  * The overall client structure in /src/ChatClient.h;.cpp
  * The setTimeout() and setInterval() timer functions in /src/base/timers.h  

## Test application ##
  * /src/rtctestapp is a Qt application that binds all together in a simple test app.
 Among other things, it shows how the GCM (Gui Call Marshaller) is implemented on the API user's side.
 It also shows how to use the IVideoRenderer interface to implement video playback.

## For application implementors ##
  * The rtctestapp above is the reference app. Build it, study it, experiment with it.
Note theat there is one critical and platform-dependent function that each app that uses Karere must define, called
`megaPostMessageToGui()`. This function is the heart of the message passing mechanism (called the Gui Call Marshaller, or GCM)
that Karere relies on. If you don't define it, there will be a link error when you try to build the application, saying that
`_megaPostMessageToGui` is an undefined symbol.  
For more details, read the comments in base/gcm.h, and for reference implementation study rtctestapp/main.cpp
  * IRtcModule, IEventHandler in /src/IRtcModule.h. These are used to initiate rtc calls and receive events.
  * IVideoRenderer in /src/IVideoRenderer.h is used to implement video playback in arbitrary GUI environments.
    Example implementation for Qt is in src/VideoRenderer_Qt.h;.cpp.
    The example usage can be seen from the rtctestapp application.

## If Mega API calls are required ##
  * To integrate with the environment, a simple bridge class called MyMegaApi is implemented in /src/sdkApi.h.
    Example usage of it is in /src/ChatClient.cpp and in /src/MegaCryptoFuncs.cpp. 

## More advanced things that should not be needed but are good to know in order to understand the underlying environment better ##
  * The function call marshalling mechanims in /src/base/gcm.h and /src/base/gcmpp.h. The code is documented in detail.
    The mechanism marshalls lambda calls from a worker thread to the GUI thread. Examples of use of
    marshallCall() can be seen for example in /src/webrtcAdapter.h and in many different places.
    This mechanism should not be directly needed in high-level code that runs in the GUI thread.

