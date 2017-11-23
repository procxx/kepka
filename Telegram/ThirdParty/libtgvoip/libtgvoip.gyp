[
  '"<(OS)" == "mac"', {
    'xcode_settings': {
      'CLANG_CXX_LANGUAGE_STANDARD': 'c++1z',
    },
    'conditions': [
      [ '"<(official_build_target)" == "mac32"', {
        'xcode_settings': {
          'MACOSX_DEPLOYMENT_TARGET': '10.6',
          'OTHER_CPLUSPLUSFLAGS': [ '-nostdinc++' ],
        },
        'include_dirs': [
          '/usr/local/macold/include/c++/v1',
          '<(DEPTH)/../../../Libraries/macold/openssl/include',
        ],
      }, {
        'xcode_settings': {
          'MACOSX_DEPLOYMENT_TARGET': '10.8',
          'CLANG_CXX_LIBRARY': 'libc++',
        },
        'include_dirs': [
          '<(DEPTH)/../../../Libraries/openssl/include',
        ],
      }]
    ]
  },
],
[
  '"<(OS)" == "win"', {
    'msbuild_toolset': 'v141',
    'libraries': [
      'winmm',
      'ws2_32',
      'kernel32',
      'user32',
    ],
    'msvs_cygwin_shell': 0,
    'msvs_settings': {
      'VCCLCompilerTool': {
        'ProgramDataBaseFileName': '$(OutDir)\\$(ProjectName).pdb',
        'DebugInformationFormat': '3',          # Program Database (/Zi)
        'AdditionalOptions': [
          '/MP',   # Enable multi process build.
          '/EHsc', # Catch C++ exceptions only, extern C functions never throw a C++ exception.
          '/wd4068', # Disable "warning C4068: unknown pragma"
        ],
        'TreatWChar_tAsBuiltInType': 'false',
      },
    },
    'msvs_external_builder_build_cmd': [
      'ninja.exe',
      '-C',
      '$(OutDir)',
      '-k0',
      '$(ProjectName)',
    ],
    'configurations': {
      'Debug': {
        'defines': [
          '_DEBUG',
        ],
        'include_dirs': [
          '<(DEPTH)/../../../Libraries/openssl/Debug/include',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '0',                # Disabled (/Od)
            'RuntimeLibrary': '1',              # Multi-threaded Debug (/MTd)
            'RuntimeTypeInfo': 'true',
          },
          'VCLibrarianTool': {
            'AdditionalOptions': [
              '/NODEFAULTLIB:LIBCMT'
            ]
          }
        },
      },
      'Release': {
        'defines': [
          'NDEBUG',
        ],
        'include_dirs': [
           '<(DEPTH)/../../../Libraries/openssl/Release/include',
        ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'Optimization': '2',                 # Maximize Speed (/O2)
            'InlineFunctionExpansion': '2',      # Any suitable (/Ob2)
            'EnableIntrinsicFunctions': 'true',  # Yes (/Oi)
            'FavorSizeOrSpeed': '1',             # Favor fast code (/Ot)
            'RuntimeLibrary': '0',               # Multi-threaded (/MT)
            'EnableEnhancedInstructionSet': '2', # Streaming SIMD Extensions 2 (/arch:SSE2)
            'WholeProgramOptimization': 'true',  # /GL
          },
          'VCLibrarianTool': {
            'AdditionalOptions': [
              '/LTCG',
            ]
          },
        },
      },
    },
  },
],
