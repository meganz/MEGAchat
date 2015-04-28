function wrbase() {
  export GYP_DEFINES="build_with_libjingle=1 build_with_chromium=0 libjingle_objc=1 clang_use_chrome_plugins=0 use_system_libcxx=1 use_openssl=1 use_nss=0"
  export GYP_GENERATORS="ninja"
}

function wrios() {
  wrbase
  export GYP_DEFINES="$GYP_DEFINES OS=ios target_arch=armv7"
  export GYP_CROSSCOMPILE=1
}

function wrsim() {
  wrbase
  export GYP_DEFINES="$GYP_DEFINES OS=ios target_arch=ia32"
  export GYP_CROSSCOMPILE=1
}

#- To build & sign the sample app for an iOS device:
  wrios && gclient runhooks --force && ninja -C out/Release-iphoneos AppRTCDemo
#  wrsim && gclient runhooks --force && ninja -C out/Release-iphonesimulator AppRTCDemo

#- To install the sample app on an iOS device:
#  ideviceinstaller -i out_ios/Debug-iphoneos/AppRTCDemo.app
#  (if installing ideviceinstaller from brew, use --HEAD to get support
#  for .app directories)
#- Alternatively, use iPhone Configuration Utility:
#  - Open "iPhone Configuration Utility" (http://support.apple.com/kb/DL1465)
#  - Click the "Add" icon (command-o)
#  - Open the app under out_ios/Debug-iphoneos/AppRTCDemo (should be added to the Applications tab)
#  - Click the device's name in the left-hand panel and select the Applications tab
#  - Click Install on the AppRTCDemo line.
#      (If you have any problems deploying for the first time, check
#      the Info.plist file to ensure that the Bundle Identifier matches
#      your phone provisioning profile, or use a development wildcard
#      provisioning profile.)
#- Alternately, use ios-deploy:
#  ios-deploy -d -b out_ios/Debug-iphoneos/AppRTCDemo.app

#- Once installed:
#  - Tap AppRTCDemo on the iOS device's home screen (might have to scroll to find it).
#  - In desktop chrome, navigate to http://apprtc.appspot.com and note
#    the r=<NNN> room number in the resulting URL; enter that number
#    into the text field on the phone.
