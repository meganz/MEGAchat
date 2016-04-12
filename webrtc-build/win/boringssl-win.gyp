{
  'targets': 
  [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'sources': [],
      'all_dependent_settings': {
          'libraries': ['/LIBPATH:"<!(echo $WEBRTC_DEPS_LIB)" ssleay32.lib libeay32.lib zlib.lib'],
          'include_dirs': ['<!(echo $WEBRTC_DEPS_INCLUDE)']
      }
    }
  ]
}

