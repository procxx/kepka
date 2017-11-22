find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5Widgets REQUIRED)

include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Core_PRIVATE_INCLUDE_DIRS})
include_directories(${Qt5Gui_INCLUDE_DIRS} ${Qt5Gui_PRIVATE_INCLUDE_DIRS})
include_directories(${Qt5Widgets_INCLUDE_DIRS} ${Qt5Widgets_PRIVATE_INCLUDE_DIRS})

# defines
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        add_definitions(-DQ_OS_LINUX64)
    else()
        add_definitions(-DQ_OS_LINUX32)
    endif()
endif()

##======================
## Codegen Tools
##======================

include_directories(SourceFiles)
add_subdirectory(SourceFiles/codegen)

#-w<(PRODUCT_DIR)/../.. -- wtf is that
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
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/colors.palette
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/colors.palette
)
add_custom_target(palette_output
    DEPENDS styles/palette.h styles/palette.cpp)

add_custom_command(
    COMMENT "Generating numbers"
    OUTPUT
        numbers.h
        numbers.cpp
    COMMAND
        codegen_numbers -o${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
    WORKING_DIRECTORY .
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
)
add_custom_target(numbers_output
    DEPENDS numbers.h numbers.cpp)

add_custom_command(
    COMMENT "Generating langs"
    OUTPUT
        lang_auto.h
        lang_auto.cpp
    COMMAND
        codegen_lang -o${CMAKE_CURRENT_BINARY_DIR} -w${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
    WORKING_DIRECTORY .
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
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
        codegen_emoji ${CMAKE_CURRENT_SOURCE_DIR}/Resources/emoji_autocomplete.json -o ${CMAKE_CURRENT_BINARY_DIR}
    WORKING_DIRECTORY .
    DEPENDS
        codegen_emoji
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
        python ${CMAKE_CURRENT_SOURCE_DIR}/SourceFiles/codegen/scheme/codegen_scheme.py -o${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    WORKING_DIRECTORY .
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/scheme.tl
    VERBATIM
)
add_custom_target(scheme_output
    DEPENDS scheme.h scheme.cpp)

##========

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
#      '-w<(PRODUCT_DIR)/../..',
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
            ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style
    )
    add_custom_target(${src_file}_styles_output
        DEPENDS
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.h
            ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp
    )

    list(APPEND style_sources
        ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp)

endforeach()

##======================
## Main app
##======================

#add_subdirectory(SourceFiles/boxes) - see comment in that dir

set(APP_SRC
    ${CMAKE_CURRENT_BINARY_DIR}/styles/palette.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/numbers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lang_auto.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/scheme.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/emoji.cpp
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

    SourceFiles/chat_helpers/bot_keyboard.cpp
    SourceFiles/chat_helpers/emoji_list_widget.cpp
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

    SourceFiles/dialogs/dialogs_indexed_list.cpp
    SourceFiles/dialogs/dialogs_layout.cpp
    SourceFiles/dialogs/dialogs_list.cpp
    SourceFiles/dialogs/dialogs_row.cpp

    SourceFiles/history/history_drag_area.cpp
    SourceFiles/history/history_item.cpp
    SourceFiles/history/history_inner_widget.cpp
    SourceFiles/history/history_location_manager.cpp
    SourceFiles/history/history_media_types.cpp
    SourceFiles/history/history_message.cpp
    SourceFiles/history/history_service_layout.cpp

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

    SourceFiles/media/player/media_player_button.cpp
    SourceFiles/media/player/media_player_cover.cpp
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
    SourceFiles/media/media_child_ffmpeg_loader.cpp
    SourceFiles/media/media_clip_ffmpeg.cpp
    SourceFiles/media/media_clip_implementation.cpp
    SourceFiles/media/media_clip_qtgif.cpp
    SourceFiles/media/media_clip_reader.cpp

    SourceFiles/mtproto/auth_key.cpp
    SourceFiles/mtproto/connection.cpp
    SourceFiles/mtproto/connection_abstract.cpp
    SourceFiles/mtproto/connection_auto.cpp
    SourceFiles/mtproto/connection_http.cpp
    SourceFiles/mtproto/connection_tcp.cpp
    SourceFiles/mtproto/core_types.cpp
    SourceFiles/mtproto/dcenter.cpp
    SourceFiles/mtproto/dc_options.cpp
    SourceFiles/mtproto/facade.cpp
    SourceFiles/mtproto/rpc_sender.cpp
    SourceFiles/mtproto/mtp_instance.cpp
    SourceFiles/mtproto/rsa_public_key.cpp
    SourceFiles/mtproto/session.cpp

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
    SourceFiles/window/themes/window_theme.cpp
    SourceFiles/window/themes/window_theme_editor.cpp
    SourceFiles/window/themes/window_theme_editor_block.cpp
    SourceFiles/window/themes/window_theme_preview.cpp
    SourceFiles/window/themes/window_theme_warning.cpp
    SourceFiles/apiwrap.cpp
    SourceFiles/app.cpp
    SourceFiles/application.cpp
    SourceFiles/auth_session.cpp
    SourceFiles/autoupdater.cpp
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
    SourceFiles/qt_static_plugins.cpp
    SourceFiles/settings.cpp
    SourceFiles/shortcuts.cpp
    SourceFiles/structs.cpp
)

set(PLAT_SRC)

if (APPLE)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/platform/mac/file_utilities_mac.mm
        SourceFiles/platform/mac/mac_iconv_helper.c
        SourceFiles/platform/mac/mac_utilities.mm
        SourceFiles/platform/mac/main_window_mac.mm
        SourceFiles/platform/mac/notifications_manager_mac.mm
        SourceFiles/platform/mac/specific_mac.mm
        SourceFiles/platform/mac/specific_mac_p.mm
        SourceFiles/platform/mac/window_title_mac.mm
    )
endif()
if (WIN32)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/platform/win/audio_win.cpp
        SourceFiles/platform/win/file_utilities_win.cpp
        SourceFiles/platform/win/main_window_win.cpp
        SourceFiles/platform/win/notifications_manager_win.cpp
        SourceFiles/platform/win/specific_win.cpp
        SourceFiles/platform/win/window_title_win.cpp
        SourceFiles/platform/win/windows_app_user_model_id.cpp
        SourceFiles/platform/win/windows_dlls.cpp
        SourceFiles/platform/win/windows_event_filter.cpp
    )
