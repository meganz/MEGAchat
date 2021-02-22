# WebRTC for Android #
We provide pre-built binaries and headers for arm32, arm64, x86 and x64. You can download them from [here](https://mega.nz/file/1qQTUIzY#Q3euQPnLZ5jpJCOE3GgNUfXp4Xw7nuuE_BG2eX73byI).
We strongly recommend to user the pre-built library, rather than build it by yourself. In case you want to build your own version, please, follow these steps:
* Install the [Chromium depot tools](http://dev.chromium.org/developers/how-tos/install-depot-tools)
* Download WebRTC and compile for all architectures

```
    mkdir webrtcAndroid
    cd webrtcAndroid
    fetch --nohooks webrtcAndroid
    cd src
    git checkout 41bfcf4a63611409220fcd458a03deaa2cd23619`    (branch-heads/4405)
    gclient sync
```
Before compile, you need to modify the file `buildtools/third_party/libc++/trunk/include/__config`

```
@@ -130 +130 @@
# define _LIBCPP_ABI_NAMESPACE 
- _LIBCPP_CONCAT(__,_LIBCPP_ABI_VERSION)
+ _LIBCPP_CONCAT(__ndk,_LIBCPP_ABI_VERSION)
#endif
```

Now, you are ready to start building the library. We recommend to compile every architecture in different console in order to reset the environment variable `LD_LIBRARY_PATH`.

### Arm 32 ###
`gn gen <WebRTC_output_arm32> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false android32_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_arm32>/clang_x64`

`ninja -C <WebRTC_output_arm32>`
### Arm 64 ###
`gn gen <WebRTC_output_arm64> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm64" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false android64_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_arm64>/clang_x64`

`ninja -C <WebRTC_output_arm64>`
### x86 ###
`gn gen <WebRTC_output_x86> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x86" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false android32_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_x86>/clang_x64`

`ninja -C <WebRTC_output_x86>`
### x64 ###
`gn gen <WebRTC_output_x64> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x64" rtc_build_examples=false rtc_build_tools=true android64_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_x64>/clang_x64`

`ninja -C <WebRTC_output_x64>`

The resulting libraries `libwebrtc.a` for each platform should be located in each `<WebRTC_output_XXX>/obj`. The libraries should be copied into `<Android_Path>/android2/app/src/main/jni/megachat/webrtc/` with a specific name for every architecture.
* `arm 32 => libwebrtc_arm.a`
* `arm 64 => libwebrtc_arm64.a`
* `x86    => libwebrtc_x86.a`
* `x64    => libwebrtc_x86_64.a`

Furthermore, you need to copy the following folders from `<webRTCAndroid>/src` as below:

  `cp -R third_party/abseil-cpp <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R third_party/boringssl <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/third_party/`  
  `cp -R third_party/libyuv <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/third_party/`  
  `cp -R api <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R base <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R common_audio <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R common_video <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R data <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R logging <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R media <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R rtc_base <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R stats <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R video <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R audio <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R call <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R modules <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R p2p <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R pc <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R sdk <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
  `cp -R system_wrappers <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/webrtc/`  
 
Should you have any question about the Android project, you can check https://github.com/meganz/android.
