# WebRTC for Android #
We provide pre-built binaries and headers for arm32, arm64, x86 and x64. You can download them from [here](https://mega.nz/file/RsMEgZqA#s0P754Ua7AqvWwamCeyrvNcyhmPjHTQQIxtqziSU4HI).
We strongly recommend to user the pre-built library, rather than build it by yourself. In case you want to build your own version, please, follow these steps:
* Install the [Chromium depot tools](http://dev.chromium.org/developers/how-tos/install-depot-tools)
* Download WebRTC and compile for all architectures

```
    mkdir webrtcAndroid
    cd webrtcAndroid
    fetch --nohooks webrtc_android
    cd src
    git checkout 93081d594f7efff72958a79251f53731b99e902b
    gclient sync
```
Before compile, you need to modify files

- `./buildtools/third_party/libc++/__config_site
```
@@ -12,7 +12,7 @@
 //    limit for PDBs (https://crbug.com/1327710#c5). To fix this, we set
 //    _LIBCPP_ABI_NAMESPACE to a shorter value.
 #define _LIBCPP_ABI_NAMESPACE Cr
-#define _LIBCPP_ABI_VERSION 2
+#define _LIBCPP_ABI_VERSION 1

 /* #undef _LIBCPP_ABI_FORCE_ITANIUM */
```

- `./buildtools/third_party/libc++/trunk/include/__config`
```
@@ -42,6 +42,8 @@
 #  define _LIBCPP_CONCAT_IMPL(_X, _Y) _X##_Y
 #  define _LIBCPP_CONCAT(_X, _Y) _LIBCPP_CONCAT_IMPL(_X, _Y)

+# define _LIBCPP_ABI_NAMESPACE _LIBCPP_CONCAT(__ndk,_LIBCPP_ABI_VERSION)
+
 // Valid C++ identifier that revs with every libc++ version. This can be used to
```

- `./buildtools/third_party/libc++/trunk/include/__assert`
```
@@ -45,7 +45,7 @@
 # define _LIBCPP_ASSERT(expression, message)                                        \
     (__builtin_expect(static_cast<bool>(expression), 1) ?                           \
       (void)0 :                                                                     \
-      ::std::__libcpp_verbose_abort("%s:%d: assertion %s failed: %s", __FILE__, __LINE__, #expression, message))
+      (void)0 )
 #elif !defined(_LIBCPP_ASSERTIONS_DISABLE_ASSUME) && __has_builtin(__builtin_assume)
 # define _LIBCPP_ASSERT(expression, message)                                        \
     (_LIBCPP_DIAGNOSTIC_PUSH                                                        \

```


Now, you are ready to start building the library. We recommend to compile every architecture in different console in order to reset the environment variable `LD_LIBRARY_PATH`, and always use absolute paths.

### Arm 32 ###
`export WebRTC_output_arm32=``pwd``/out/Release-arm32`

`gn gen $WebRTC_output_arm32 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true android32_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$WebRTC_output_arm32/clang_x64`

`ninja -C $WebRTC_output_arm32`
### Arm 64 ###
`export WebRTC_output_arm64=``pwd``/out/Release-arm64`

`gn gen $WebRTC_output_arm64 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="arm64" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true android64_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$WebRTC_output_arm64/clang_x64`

`ninja -C $WebRTC_output_arm64`
### x86 ###
`export WebRTC_output_x86=``pwd``/out/Release-x86`

`gn gen $WebRTC_output_x86 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x86" rtc_build_examples=false rtc_build_tools=false rtc_enable_protobuf=false libcxx_is_shared=true android32_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$WebRTC_output_x86/clang_x64`

`ninja -C $WebRTC_output_x86`
### x64 ###
`export WebRTC_output_x86_64=``pwd``/out/Release-x86_64`

`gn gen $WebRTC_output_x86_64 --args='treat_warnings_as_errors=false fatal_linker_warnings=false rtc_include_tests=false target_os="android" target_cpu="x64" rtc_build_examples=false rtc_build_tools=true android64_ndk_api_level=21'`

`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$WebRTC_output_x86_64/clang_x64`

`ninja -C $WebRTC_output_x86_64`

The resulting libraries `libwebrtc.a` for each platform should be located in each `<WebRTC_output_XXX>/obj`. The libraries should be copied into `<Android_Path>/android/app/src/main/jni/megachat/webrtc/` with a specific name for every architecture.
* `arm 32 => libwebrtc_arm.a`
* `arm 64 => libwebrtc_arm64.a`
* `x86    => libwebrtc_x86.a`
* `x64    => libwebrtc_x86_64.a`

You need to copy the following folders from `<webRTCAndroid>/src` as below:

  `cp -R third_party/abseil-cpp <Android_Path>/android/app/src/main/jni/megachat/webrtc/include/third_party/`  
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
  
Furthermore, you should copy the library, libwebrtc.jar. This jar is located in `<WebRTC_output_XXX>/lib.java/sdk/android/`. You can take one of four `WebRTC_output_XXX` (arm32, arm64, x64 and x86_64) because it is present in all and it is the same
  `cp <WebRTC_output_XXX>/lib.java/sdk/android/libwebrtc.jar <Android_Path>/android/app/src/main/jni/megachat/webrtc/libwebrtc.jar`
 
Should you have any question about the Android project, you can check https://github.com/meganz/android.
