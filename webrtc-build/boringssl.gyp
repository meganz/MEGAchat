{
  'targets': 
  [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'sources': [],
      'all_dependent_settings': {
        'libraries': [
          '<!(echo $DEPS_SYSROOT/lib/libssl.a)',
          '<!(echo $DEPS_SYSROOT/lib/libcrypto.a)',
          '<!(echo $DEPS_SYSROOT/lib/libz.a)'
        ],
        'include_dirs': ['<!(echo $DEPS_SYSROOT/include)'],
      },
    }
  ]
}

