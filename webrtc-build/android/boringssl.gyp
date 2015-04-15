{
  'targets': 
  [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'include_dirs': ['<!(echo $ANDROID_DEPS/usr/include)'],
      'sources': [],
      'all_dependent_settings': {
        'libraries': [
          '<!(echo $ANDROID_DEPS/usr/lib/libssl.a)',
          '<!(echo $ANDROID_DEPS/usr/lib/libcrypto.a)'
        ],
      },
    }
  ]
}

