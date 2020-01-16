# WebRTC for Android #
We provide the library and headers pre-compiled for arm32, arm64, x86 and x64. You can download them from here https://mega.nz/#!wixgSaZZ!6zRMV_d8ogouBaEidHzGws1KvLrBwBiKEm0VIVgXEPk.
We recommend you that use this pre-compiled version, but if you want to do it yourself, you can follow this steps:
* Install the Chromium depot tools (http://dev.chromium.org/developers/how-tos/install-depot-tools)
* Download webRTC and compile for all architectures
`mkdir webrtcAndroid`
`cd webrtcAndroid`
`fetch --nohooks webrtcAndroid`
`cd src`
`git checkout 9863f3d246e2da7a2e1f42bbc5757f6af5ec5682`    (branch-heads/m76)
`gclient sync`
Before compile, you need to modify the file `buildtools/third_party/libc++/trunk/include/__config`
`
# define _LIBCPP_ABI_NAMESPACE 
- _LIBCPP_CONCAT(__,_LIBCPP_ABI_VERSION)
+ _LIBCPP_CONCAT(__ndk,_LIBCPP_ABI_VERSION)
`
Now, you are ready to start compilation process. (we recomend compile every architecture in different console to restart LD_LIBRARY_PATH variable)
### Arm 32 ###
`gn gen <WebRTC_output_arm32> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false'`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_arm32>/clang_x64`
`ninja -C <WebRTC_output_arm32>`
### Arm 64 ###
`gn gen <WebRTC_output_arm64> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm64" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false'`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_arm64>/clang_x64`
`ninja -C <WebRTC_output_arm64>`
### x86 ###
`gn gen <WebRTC_output_x86> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x86" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true libcxx_abi_unstable=false'`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_x86>/clang_x64`
`ninja -C <WebRTC_output_x86>`
### x64 ###
`gn gen <WebRTC_output_x64> --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_bseil-cpptests=false target_os="android" target_cpu="x64" rtc_build_examples=false rtc_build_tools=true'`
`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:<WebRTC_output_x64>/clang_x64`
`ninja -C <WebRTC_output_x64>`

The resulting libraries `libwebrtc.a` for each platform should be located in each `<WebRTC_output_XXX>/obj`. The libraries should be copy to <Android_Path>/android2/app/src/main/jni/megachat/webrtc/ with a specific name for every architecture
* `arm 32 => libwebrtc_arm.a`
* `arm 64 => libwebrtc_arm.a`
* `x86    => libwebrtc_x86.a`
* `x64    => libwebrtc_x86_64.a`
Furthermore, you have to copy from `src` next directories: 
  `cp -R third_party/abseil-cpp <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/third_party/
  cp -R third_party/boringssl <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/third_party/
  cp -R third_party/libyuv <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/third_party/
  cp -R api <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R base <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R common_audio <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R data <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R media <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R rtc_base <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R stats <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R video <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R audio <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R call <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R crypto <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R modules <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R pc <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R sdk <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp -R system_wrappers <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/
  cp common_types.h <Android_Path>/android2/app/src/main/jni/megachat/webrtc/include/webrtc/`
 
 If you have some doubt about Android project, you can review https://github.com/meganz/android2