endif()
if (WINRT)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/platform/winrt/main_window_winrt.cpp
    )
endif()
if (LINUX)
    set(PLAT_SRC ${PLAT_SRC}
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

set(THIRD_PARTY_SRC)

if (APPLE)
    list(APPEND THIRD_PARTY_SRC
        ThirdParty/SPMediaKeyTap/SPMediaKeyTap.m
        ThirdParty/SPMediaKeyTap/SPInvocationGrabbing/NSObject+SPInvocationGrabbing.m
    )
    include_directories(ThirdParty/SPMediaKeyTap)
endif()

list(APPEND THIRD_PARTY_SRC
    ThirdParty/minizip/ioapi.c
    ThirdParty/minizip/zip.c
    ThirdParty/minizip/unzip.c
)
include_directories(ThirdParty/minizip)

include_directories(ThirdParty/GSL/include ThirdParty/variant/include ThirdParty/emoji_suggestions)

##======================
## Telegram
##======================

include_directories(SourceFiles SourceFiles/core)

include_directories(${OPENAL_INCLUDE_DIR} ${ZLIB_INCLUDE_DIRS}
    ${LIBZIP_INCLUDE_DIR_ZIP} ${LIBZIP_INCLUDE_DIR_ZIPCONF}
    ${OPENSSL_INCLUDE_DIR} ${LIBLZMA_INCLUDE_DIRS} ${GTK3_INCLUDE_DIRS})

link_directories(${GTK3_LIBRARY_DIRS})

# Shut up for testbuilding, remove me
include_directories(/usr/local/opt/openal-soft/include)
add_definitions(-Wno-switch)
# End remove me
if (WIN32)
    add_definitions(-DTDESKTOP_DISABLE_CRASH_REPORTS -D_CRT_SECURE_NO_WARNINGS -DWIN32  -D_WINDOWS -DUNICODE -DWIN64 -DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP)
endif()

add_executable(Telegram
    SourceFiles/stdafx.cpp
    ${APP_SRC}
    ${PLAT_SRC}
    ${THIRD_PARTY_SRC}
)
target_link_libraries(Telegram tg_boxes)
target_link_libraries(Telegram Qt5::Core Qt5::Widgets) # crashpad::crashpad_client)
target_link_libraries(Telegram ${LIBLZMA_LIBRARIES} ${OPENSSL_LIBRARIES} ${ZLIB_LIBRARIES} ${OPENAL_LIBRARY})
qt5_use_modules(Telegram Core Widgets)

add_dependencies(Telegram boxes_styles_output)

set_target_properties(Telegram
    PROPERTIES
        COTIRE_CXX_PREFIX_HEADER_INIT SourceFiles/stdafx.h
        COTIRE_ADD_UNITY_BUILD FALSE
)
cotire(Telegram)

##======================

if (APPLE)
    set(UPD_SRC SourceFiles/_other/updater_osx.m)
endif()
if (WIN32)
    set(UPD_SRC SourceFiles/_other/updater.cpp)
endif()
if (LINUX)
    set(UPD_SRC SourceFiles/_other/updater_linux.cpp)
endif()

##======================
## Updater
##======================

#add_executable(Updater ${UPD_SRC})
#cotire(Updater)

