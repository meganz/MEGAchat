# MEGAchat library

##  How to build the MEGAchat library

For the development and compilation of MEGAchat, we mainly use CMake as the cross-platform project configuration tool. We also use VCPKG to manage the required dependencies to build MEGAchat in most platforms: Windows, MacOS and Linux.

For details on the necessary building tools for each operating system, review the [Building tools](https://github.com/meganz/sdk#building-tools) chapter in the MEGA SDK repository.

More information for the WebRTC Android compilation in [WebRTC for Android](WebrtcAndroid.md)

### Dependencies

MEGAchat requires some dependencies to work. Most of them are automatically downloaded and built using VCPKG during the configuration phase.

For Linux, some extra libraries are needed in the system so that VCPKG can build the dependencies.
For Debian-based distributions, you can install the necessary libraries using the following command:

    sudo apt install python3-pkg-resources libglib2.0-dev libgtk-3-dev libasound2-dev libpulse-dev

Package names may vary for different Linux distributions, but it should build succesfully with packages providing the same libraries.

You can take a look at the complete set of dependencies in the [vcpkg.json](vcpkg.json) file at root of the repository.

MEGAchat also needs the MEGA SDK library. There are instructions later in this document on how to get it to be used with MEGAchat. The MEGA SDK project is automatically loaded by the MEGAchat CMake, so you only have to simply clone it in the expected path.

There is only one extra optional dependency that should be installed in the system: The Qt Framework. It is only necessary to build the Qt Example App but it is not required for the tests, CLI example, or the library itself.

### Prepare the sources

First of all, prepare a directory of your choice to work with MEGAchat. The `mega` directory
will be used as the workspace directory in the examples in this document.

	mkdir mega
	cd mega

After preparing the directory, clone the MEGAchat repository to obtain the MEGAchat source code.

	git clone https://github.com/meganz/megachat

As MEGAchat requires the MEGA SDK to work, clone the MEGA SDK in the expected path.

	git clone https://github.com/meganz/sdk megachat/third-party/mega

Finally, clone the VCPKG repository next to the MEGAchat folder. If you are already using VCPKG and you have a local clone of the repository, you can skip this step and use the existing VCPKG on your system.

	git clone https://github.com/microsoft/vcpkg

### Configuration

The following instructions are for configuring the project from the command line interface (CLI), but cmake-gui or any editor or IDE
compatible with CMake should be suitable if the same CMake parameters are configured.

MEGAchat is configured like any other regular CMake project. The only parameter that is always needed is the VCPKG directory
to manage the third-party dependencies. The MEGA SDK dependency is built as part of the MEGAchat build.

To configure MEGAchat, from the workspace (`mega` directory), run CMake:

	cmake -DVCPKG_ROOT=vcpkg -DCMAKE_BUILD_TYPE=Debug -S megachat -B build_dir

**Note 1**: The `-DCMAKE_BUILD_TYPE=<Debug|Release>` may not be needed for multiconfig generators, like Visual Studio.

**Note 2** If the Qt Framework is installed on your system but CMake fails to detect it, you can add `-DCMAKE_PREFIX_PATH=</path/to/qt/install/dir>` so that CMake can locate it. If Qt is not installed and you prefer not to install it, you can disable the Qt Example App by setting `-DENABLE_CHATLIB_QTAPP=OFF`. The library, CLI example, and tests will still be built.

In the cmake command above, relative paths have been used for simplicity. If you want to change the location of VCPKG, MEGAchat or the build directory, simply provide a valid relative or absolute path for any of them.

During the project configuration, VCPKG will build and configure the necessary libraries for the platform. It may take a while on the first run, but once the libraries are built, VCPKG will retrieve them from the binary cache.

MEGAchat can be configured with different options, some of which can be found in the [chatlib_options.cmake](contrib/cmake/modules/chatlib_options.cmake) file.
The options to manage the examples and tests are in the [CMakeLists.txt](CMakeLists.txt).

### Building the sources

Once MEGAchat is configured, simply build the complete project:

	cmake --build build_dir

You can specify `--target=<target>` like `CHATlib` or `megaclc`, or just leave the command as it is to build all the tagets.
Additionally, `-j <N>` or `--parallel <N>` can be added to manage concurrency and speed up the build.

Once the build is finished, binaries will be available in the `build_dir`, which was specified in the CMake configuration command.

## Getting familiar with the codebase ##

### MegaChatApi: the intermediate layer ###

To abstract the code complexity, MEGAchat provides an intermediate layer that enables to quickly create new applications.  

The documentation is available at `src/megachatapi.h`  


### Introduction to the threading model ###

The MEGAchat threading model is similar to the javascript threading model - everything runs in the main (GUI) thread, blocking is never allowed, and external events (network, timers etc, webrtc events etc) trigger callbacks on the main thread. For this to work, MEGAchat must be able to interact with the application's event loop - this is usually the event/message loop of the GUI framework in case of a GUI application, or a custom message loop in case of a console application. As this message loop is very platform-specific, it is the application developer's responsibility to implement the interface between it and MEGAchat. This may sound more complicated than it is in reality - the interface consists of two parts. One part is the implementation of `megaPostMessageToGui(void*)` function, which posts an opaque `void*` pointer to the application's message loop. This function is normally called by threads other than the main thread, but can also be called by the GUI thread itself. The other part is the code in the application's message loop that recognizes this type of messages and passes them back to MEGAchat, by calling `megaProcessMessage(void*)` with that same pointer - this time in the context of the main (GUI) thread. All this is implemented in `/src/base/gcm.h` and `/src/base/gcm.hpp`. These files contain detailed documentation. An example of implementing this on Windows is: `megaPostMessageToGui(void*)` would do a `PostMessage()` with a user message type, and the `void*` as the lParam or wParam of the message, and in the event processing `switch` statement, there will be an entry for that message type, getting the `void*` pointer by casting the lParam or wParam of the message, and passing it to `megaProcessMessage(void*)`.

MEGAchat relies on libuv, running in its own dedicated thread, to monitor multiple sockets for raw I/O events, and to implement timers. It also relies on the higher-level I/O functionality of libuv such as DNS resolution and SSL sockets. A thin layer on top of libuv, called 'services' (`/src/base/?services\*.\*`) is implemented on top of libuv and the GCM to have simple, javascript-like async C++11 APIs for timers (`src/base/timer.h`), DNS resolution (`/src/base/services-dns.hpp`), http client (`/src/base/services-http.hpp`). This layer was originally designed to have a lower-level component with plain C interface (`cservices*.cpp/h` files), so that the services can be used by several DLLs built with different compilers, and a high-level header-only C++11 layer that is the frontend and contains the public API - these are the .hpp files.    

All network libraries in MEGAchat (libcurl) use libuv for network operation and timers (C libraries use libuv directly, C++ code uses the C++11 layer, i.e. `timers.hpp`). It is strongly recommended that the SDK user also does the same, although it is possible for example to have a dedicated worker thread blocking on a socket, and posting events to the GUI thread via the GCM.

The usage pattern is as follows: a callback is registered for a certain event (socket I/O event, timer, etc), and that callback is called by *the libuv thread* when the event occurs. If the event may propagate outside the library whose callback is called, and especially to the GUI, then, at some point, event processing must be marshalled to the GUI thread, using the GCM mechanism. However, if the event is internal and never propagates outside the library then it can be handled directly in the context of the libuv thread (provided that it never blocks it). This saves the performance cost of marshalling it to the GUI thread, and is recommended if the event occurs at a high frequency, e.g. an incoming data chunk event that only needs the data appended to a buffer. When the transfer is complete, a completion event can be marshalled on the GUI thread once per transfer, combining the advantages of both approaches.

### Logger ###

MEGAchat has an advanced logging facility that supports file and console logging with color, log file rotation, multiple log channels, each with individual log level. Log levels are configured at runtime (at startup), and not at compile time (i.e. not by disabling log macros). This allows a release-built app to enable full debug logging for any channels. Log channels are defined and default-configured in src/base/loggerChannelConfig.h. The file contains detailed documentation. For convenience, dedicated logging macros for each channel are usually defined in the code that uses it - see the XXX_LOG_DEBUG/WARN/ERROR macros in karereCommon.h for examples. The SDK user is free to create additional log channels if needed. A GUI log channel is already  defined. Log channel configuration can be overriden at runtime by the KRLOG environment variable. Its format is as follows:  
    ```KRLOG=<chan name>=<log level>,<chan name2>=<log level2>...```  
    Log levels are 'off', 'error', 'warn', 'info', 'verbose', 'debug', 'debugv'.    
    There is one special channel name - 'all'. Setting the log level of this channel sets the log levels of all channels. This allows for example to easily silence all channels except one (or few), by:  
    ```KRLOG=all=warn,mychannel=debug,myotherchannel=info```   
    The same channel can be configured multiple times, and only the last setting will be effective, which makes the above trick possible.  
MEGAchat requires the function `karere::getAppDir()` to be defined by the application at compile time, in order to know where to create the log file and start logging as early as possible, before `main()` is entered. If MEGAchat is built as a static lib, this is not a problem. In case of dynamic lib, this function has to be a weak symbol, so that MEGAchat itself can compile without the function implementation, and the implementation to be linked when the MEGAchat shared lib is loaded at app startup. Weak symbols are not really portable across compilers, and this may be a problem. However they are supported by both gcc and clang. If no weak symbols are supported, karer ehas to be built as static lib.

### Files of interest ###

  * The Promise lib in `base/promise.h` and example usage for example in `/src/test-promise.cpp`
  * The `setTimeout()` and `setInterval()` timer functions in `/src/base/timers.hpp`  
  * The `marshallCall()` function marshalls lambda calls from a worker thread to the GUI thread. Examples of use can be seen for example in `/src/webrtcAdapter.h` and in many different places. This mechanism should not be directly needed in high-level code that runs in the GUI thread.
  * The overall client structure in `/src/chatClient.h;.cpp`
  * The intermediate layer shows how to implement `megaPostMessageToGui()`, how to start the 'services', and how to instantiate the MEGAchat client. Also shows how to implement the `getAppDir()` method, which is a weak symbol needed by the MEGAchat library in order to create the log file and start logging as early as possible, before `main()` is entered. 
  * The video module public interface in `src/rtcModile/IRtcModule.h` and related headers  

### For application implementors ###

  * The rtctestapp above is the reference app. Build it, study it, experiment with it.  
Note that there is one critical and platform-dependent function that each app that uses MEGAchat must provide, which will be referenced as `megaPostMessageToGui()`, but it can have any name, provided that the signature is `extern "C" void(void*)`. This function is the heart of the message passing mechanism (called the Gui Call Marshaller, or GCM) that MEGAchat relies on. You must pass a pointer to this function to `services_init()`.  
For more details, read the comments in base/gcm.h, and for reference implementation study rtctestapp/main.cpp
  * IRtcModule, IEventHandler in /src/IRtcModule.h. These are used to initiate rtc calls and receive events.
  * IVideoRenderer in /src/IVideoRenderer.h is used to implement video playback in arbitrary GUI environments.
    Example implementation for Qt is in examples/qtmegachatapi/videoRenderer_Qt.h;.cpp.
    The example usage can be seen from the rtctestapp application.

### If Mega API calls are required ###
  * To integrate with the environment, a simple bridge class called MyMegaApi is implemented in /src/sdkApi.h.
    Example usage of it is in /src/chatClient.cpp. 
