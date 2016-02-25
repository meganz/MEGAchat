{
  'variables': {
     'have_depdirs':
         '<!(if [ ! -z "$WEBRTC_DEPS_INCLUDE" ] || [ ! -z "$WEBRTC_DEPS_LIB" ]; then echo "1"; else echo "0"; fi)'
  },
  'targets': 
  [
    {
      'target_name': 'boringssl',
      'type': '<(component)',
      'sources': [],
      'all_dependent_settings': {
          'libraries': ['-lssl -lcrypto -lz']
      },
      'conditions': [
        ['have_depdirs==1', {
            'all_dependent_settings': {
                'libraries': ['-L<!(echo $WEBRTC_DEPS_LIB)'],
                'include_dirs': ['<!(echo $WEBRTC_DEPS_INCLUDE)'],
             }
        }]
      ]
    }
  ]
}

