# defines
if (LINUX)
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DQ_OS_LINUX64)
    else()
        add_definitions(-DQ_OS_LINUX32)
    endif()
endif()

##================================================
## API tokens
##================================================
if (DEFINED ENV{API_ID} AND DEFINED ENV{API_HASH})
    set(API_ID ENV{API_ID})
    set(API_HASH ENV{API_HASH})
else()
    message(WARNING "No custom API tokens detected. You must grab your own tokens from https://core.telegram.org/api/obtaining_api_id and export them using environment options. Will use default for testing purposes.")
    set(API_ID 17349)
    set(API_HASH "344583e45741c457fe1862106095a5eb")
endif()

#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -H")
set(CMAKE_INCLUDE_CURRENT_DIR ON)
##================================================
## Codegen Tools
##================================================
include_directories(SourceFiles)
add_subdirectory(SourceFiles/codegen)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/styles)

add_custom_command(
    COMMENT "Generating palette"
    OUTPUT
        styles/palette.h
        styles/palette.cpp
    COMMAND
        codegen_style -I${CMAKE_CURRENT_SOURCE_DIR}/Resources -I${CMAKE_CURRENT_SOURCE_DIR}
        -o${CMAKE_CURRENT_BINARY_DIR}/styles -w${CMAKE_SOURCE_DIR}
        colors.palette
    WORKING_DIRECTORY styles
    DEPENDS
        codegen_style
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/colors.palette
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/colors.palette
)
add_custom_target(palette_output
    DEPENDS styles/palette.h styles/palette.cpp)

