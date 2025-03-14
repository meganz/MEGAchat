# WebRTC for Android #
We provide pre-built binaries and headers for arm32, arm64, x86 and x64. You can download them from [here](https://mega.nz/file/N2k2XRaA#bS9iudrjiULmMaGbBKErsYosELbnU22b8Zj213Ti1nE).
We strongly recommend to user the pre-built library, rather than build it by yourself. In case you want to build your own version, please, follow these steps:
* Install the [Chromium depot tools](http://dev.chromium.org/developers/how-tos/install-depot-tools)
* Download WebRTC and compile for all architectures

```
    mkdir webrtcAndroid
    cd webrtcAndroid
    fetch --nohooks webrtc_android
    cd src
    git checkout 79aff54b0fa9238ce3518dd9eaf9610cd6f22e82
    gclient sync
```

Now, you are ready to start building the library. We recommend to compile every architecture in different console in order to reset the environment variable `LD_LIBRARY_PATH`, and always use absolute paths.

### Arm 32 ###
```
    export WebRTC_output_arm32=`pwd`/out/Release-arm32
    gn gen $WebRTC_output_arm32 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm" rtc_build_examples=false rtc_build_tools=false libyuv_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false is_component_build=false android32_ndk_api_level=26 rtc_disable_logging=true'
    ninja -C $WebRTC_output_arm32
```

### Arm 64 ###
```
    export WebRTC_output_arm64=`pwd`/out/Release-arm64
    gn gen $WebRTC_output_arm64 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm64" rtc_build_examples=false rtc_build_tools=false libyuv_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false is_component_build=false android64_ndk_api_level=26 rtc_disable_logging=true'
    ninja -C $WebRTC_output_arm64
```

### x86 ###
```
    export WebRTC_output_x86=`pwd`/out/Release-x86
    gn gen $WebRTC_output_x86 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x86" rtc_build_examples=false rtc_build_tools=false libyuv_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false is_component_build=false android32_ndk_api_level=26 rtc_disable_logging=true'
    ninja -C $WebRTC_output_x86
```

### x64 ###
```
    export WebRTC_output_x86_64=`pwd`/out/Release-x86_64
    gn gen $WebRTC_output_x86_64 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x64" rtc_build_examples=false rtc_build_tools=false libyuv_include_tests=false rtc_enable_protobuf=false use_custom_libcxx=false is_component_build=false android64_ndk_api_level=26 rtc_disable_logging=true'
    ninja -C $WebRTC_output_x86_64
```

The resulting libraries `libwebrtc.a` for each platform should be located in each `<WebRTC_output_XXX>/obj`. The libraries should be copied into `<Android_Path>/android/sdk/src/main/jni/megachat/webrtc/` with a specific name for every architecture.
* `arm 32 => libwebrtc_arm.a`
* `arm 64 => libwebrtc_arm64.a`
* `x86    => libwebrtc_x86.a`
* `x64    => libwebrtc_x86_64.a`

You need to copy some folders and files (including static libraries files for all platforms, and libwebrtc.jar of one of architectures) from `<webRTCAndroid>/src` into webRTC dir in Android project as below:

```
    export MEGA_AND_PATH=path/to/webrtc/dir/into/android/project
    export WEBRTC_ORIGIN_PATH=path/to/webRTCAndroid/project/src
    mkdir $MEGA_AND_PATH
    mkdir $MEGA_AND_PATH/include
    mkdir $MEGA_AND_PATH/include/webrtc
    mkdir $MEGAANDPATH/include/third_party
    mkdir $MEGA_AND_PATH/include/webrtc/third_party
    cp $WEBRTC_ORIGIN_PATH/out/Release-arm32/obj/libwebrtc.a $MEGA_AND_PATH/libwebrtc_arm.a
    cp $WEBRTC_ORIGIN_PATH/out/Release-arm64/obj/libwebrtc.a $MEGA_AND_PATH/libwebrtc_arm64.a
    cp $WEBRTC_ORIGIN_PATH/out/Release-x86/obj/libwebrtc.a $MEGA_AND_PATH/libwebrtc_x86.a
    cp $WEBRTC_ORIGIN_PATH/out/Release-x86_64/obj/libwebrtc.a $MEGA_AND_PATH/libwebrtc_x86_64.a
    cp $WEBRTC_ORIGIN_PATH/out/Release-arm64/lib.java/sdk/android/libwebrtc.jar $MEGA_AND_PATH/libwebrtc.jar
    cp -R $WEBRTC_ORIGIN_PATH/third_party/boringssl/src/include/openssl $MEGA_AND_PATH/include/
    cp -R $WEBRTC_ORIGIN_PATH/third_party/abseil-cpp/absl $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/api $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/call $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/common_video $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/logging $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/media $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/modules $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/p2p $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/pc $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/rtc_base $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/sdk $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/system_wrappers $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTC_ORIGIN_PATH/third_party/jni_zero $MEGA_AND_PATH/include/webrtc/third_party
    cp -R $WEBRTC_ORIGIN_PATH/third_party/libyuv/include/* $MEGA_AND_PATH/include/webrtc/
    cp -R $WEBRTCORIGIN/third_party/boringssl $MEGAANDPATH/include/third_party
```

Should you have any question about the Android project, you can check https://github.com/meganz/android.
