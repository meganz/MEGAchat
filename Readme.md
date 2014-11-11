#  Building the webrtc module #

## Dependencies ##

 - `cmake` (and ccmake if a config GUI is required)  
 - Our version of strophe (https://code.developers.mega.co.nz/messenger/strophe-native, see readme for it's own dependencies)  
 - Qt4 (for the test app): `libqtcore4 libqtgui4 libqt4-dev`  
 - Native WebRTC stack from Chrome, see below for build instructions for it.  
 - Our high level CMake build system that allows CMake projects to build and link with webrtc:  
`git clone git@code.developers.mega.co.nz:messenger/webrtc-buildsys.git`

## Building webrtc ##
First, create a directory where all webrtc stuff will go, and cd to it. All instructions in this section assume that the current directory is that one.  

### Install depot_tools ###
We need to install Google's depot_tools, as per these instructions:  
https://sites.google.com/a/chromium.org/dev/developers/how-tos/install-depot-tools  

Make sure they are in the system's path, because they provide custom versions of commands that may already be available on the system, and the custom ones must be picked instead of the system ones.

### Checkout the code ###
For code checkout and configuration the tool `gclient` from depot tools is used, as it needs to checkout 100s of repositories and run a lot of config scripts. What is more specific here is that we need to checkout a specific revision of the source tree, as there are a lot of changes in the webRTC code and the most recent code will not work. Another reason not to use the most recent version is that Google changed the build process to require the whole Chromium source tree, which is about 10GB of code, versus the older versions that require only a fraction of that. First, we need to tell gclient with what repository to work:  
`gclient config http://webrtc.googlecode.com/svn/trunk`  
This creates a .gclient file in the current dir. Next, checkout the code:  
`gclient sync --revision r6937 --force`  
This may take a long time.  

### Install dependencies ###
There are various packages required by the webrtc build, most of them are checked out by gclient, but there are some that need to be installed on the system. To do that on linux, you can run:  
`cd trunk/build && ./install-build-deps.sh`  
Also you need Java JDK 6 or 7(For something related to android, but seems to be run unconditionally):  
If you don't have JDK installed, install `openjdk-7-jdk`. Export JAVA_HOME to point to your JDK installation, on Ubuntu is something like that:  
`export JAVA_HOME=/usr/lib/jvm/java-7-openjdk`   

### Configure the build ###
We need to set some env variables before proceeding with running teh config scripts.  
`export GYP_GENERATORS="ninja"`  
`export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0`  
Now issue the command:  
`gclient runhooks --force`  
This will run the config scripts and generate ninja files from the gyp projects.

### Build ###
Run:  
`ninja -C out/Release`  
or  
`ninja -C out/Debug`  
to build webrtc in the corresponding mode. Go get a coffee.

### Build with CMake ###
Unfortunately the webrtc build does not generate a single lib and config header file(for specific C defines to configure the code). Rather, it creates a lot of smaller static libs that can be used only from within the chrome/webrtc build system, which takes care of include paths, linking libs, setting proper defines (and there are quite a few of them). So we either need to build our code within the webrtc/chrome build system, rewrite the build system to something more universal, or do a combiantion of both. That's what we do currently. Fortunately, the Chrome build system generates a webrtc test app that links in the whole webrtc stack - the app is called peerconnection_client. We can get the ninja file generated for this executable and translate it to CMake. The file is located at trunk/out/Release|Debug/obj/talk peerconnection_client.ninja. The webrtc-buildsys project that was listed in the dependencies is a basically a translation of this ninja file and allows linking webrtc in a higher level CMake file with just a single line (with the help of a cmake module that allows propagation of defines, include dirs etc to dependent projects in a very simple and straightforward way).  
You do not need to run cmake in webrtc-buildsys directly, but rather include it from the actual application that links to it. This will be descripbed in the webrtc module build procedure.

## Build of webrtc module ##
Checkout the git repository and cd to the root of the checkout  
`mkdir build`  
`cd build`  
`ccmake ../src`  
In the menu, first hit 'c'. The config parameters will get populated. Then you need to setup the following paths:  
`optWebrtcCmakeBuild` -  path to the directory where the webrtc-buildsys checkout is located  
`webrtcRoot` - path to the trunk directory of the webrtc source tree  
`WEBRTC_BUILD_TYPE` - the build mode of the webrtc code, as built with ninja. If you built with `-C opt/Release`, then specify `Release` here, similarly for Debug.  
`CMAKE_BUILD_TYPE` - Set this to Debug to build the webrtc module (not the webrtc stack itself) in debug mode 
Hit 'c' again to re-configure, and then 'g'. After that ccmake should quite and in the console, just type
`make`  
And if all is well, the test app will build.