add_custom_command(
    COMMENT "Generating numbers"
    OUTPUT
        numbers.h
        numbers.cpp
    COMMAND
        codegen_numbers -o${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
    WORKING_DIRECTORY .
    DEPENDS
        codegen_numbers
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
)
add_custom_target(numbers_output
    DEPENDS numbers.h numbers.cpp)

add_custom_command(
    COMMENT "Generating langs"
    OUTPUT
        lang_auto.h
        lang_auto.cpp
    COMMAND
        codegen_lang -o${CMAKE_CURRENT_BINARY_DIR} -w${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
    WORKING_DIRECTORY .
    DEPENDS
        codegen_lang
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
)

add_custom_target(lang_output
    DEPENDS lang_auto.h lang_auto.cpp)

add_custom_command(
    COMMENT "Generating emoji"
    OUTPUT
        emoji.cpp
        emoji.h
        emoji_suggestions_data.cpp
        emoji_suggestions_data.h
    COMMAND
        codegen_emoji ${CMAKE_CURRENT_SOURCE_DIR}/Resources/emoji_autocomplete.json
        -o ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY .
    DEPENDS
        codegen_emoji
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/emoji_autocomplete.json
    VERBATIM
)
add_custom_target(emoji_output
    DEPENDS
        emoji.h
        emoji.cpp
        emoji_suggestions_data.cpp
        emoji_suggestions_data.h
)

add_custom_command(
    COMMENT "Generating scheme"
    OUTPUT
        scheme.cpp
        scheme.h
    COMMAND
        ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/SourceFiles/codegen/scheme/codegen_scheme.py
        -o${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    WORKING_DIRECTORY .
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/SourceFiles/codegen/scheme/codegen_scheme.py
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    MAIN_DEPENDENCY
        ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    VERBATIM
)
add_custom_target(scheme_output
    DEPENDS scheme.h scheme.cpp)

##================================================

list(APPEND style_files
    Resources/basic
    SourceFiles/boxes/boxes
    SourceFiles/calls/calls
    SourceFiles/chat_helpers/chat_helpers
    SourceFiles/dialogs/dialogs
    SourceFiles/history/history
    SourceFiles/intro/intro
    SourceFiles/media/player/media_player
    SourceFiles/media/view/mediaview
    SourceFiles/overview/overview
    SourceFiles/profile/profile
    SourceFiles/settings/settings
    SourceFiles/ui/widgets/widgets
    SourceFiles/window/window
)

foreach (src ${style_files})
    get_filename_component(src_file ${src} NAME)
    add_custom_command(
        COMMENT "Generating ${src_file}"
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.h
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp
        COMMAND
            codegen_style -I${CMAKE_CURRENT_SOURCE_DIR}/Resources -I${CMAKE_CURRENT_SOURCE_DIR}
            -I${CMAKE_CURRENT_SOURCE_DIR}/SourceFiles
            -o${CMAKE_CURRENT_BINARY_DIR}/styles -w${CMAKE_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style
        DEPENDS
            codegen_style
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style
        MAIN_DEPENDENCY
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style
    )
    add_custom_target(${src_file}_styles_output
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.h
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp
    )

    list(APPEND style_sources
        ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp)

endforeach()

##================================================
## Branding
##================================================
configure_file(SourceFiles/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

##================================================
## Kepka app
##================================================
add_executable(Kepka WIN32 MACOSX_BUNDLE
    ${CMAKE_CURRENT_BINARY_DIR}/styles/palette.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/numbers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lang_auto.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/scheme.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/emoji.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/emoji_suggestions_data.cpp
    ${style_sources}

    SourceFiles/base/observer.cpp
    SourceFiles/base/parse_helper.cpp
    SourceFiles/base/qthelp_url.cpp
    SourceFiles/base/runtime_composer.cpp
    SourceFiles/base/task_queue.cpp
    SourceFiles/base/timer.cpp

    SourceFiles/boxes/about_box.cpp
    SourceFiles/boxes/abstract_box.cpp
    SourceFiles/boxes/add_contact_box.cpp
    SourceFiles/boxes/autolock_box.cpp
    SourceFiles/boxes/background_box.cpp
    SourceFiles/boxes/calendar_box.cpp
    SourceFiles/boxes/change_phone_box.cpp
    SourceFiles/boxes/confirm_box.cpp
    SourceFiles/boxes/confirm_phone_box.cpp
    SourceFiles/boxes/connection_box.cpp
    SourceFiles/boxes/download_path_box.cpp
    SourceFiles/boxes/edit_color_box.cpp
    SourceFiles/boxes/edit_participant_box.cpp
    SourceFiles/boxes/edit_privacy_box.cpp
    SourceFiles/boxes/language_box.cpp
    SourceFiles/boxes/local_storage_box.cpp
    SourceFiles/boxes/mute_settings_box.cpp
    SourceFiles/boxes/notifications_box.cpp
    SourceFiles/boxes/passcode_box.cpp
    SourceFiles/boxes/peer_list_box.cpp
    SourceFiles/boxes/peer_list_controllers.cpp
    SourceFiles/boxes/photo_crop_box.cpp
    SourceFiles/boxes/rate_call_box.cpp
    SourceFiles/boxes/report_box.cpp
    SourceFiles/boxes/self_destruction_box.cpp
    SourceFiles/boxes/send_files_box.cpp
    SourceFiles/boxes/sessions_box.cpp
    SourceFiles/boxes/share_box.cpp
    SourceFiles/boxes/sticker_set_box.cpp
    SourceFiles/boxes/stickers_box.cpp
    SourceFiles/boxes/username_box.cpp

    SourceFiles/calls/calls_box_controller.cpp
    SourceFiles/calls/calls_call.cpp
    SourceFiles/calls/calls_emoji_fingerprint.cpp
    SourceFiles/calls/calls_instance.cpp
    SourceFiles/calls/calls_panel.cpp
    SourceFiles/calls/calls_top_bar.cpp

    SourceFiles/chat_helpers/bot_keyboard.cpp
    SourceFiles/chat_helpers/emoji_list_widget.cpp
    SourceFiles/chat_helpers/emoji_suggestions_widget.cpp
    SourceFiles/chat_helpers/field_autocomplete.cpp
    SourceFiles/chat_helpers/gifs_list_widget.cpp
    SourceFiles/chat_helpers/message_field.cpp
    SourceFiles/chat_helpers/stickers.cpp
    SourceFiles/chat_helpers/stickers_list_widget.cpp
    SourceFiles/chat_helpers/tabbed_panel.cpp
    SourceFiles/chat_helpers/tabbed_section.cpp
    SourceFiles/chat_helpers/tabbed_selector.cpp

    SourceFiles/core/click_handler.cpp
    SourceFiles/core/click_handler_types.cpp
    SourceFiles/core/file_utilities.cpp
    SourceFiles/core/single_timer.cpp
    SourceFiles/core/utils.cpp

    SourceFiles/data/data_abstract_structure.cpp
    SourceFiles/data/data_drafts.cpp
    SourceFiles/data/data_game.cpp
    SourceFiles/data/data_photo.cpp

    SourceFiles/dialogs/dialogs_indexed_list.cpp
    SourceFiles/dialogs/dialogs_inner_widget.cpp
    SourceFiles/dialogs/dialogs_layout.cpp
    SourceFiles/dialogs/dialogs_list.cpp
    SourceFiles/dialogs/dialogs_row.cpp
    SourceFiles/dialogs/dialogs_search_from_controllers.cpp
    SourceFiles/dialogs/dialogs_widget.cpp

    SourceFiles/history/history.cpp
    SourceFiles/history/history_admin_log_filter.cpp
    SourceFiles/history/history_admin_log_inner.cpp
    SourceFiles/history/history_admin_log_item.cpp
    SourceFiles/history/history_admin_log_section.cpp
    SourceFiles/history/history_drag_area.cpp
    SourceFiles/history/history_inner_widget.cpp
    SourceFiles/history/history_item.cpp
    SourceFiles/history/history_location_manager.cpp
    SourceFiles/history/history_media_types.cpp
    SourceFiles/history/history_message.cpp
    SourceFiles/history/history_service.cpp
    SourceFiles/history/history_service_layout.cpp
    SourceFiles/history/history_widget.cpp

    SourceFiles/inline_bots/inline_bot_layout_internal.cpp
    SourceFiles/inline_bots/inline_bot_layout_item.cpp
    SourceFiles/inline_bots/inline_bot_result.cpp
    SourceFiles/inline_bots/inline_bot_send_data.cpp
    SourceFiles/inline_bots/inline_results_widget.cpp

    SourceFiles/intro/introcode.cpp
    SourceFiles/intro/introphone.cpp
    SourceFiles/intro/intropwdcheck.cpp
    SourceFiles/intro/introsignup.cpp
    SourceFiles/intro/introstart.cpp
    SourceFiles/intro/introwidget.cpp

    SourceFiles/lang/lang_cloud_manager.cpp
    SourceFiles/lang/lang_file_parser.cpp
    SourceFiles/lang/lang_instance.cpp
    SourceFiles/lang/lang_keys.cpp
    SourceFiles/lang/lang_tag.cpp
    SourceFiles/lang/lang_translator.cpp

    SourceFiles/media/player/media_player_button.cpp
    SourceFiles/media/player/media_player_cover.cpp
    SourceFiles/media/player/media_player_float.cpp
    SourceFiles/media/player/media_player_instance.cpp
    SourceFiles/media/player/media_player_list.cpp
    SourceFiles/media/player/media_player_panel.cpp
    SourceFiles/media/player/media_player_volume_controller.cpp
    SourceFiles/media/player/media_player_widget.cpp

    SourceFiles/media/view/media_clip_controller.cpp
    SourceFiles/media/view/media_clip_playback.cpp
    SourceFiles/media/view/media_clip_volume_controller.cpp

    SourceFiles/media/media_audio.cpp
    SourceFiles/media/media_audio_capture.cpp
    SourceFiles/media/media_audio_ffmpeg_loader.cpp
    SourceFiles/media/media_audio_loader.cpp
    SourceFiles/media/media_audio_loaders.cpp
    SourceFiles/media/media_audio_track.cpp
    SourceFiles/media/media_child_ffmpeg_loader.cpp
    SourceFiles/media/media_clip_ffmpeg.cpp
    SourceFiles/media/media_clip_implementation.cpp
    SourceFiles/media/media_clip_qtgif.cpp
    SourceFiles/media/media_clip_reader.cpp

    SourceFiles/mtproto/auth_key.cpp
    SourceFiles/mtproto/config_loader.cpp
    SourceFiles/mtproto/connection.cpp
    SourceFiles/mtproto/connection_abstract.cpp
    SourceFiles/mtproto/connection_auto.cpp
    SourceFiles/mtproto/connection_http.cpp
    SourceFiles/mtproto/connection_tcp.cpp
    SourceFiles/mtproto/core_types.cpp
    SourceFiles/mtproto/dc_options.cpp
    SourceFiles/mtproto/dcenter.cpp
    SourceFiles/mtproto/facade.cpp
    SourceFiles/mtproto/mtp_instance.cpp
    SourceFiles/mtproto/rpc_sender.cpp
    SourceFiles/mtproto/rsa_public_key.cpp
    SourceFiles/mtproto/session.cpp
    SourceFiles/mtproto/special_config_request.cpp
    SourceFiles/mtproto/type_utils.cpp

    SourceFiles/overview/overview_layout.cpp

    SourceFiles/profile/profile_back_button.cpp
    SourceFiles/profile/profile_block_actions.cpp
    SourceFiles/profile/profile_block_channel_members.cpp
    SourceFiles/profile/profile_block_group_members.cpp
    SourceFiles/profile/profile_block_info.cpp
    SourceFiles/profile/profile_block_invite_link.cpp
    SourceFiles/profile/profile_block_peer_list.cpp
    SourceFiles/profile/profile_block_settings.cpp
    SourceFiles/profile/profile_block_shared_media.cpp
    SourceFiles/profile/profile_block_widget.cpp
    SourceFiles/profile/profile_channel_controllers.cpp
    SourceFiles/profile/profile_common_groups_section.cpp
    SourceFiles/profile/profile_cover.cpp
    SourceFiles/profile/profile_cover_drop_area.cpp
    SourceFiles/profile/profile_fixed_bar.cpp
    SourceFiles/profile/profile_inner_widget.cpp
    SourceFiles/profile/profile_section_memento.cpp
    SourceFiles/profile/profile_userpic_button.cpp
    SourceFiles/profile/profile_widget.cpp

    SourceFiles/settings/settings_advanced_widget.cpp
    SourceFiles/settings/settings_background_widget.cpp
    SourceFiles/settings/settings_block_widget.cpp
    SourceFiles/settings/settings_chat_settings_widget.cpp
    SourceFiles/settings/settings_cover.cpp
    SourceFiles/settings/settings_fixed_bar.cpp
    SourceFiles/settings/settings_general_widget.cpp
    SourceFiles/settings/settings_info_widget.cpp
    SourceFiles/settings/settings_inner_widget.cpp
    SourceFiles/settings/settings_layer.cpp
    SourceFiles/settings/settings_notifications_widget.cpp
    SourceFiles/settings/settings_privacy_controllers.cpp
    SourceFiles/settings/settings_privacy_widget.cpp
    SourceFiles/settings/settings_scale_widget.cpp
    SourceFiles/settings/settings_widget.cpp

    SourceFiles/storage/file_download.cpp
    SourceFiles/storage/file_upload.cpp
    SourceFiles/storage/localimageloader.cpp
    SourceFiles/storage/localstorage.cpp
    SourceFiles/storage/serialize_common.cpp
    SourceFiles/storage/serialize_document.cpp

    SourceFiles/ui/effects/cross_animation.cpp
    SourceFiles/ui/effects/panel_animation.cpp
    SourceFiles/ui/effects/radial_animation.cpp
    SourceFiles/ui/effects/ripple_animation.cpp
    SourceFiles/ui/effects/round_checkbox.cpp
    SourceFiles/ui/effects/send_action_animations.cpp
    SourceFiles/ui/effects/slide_animation.cpp
    SourceFiles/ui/effects/widget_fade_wrap.cpp
    SourceFiles/ui/effects/widget_slide_wrap.cpp

    SourceFiles/ui/style/style_core.cpp
    SourceFiles/ui/style/style_core_color.cpp
    SourceFiles/ui/style/style_core_font.cpp
    SourceFiles/ui/style/style_core_icon.cpp
    SourceFiles/ui/style/style_core_types.cpp

    SourceFiles/ui/text/text.cpp
    SourceFiles/ui/text/text_block.cpp
    SourceFiles/ui/text/text_entity.cpp

    SourceFiles/ui/toast/toast.cpp
    SourceFiles/ui/toast/toast_manager.cpp
    SourceFiles/ui/toast/toast_widget.cpp

    SourceFiles/ui/widgets/buttons.cpp
    SourceFiles/ui/widgets/checkbox.cpp
    SourceFiles/ui/widgets/continuous_sliders.cpp
    SourceFiles/ui/widgets/discrete_sliders.cpp
    SourceFiles/ui/widgets/dropdown_menu.cpp
    SourceFiles/ui/widgets/inner_dropdown.cpp
    SourceFiles/ui/widgets/input_fields.cpp
    SourceFiles/ui/widgets/labels.cpp
    SourceFiles/ui/widgets/menu.cpp
    SourceFiles/ui/widgets/multi_select.cpp
    SourceFiles/ui/widgets/popup_menu.cpp
    SourceFiles/ui/widgets/scroll_area.cpp
    SourceFiles/ui/widgets/shadow.cpp
    SourceFiles/ui/widgets/tooltip.cpp

    SourceFiles/ui/abstract_button.cpp
    SourceFiles/ui/animation.cpp
    SourceFiles/ui/countryinput.cpp
    SourceFiles/ui/emoji_config.cpp
    SourceFiles/ui/images.cpp
    SourceFiles/ui/special_buttons.cpp
    SourceFiles/ui/twidget.cpp

    SourceFiles/window/themes/window_theme.cpp
    SourceFiles/window/themes/window_theme_editor.cpp
    SourceFiles/window/themes/window_theme_editor_block.cpp
    SourceFiles/window/themes/window_theme_preview.cpp
    SourceFiles/window/themes/window_theme_warning.cpp

    SourceFiles/window/main_window.cpp
    SourceFiles/window/notifications_manager.cpp
    SourceFiles/window/notifications_manager_default.cpp
    SourceFiles/window/notifications_utilities.cpp
    SourceFiles/window/player_wrap_widget.cpp
    SourceFiles/window/section_widget.cpp
    SourceFiles/window/top_bar_widget.cpp
    SourceFiles/window/window_controller.cpp
    SourceFiles/window/window_main_menu.cpp
    SourceFiles/window/window_slide_animation.cpp

    SourceFiles/apiwrap.cpp
    SourceFiles/app.cpp
    SourceFiles/application.cpp
    SourceFiles/auth_session.cpp
    SourceFiles/config.cpp
    SourceFiles/facades.cpp
    SourceFiles/layerwidget.cpp
    SourceFiles/layout.cpp
    SourceFiles/logs.cpp
    SourceFiles/main.cpp
    SourceFiles/mainwidget.cpp
    SourceFiles/mainwindow.cpp
    SourceFiles/mediaview.cpp
    SourceFiles/messenger.cpp
    SourceFiles/observer_peer.cpp
    SourceFiles/overviewwidget.cpp
    SourceFiles/passcodewidget.cpp
    SourceFiles/qt_functions.cpp
    SourceFiles/settings.cpp
    SourceFiles/shortcuts.cpp
    SourceFiles/structs.cpp
)

if (APPLE)
    target_sources(Kepka PRIVATE
        SourceFiles/platform/mac/file_utilities_mac.mm
        SourceFiles/platform/mac/mac_utilities.mm
        SourceFiles/platform/mac/main_window_mac.mm
        SourceFiles/platform/mac/notifications_manager_mac.mm
        SourceFiles/platform/mac/specific_mac.mm
        SourceFiles/platform/mac/specific_mac_p.mm
        SourceFiles/platform/mac/window_title_mac.mm
    )
    set(tg_RESOURCES Resources/qrc/telegram_mac.qrc)
endif()

if (WIN32)
    configure_file(Resources/winrc/Kepka.rc.in Resources/winrc/Kepka.rc)
    target_sources(Kepka PRIVATE
        SourceFiles/platform/win/audio_win.cpp
        SourceFiles/platform/win/file_utilities_win.cpp
        SourceFiles/platform/win/main_window_win.cpp
        SourceFiles/platform/win/notifications_manager_win.cpp
        SourceFiles/platform/win/specific_win.cpp
        SourceFiles/platform/win/window_title_win.cpp
        SourceFiles/platform/win/windows_app_user_model_id.cpp
        SourceFiles/platform/win/windows_dlls.cpp
        SourceFiles/platform/win/windows_event_filter.cpp

        ${CMAKE_CURRENT_BINARY_DIR}/Resources/winrc/Kepka.rc
    )
    set(tg_RESOURCES Resources/qrc/telegram_wnd.qrc)
endif()

if (WINRT)
    target_sources(Kepka PRIVATE
        SourceFiles/platform/winrt/main_window_winrt.cpp
    )
endif()

if (LINUX)
    target_sources(Kepka PRIVATE
        SourceFiles/platform/linux/file_utilities_linux.cpp
        SourceFiles/platform/linux/linux_desktop_environment.cpp
        SourceFiles/platform/linux/linux_gdk_helper.cpp
        SourceFiles/platform/linux/linux_libnotify.cpp
        SourceFiles/platform/linux/linux_libs.cpp
        SourceFiles/platform/linux/main_window_linux.cpp
        SourceFiles/platform/linux/notifications_manager_linux.cpp
        SourceFiles/platform/linux/specific_linux.cpp
    )
endif()

list(APPEND tg_RESOURCES
    Resources/qrc/telegram.qrc
    Resources/qrc/telegram_sounds.qrc
    Resources/qrc/telegram_emoji.qrc
    Resources/qrc/telegram_emoji_large.qrc
)

qt5_add_resources(tg_RESOURCES_RCC ${tg_RESOURCES})

target_sources(Kepka PRIVATE
    ${tg_RESOURCES_RCC}
)

if (APPLE)
    target_sources(Kepka PRIVATE
        ThirdParty/SPMediaKeyTap/SPMediaKeyTap.m
        ThirdParty/SPMediaKeyTap/SPInvocationGrabbing/NSObject+SPInvocationGrabbing.m
    )
    target_include_directories(Kepka PRIVATE ThirdParty/SPMediaKeyTap)
endif()

target_sources(Kepka PRIVATE
    ThirdParty/minizip/ioapi.c
    ThirdParty/minizip/zip.c
    ThirdParty/minizip/unzip.c

    ThirdParty/emoji_suggestions/emoji_suggestions.cpp
)

include_directories(ThirdParty/GSL/include
    ThirdParty/variant/include
    ThirdParty/emoji_suggestions
    ThirdParty/libtgvoip
    ThirdParty
)

add_subdirectory(ThirdParty/libtgvoip)

if(NOT WIN32)
    add_definitions(-Wno-switch)
endif()

if (WIN32)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_WARNINGS
        -DWIN32 -D_WINDOWS -D_UNICODE -DUNICODE
        -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP
        -DHAVE_STDINT_H -DNOMINMAX)
endif()

add_definitions(-DAL_ALEXT_PROTOTYPES)

if (LINUX)
    add_definitions(-D_REENTRANT)
endif()

# TODO check if needs libexif in deps

if (APPLE)
    set(MACOSX_BUNDLE_ICON_FILE Icon.icns)

    add_custom_command(
        COMMENT "Generating icon file"
        OUTPUT
            ${CMAKE_CURRENT_BINARY_DIR}/${MACOSX_BUNDLE_ICON_FILE}
        COMMAND
            iconutil -c icns -o ${CMAKE_CURRENT_BINARY_DIR}/${MACOSX_BUNDLE_ICON_FILE}
            ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/Images.xcassets/Icon.iconset/
        WORKING_DIRECTORY .
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/Images.xcassets/Icon.iconset
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/Images.xcassets/Icon.iconset
    )
    add_custom_target(iconset_output
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${MACOSX_BUNDLE_ICON_FILE})

    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/${MACOSX_BUNDLE_ICON_FILE}
        PROPERTIES
            MACOSX_PACKAGE_LOCATION Resources)

    target_sources(Kepka PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/${MACOSX_BUNDLE_ICON_FILE}
    )

    foreach(langpack en de es it nl ko pt-BR)
        set(LOCALIZABLE_STRINGS Resources/langs/${langpack}.lproj/Localizable.strings)
        set_source_files_properties(${LOCALIZABLE_STRINGS}
            PROPERTIES
                MACOSX_PACKAGE_LOCATION ${LOCALIZABLE_STRINGS})
        target_sources(Kepka PRIVATE
            ${LOCALIZABLE_STRINGS}
        )
    endforeach(langpack)
endif()

# Disable a single annoying warning about c++17
if(NOT WIN32)
    set_flag_if_supported(Kepka WNCE -Wno-c++1z-extensions)
endif()

target_compile_definitions(Kepka PRIVATE
    TDESKTOP_DISABLE_CRASH_REPORTS
    ${FFMPEG_DEFINITIONS})

if (PACKAGED_BUILD)
    target_compile_definitions(Kepka PRIVATE
        TDESKTOP_DISABLE_REGISTER_CUSTOM_SCHEME
        TDESKTOP_DISABLE_DESKTOP_FILE_GENERATION)
endif()

if(MSVC AND MSVC_VERSION GREATER_EQUAL 1912)
    target_compile_options(Kepka PRIVATE /permissive-)
endif()

target_include_directories(Kepka PRIVATE ${OPENAL_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIR}
    ${OPUS_INCLUDE_DIR} ${FFMPEG_INCLUDE_DIRS} ${ALSA_INCLUDE_DIRS} ${PULSEAUDIO_INCLUDE_DIR} )

target_link_libraries(Kepka
    Qt5::Core
    Qt5::Widgets
    Qt5::Network
    Qt5::GuiPrivate
    Threads::Threads
    tgvoip
    ${OPENAL_LIBRARY}
    ${FFMPEG_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${SWRESAMPLE_LIBRARIES}
    ${SWSCALE_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${OPUS_LIBRARIES}
    ${CONAN_LIBS}
) # crashpad::crashpad_client)

if (APPLE)
    add_dependencies(Kepka iconset_output)

    set_target_properties(Kepka
        PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Telegram.plist
    )
    foreach(framework
        Cocoa
        CoreFoundation
        CoreServices
        ApplicationServices
        CoreText
        CoreGraphics
        Foundation
        OpenGL
        QuartzCore
        AGL
        Security
        SystemConfiguration
        Carbon
        CoreAudio
        AudioToolbox
        AudioUnit
        AppKit
        CoreWLAN
        IOKit
    )
        set(FW_LIB FW_LIB-NOTFOUND)
        find_library(FW_LIB ${framework})
        message(STATUS "Found framework ${FW_LIB}")
        target_link_libraries(Kepka ${FW_LIB})
    endforeach(framework)
    set(FW_LIB FW_LIB-NOTFOUND)
endif()

if(WIN32)
    target_link_libraries(Kepka
        winmm imm32 ws2_32 kernel32 user32 gdi32 winspool comdlg32 advapi32 shell32 ole32
        oleaut32 uuid odbc32 odbccp32 Shlwapi Iphlpapi Gdiplus Strmiids
    )
endif()

if (LINUX)
    target_link_libraries(Kepka Qt5::DBus dl ${ALSA_LIBRARIES} ${PULSEAUDIO_LIBRARIES})
endif()

# Enable LTO optimizations...
if(ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_result OUTPUT ipo_output)

    if(ipo_result)
        message(STATUS "IPO optimizations enabled.")
        if(LINUX AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU"))
            set(CMAKE_AR "/usr/bin/gcc-ar")
            set(CMAKE_RANLIB "/usr/bin/gcc-ranlib")
            set(CMAKE_NM "/usr/bin/gcc-nm")
        endif()
        set_property(TARGET Kepka PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO is not supported: ${ipo_output}")
    endif()
endif()

##================================================
## Installlation
##================================================

include(GNUInstallDirs)

set_target_properties(Kepka PROPERTIES
    OUTPUT_NAME "kepka"
    AUTOMOC ON
)

install(TARGETS Kepka
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES "${CMAKE_SOURCE_DIR}/lib/xdg/kepka.desktop"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
)
install(FILES "${CMAKE_SOURCE_DIR}/lib/xdg/tg.protocol"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/kservices5
)
install(FILES "${CMAKE_SOURCE_DIR}/lib/xdg/kepka.appdata.xml"
    DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo
)

foreach(icon_size 16 32 48 64 128 256 512)
    install(FILES "${CMAKE_SOURCE_DIR}/Telegram/Resources/art/icon${icon_size}.png"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/${icon_size}x${icon_size}/apps
        RENAME kepka.png
    )
endforeach(icon_size)

##================================================
## Tests
##================================================

if(BUILD_TESTING)
    include_directories(ThirdParty/Catch/single_include)
    add_subdirectory(SourceFiles/base/tests)
endif()
