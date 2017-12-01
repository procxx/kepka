# This file is part of Telegram Desktop,
# the official desktop version of Telegram messaging app, see https://telegram.org
#
# Telegram Desktop is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# It is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# In addition, as a special exception, the copyright holders give permission
# to link the code of portions of this program with the OpenSSL library.
#
# Full license: https://github.com/telegramdesktop/tdesktop/blob/master/LICENSE
# Copyright (c) 2014 John Preston, https://desktop.telegram.org

{
  'conditions': [[ 'build_linux', {
    'variables': {
      'not_need_gtk%': '<!(python -c "print(\'TDESKTOP_DISABLE_GTK_INTEGRATION\' in \'<(build_defines)\')")',
      'pkgconfig_libs': [
# In order to work libxkbcommon must be linked statically,
# PKGCONFIG links it like "-L/usr/local/lib -lxkbcommon"
# which makes a dynamic link which leads to segfault in
# QApplication() -> createPlatformIntegration -> QXcbIntegrationPlugin::create
        #'xkbcommon',
      ],
    },
    'include_dirs': [
      '<!(rpm --eval "%{_includedir}")',
      '<!(rpm --eval "%{_includedir}")/ffmpeg',
      '<!(rpm --eval "%{_includedir}")/minizip',
      '<!(rpm --eval "%{_includedir}")/opus',
      '<!(rpm --eval "%{_includedir}")/libtgvoip',
    ],
    'library_dirs': [
+      '<!(rpm --eval "%{_libdir}")',
    ],
    'libraries': [
      'openal',
      'avformat',
      'avcodec',
      'swresample',
      'swscale',
      'avutil',
      'z',
      'minizip',
      'tgvoip',
      'webp',
#      '<!(pkg-config 2> /dev/null --libs <@(pkgconfig_libs))',
    ],
    'conditions': [['not_need_gtk!="True"', {
        'cflags_cc': [
            '<!(pkg-config 2> /dev/null --cflags appindicator-0.1)',
            '<!(pkg-config 2> /dev/null --cflags gtk+-3.0)',
            '<!(pkg-config 2> /dev/null --cflags dee-1.0)',
         ],
    }]],
    'configurations': {
      'Release': {
        'cflags': [
          '-flto',
          '-fno-strict-aliasing',
        ],
        'cflags_cc': [
          '-flto',
          '-fno-strict-aliasing',
        ],
        'ldflags': [
        ],
      },
    },
    'cmake_precompiled_header': '<(src_loc)/stdafx.h',
    'cmake_precompiled_header_script': 'PrecompiledHeader.cmake',
  }]],
}
