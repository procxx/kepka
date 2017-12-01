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
  'includes': [
    'common.gypi',
  ],
  'targets': [{
    'target_name': 'Telegram',
    'variables': {
      'variables': {
        'libs_loc': '../../../Libraries',
      },
      'libs_loc': '<(libs_loc)',
      'src_loc': '../SourceFiles',
      'res_loc': '../Resources',
      'submodules_loc': '../ThirdParty',
      'sp_media_key_tap_loc': '<(submodules_loc)/SPMediaKeyTap',
      'emoji_suggestions_loc': '<(submodules_loc)/emoji_suggestions',
      'style_files': [
        '<(res_loc)/colors.palette',
        '<(res_loc)/basic.style',
        '<(src_loc)/boxes/boxes.style',
        '<(src_loc)/calls/calls.style',
        '<(src_loc)/dialogs/dialogs.style',
        '<(src_loc)/history/history.style',
        '<(src_loc)/intro/intro.style',
        '<(src_loc)/media/view/mediaview.style',
        '<(src_loc)/media/player/media_player.style',
        '<(src_loc)/overview/overview.style',
        '<(src_loc)/profile/profile.style',
        '<(src_loc)/settings/settings.style',
        '<(src_loc)/chat_helpers/chat_helpers.style',
        '<(src_loc)/ui/widgets/widgets.style',
        '<(src_loc)/window/window.style',
      ],
      'langpacks': [
        'en',
        'de',
        'es',
        'it',
        'nl',
        'ko',
        'pt-BR',
      ],
      'build_defines%': '',
      'list_sources_command': 'python <(DEPTH)/list_sources.py --input <(DEPTH)/telegram_sources.txt --replace src_loc=<(src_loc)',
    },
    'includes': [
      'common_executable.gypi',
      'telegram_qrc.gypi',
      'telegram_win.gypi',
      'telegram_mac.gypi',
      'telegram_linux.gypi',
      'qt.gypi',
      'qt_moc.gypi',
      'qt_rcc.gypi',
      'codegen_rules.gypi',
    ],

    'dependencies': [
      'codegen.gyp:codegen_emoji',
      'codegen.gyp:codegen_lang',
      'codegen.gyp:codegen_numbers',
      'codegen.gyp:codegen_style',
    ],

    'defines': [
      'TGVOIP_USE_CXX11_LIB',
      '__STDC_FORMAT_MACROS',
      'TDESKTOP_DISABLE_AUTOUPDATE',
      'TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME',
      'TDESKTOP_DISABLE_DESKTOP_FILE_GENERATION',
      'TDESKTOP_DISABLE_CRASH_REPORTS',
      'TDESKTOP_DISABLE_UNITY_INTEGRATION',
    ],

    'include_dirs': [
      '<(src_loc)',
      '<(SHARED_INTERMEDIATE_DIR)',
      '<(sp_media_key_tap_loc)',
      '<(emoji_suggestions_loc)',
      '<(submodules_loc)/GSL/include',
    ],
    'sources': [
      '<@(qrc_files)',
      '<@(style_files)',
      '<!@(<(list_sources_command) <(qt_moc_list_sources_arg))',
    ],
    'sources!': [
      '<!@(<(list_sources_command) <(qt_moc_list_sources_arg) --exclude_for <(build_os))',
    ],
    'conditions': [
      [ '"<(official_build_target)" != ""', {
        'defines': [
          'CUSTOM_API_ID',
        ],
        'dependencies': [
          'utils.gyp:Packer',
        ],
      }],
    ],
  }],
}
