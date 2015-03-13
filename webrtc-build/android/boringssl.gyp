{
  'targets': 
  [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'includes': [],
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

