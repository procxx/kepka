find_package(Qt5Core 5.6 REQUIRED)
find_package(Qt5Gui 5.6 REQUIRED)
find_package(Qt5Widgets 5.6 REQUIRED)

include_directories(${Qt5Core_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Widgets_INCLUDE_DIRS})


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

set(codegen_common_SRC
    SourceFiles/codegen/common/basic_tokenized_file.cpp
    SourceFiles/codegen/common/checked_utf8_string.cpp
    SourceFiles/codegen/common/clean_file.cpp
    SourceFiles/codegen/common/cpp_file.cpp
    SourceFiles/codegen/common/logging.cpp
)

add_library(codegen_common STATIC ${codegen_common_SRC})
qt5_use_modules(codegen_common Core)

##======================

set(codegen_style_SRC
    SourceFiles/codegen/style/generator.cpp
    SourceFiles/codegen/style/main.cpp
    SourceFiles/codegen/style/module.cpp
    SourceFiles/codegen/style/options.cpp
    SourceFiles/codegen/style/parsed_file.cpp
    SourceFiles/codegen/style/processor.cpp
    SourceFiles/codegen/style/structure_types.cpp
)

add_executable(codegen_style ${codegen_style_SRC})
target_link_libraries(codegen_style codegen_common Qt5::Core Qt5::Gui)
qt5_use_modules(codegen_style Core Gui)

##======================

set(codegen_numbers_SRC
    SourceFiles/codegen/numbers/generator.cpp
    SourceFiles/codegen/numbers/main.cpp
    SourceFiles/codegen/numbers/options.cpp
    SourceFiles/codegen/numbers/parsed_file.cpp
    SourceFiles/codegen/numbers/processor.cpp
)

add_executable(codegen_numbers ${codegen_numbers_SRC})
target_link_libraries(codegen_numbers codegen_common Qt5::Core)
qt5_use_modules(codegen_numbers Core)

##======================

set(MetaLang_SRC
    SourceFiles/_other/mlmain.cpp
    SourceFiles/_other/genlang.cpp
)

add_executable(MetaLang ${MetaLang_SRC})
target_link_libraries(MetaLang codegen_common Qt5::Core)
#qt5_use_modules(codegen_numbers Core)

##======================

#-w<(PRODUCT_DIR)/../.. -- wtf is that
add_custom_command(
    COMMENT "Generating palette"
    COMMAND
        codegen_style -I${CMAKE_CURRENT_SOURCE_DIR}/Resources -I${CMAKE_CURRENT_SOURCE_DIR}
        -o${CMAKE_CURRENT_BINARY_DIR}/styles -w${CMAKE_SOURCE_DIR}
        colors.palette
    OUTPUT
        styles/palette.h
        styles/palette.cpp
    WORKING_DIRECTORY styles
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/colors.palette)

add_custom_command(
    COMMENT "Generating numbers"
    COMMAND
        codegen_numbers -o${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt
    OUTPUT
        numbers.h
        numbers.cpp
    WORKING_DIRECTORY .
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/numbers.txt)

add_custom_command(
    COMMENT "Generating langs"
    COMMAND
        MetaLang
            -lang_in ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings
            -lang_out ${CMAKE_CURRENT_BINARY_DIR}/lang_auto
    OUTPUT
        lang_auto.h
        lang_auto.cpp
    WORKING_DIRECTORY .
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/Resources/langs/lang.strings)

##========

list(APPEND style_files
    Resources/basic
    SourceFiles/boxes/boxes
    SourceFiles/dialogs/dialogs
    SourceFiles/history/history
    SourceFiles/intro/intro
    SourceFiles/media/player/media_player
    SourceFiles/media/view/mediaview
    SourceFiles/overview/overview
    SourceFiles/profile/profile
    SourceFiles/settings/settings
    SourceFiles/stickers/stickers
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
        MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/${src}.style)

    list(APPEND style_sources
        ${CMAKE_CURRENT_BINARY_DIR}/styles/style_${src_file}.cpp)

endforeach()

##======================
## Main app
##======================

# @fixme hardcoded path before I figure a way to include private Qt headers or remove
# this dependency on QtGui/private/qfontengine_p.h from sources
#include_directories(/usr/local/opt/qt5/lib/QtGui.framework/Versions/5/Headers/5.7.0/QtGui)
#include_directories(/usr/local/opt/qt5/lib/QtCore.framework/Versions/5/Headers/5.7.0/QtCore)
set(APP_SRC
    ${CMAKE_CURRENT_BINARY_DIR}/styles/palette.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/numbers.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/lang_auto.cpp
    ${style_sources}

    SourceFiles/apiwrap.cpp
    SourceFiles/app.cpp
    SourceFiles/application.cpp
    SourceFiles/autoupdater.cpp
    SourceFiles/dialogswidget.cpp
    SourceFiles/facades.cpp
    SourceFiles/fileuploader.cpp
    SourceFiles/history.cpp
    SourceFiles/historywidget.cpp
    SourceFiles/lang.cpp
    SourceFiles/langloaderplain.cpp
    SourceFiles/layerwidget.cpp
    SourceFiles/layout.cpp
    SourceFiles/localimageloader.cpp
    SourceFiles/localstorage.cpp
    SourceFiles/logs.cpp
    SourceFiles/main.cpp
    SourceFiles/mainwidget.cpp
    SourceFiles/mainwindow.cpp
    SourceFiles/mediaview.cpp
    SourceFiles/observer_peer.cpp
    SourceFiles/overviewwidget.cpp
    SourceFiles/passcodewidget.cpp
    SourceFiles/settings.cpp
    SourceFiles/shortcuts.cpp
    SourceFiles/stdafx.cpp
    SourceFiles/structs.cpp

    SourceFiles/boxes/aboutbox.cpp
    SourceFiles/boxes/abstractbox.cpp
    SourceFiles/boxes/addcontactbox.cpp
    SourceFiles/boxes/autolockbox.cpp
    SourceFiles/boxes/backgroundbox.cpp
    SourceFiles/boxes/confirmbox.cpp
    SourceFiles/boxes/confirmphonebox.cpp
    SourceFiles/boxes/connectionbox.cpp
    SourceFiles/boxes/contactsbox.cpp
    SourceFiles/boxes/downloadpathbox.cpp
    SourceFiles/boxes/emojibox.cpp
    SourceFiles/boxes/languagebox.cpp
    SourceFiles/boxes/localstoragebox.cpp
    SourceFiles/boxes/members_box.cpp
    SourceFiles/boxes/notifications_box.cpp
    SourceFiles/boxes/passcodebox.cpp
    SourceFiles/boxes/photocropbox.cpp
    SourceFiles/boxes/report_box.cpp
    SourceFiles/boxes/send_files_box.cpp
    SourceFiles/boxes/sessionsbox.cpp
    SourceFiles/boxes/sharebox.cpp
    SourceFiles/boxes/stickers_box.cpp
    SourceFiles/boxes/stickersetbox.cpp
    SourceFiles/boxes/usernamebox.cpp

    SourceFiles/core/click_handler.cpp
    SourceFiles/core/click_handler_types.cpp
    SourceFiles/core/observer.cpp
    SourceFiles/core/parse_helper.cpp
    SourceFiles/core/qthelp_url.cpp
    SourceFiles/core/runtime_composer.cpp
    SourceFiles/core/single_timer.cpp
    SourceFiles/core/task_queue.cpp
    SourceFiles/core/utils.cpp

    SourceFiles/data/data_abstract_structure.cpp
    SourceFiles/data/data_drafts.cpp

    SourceFiles/dialogs/dialogs_indexed_list.cpp
    SourceFiles/dialogs/dialogs_layout.cpp
    SourceFiles/dialogs/dialogs_list.cpp
    SourceFiles/dialogs/dialogs_row.cpp

    SourceFiles/history/field_autocomplete.cpp
    SourceFiles/history/history_drag_area.cpp
    SourceFiles/history/history_item.cpp
    SourceFiles/history/history_location_manager.cpp
    SourceFiles/history/history_media_types.cpp
    SourceFiles/history/history_message.cpp
    SourceFiles/history/history_service_layout.cpp

    SourceFiles/inline_bots/inline_bot_layout_internal.cpp
    SourceFiles/inline_bots/inline_bot_layout_item.cpp
    SourceFiles/inline_bots/inline_bot_result.cpp
    SourceFiles/inline_bots/inline_bot_send_data.cpp

    SourceFiles/intro/introcode.cpp
    SourceFiles/intro/introphone.cpp
    SourceFiles/intro/intropwdcheck.cpp
    SourceFiles/intro/introsignup.cpp
    SourceFiles/intro/introstart.cpp
    SourceFiles/intro/introwidget.cpp

    SourceFiles/media/media_audio.cpp
    SourceFiles/media/media_audio_ffmpeg_loader.cpp
    SourceFiles/media/media_audio_loader.cpp
    SourceFiles/media/media_audio_loaders.cpp
    SourceFiles/media/media_child_ffmpeg_loader.cpp
    SourceFiles/media/media_clip_ffmpeg.cpp
    SourceFiles/media/media_clip_implementation.cpp
    SourceFiles/media/media_clip_qtgif.cpp
    SourceFiles/media/media_clip_reader.cpp

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

    SourceFiles/mtproto/auth_key.cpp
    SourceFiles/mtproto/connection.cpp
    SourceFiles/mtproto/connection_abstract.cpp
    SourceFiles/mtproto/connection_auto.cpp
    SourceFiles/mtproto/connection_http.cpp
    SourceFiles/mtproto/connection_tcp.cpp
    SourceFiles/mtproto/core_types.cpp
    SourceFiles/mtproto/dcenter.cpp
    SourceFiles/mtproto/facade.cpp
    SourceFiles/mtproto/file_download.cpp
    SourceFiles/mtproto/rpc_sender.cpp
    SourceFiles/mtproto/rsa_public_key.cpp
    SourceFiles/mtproto/scheme_auto.cpp
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

    SourceFiles/serialize/serialize_common.cpp
    SourceFiles/serialize/serialize_document.cpp

    SourceFiles/settings/settings_advanced_widget.cpp
    SourceFiles/settings/settings_background_widget.cpp
    SourceFiles/settings/settings_block_widget.cpp
    SourceFiles/settings/settings_chat_settings_widget.cpp
    SourceFiles/settings/settings_cover.cpp
    SourceFiles/settings/settings_fixed_bar.cpp
    SourceFiles/settings/settings_general_widget.cpp
    SourceFiles/settings/settings_info_widget.cpp
    SourceFiles/settings/settings_inner_widget.cpp
    SourceFiles/settings/settings_notifications_widget.cpp
    SourceFiles/settings/settings_privacy_widget.cpp
    SourceFiles/settings/settings_scale_widget.cpp
    SourceFiles/settings/settings_widget.cpp

    SourceFiles/stickers/emoji_pan.cpp
    SourceFiles/stickers/stickers.cpp

    SourceFiles/ui/abstract_button.cpp
    SourceFiles/ui/animation.cpp
    SourceFiles/ui/countryinput.cpp
    SourceFiles/ui/emoji_config.cpp
    SourceFiles/ui/filedialog.cpp
    SourceFiles/ui/images.cpp
    SourceFiles/ui/twidget.cpp

    SourceFiles/ui/buttons/history_down_button.cpp
    SourceFiles/ui/buttons/peer_avatar_button.cpp

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

    SourceFiles/window/main_window.cpp
    SourceFiles/window/notifications_manager.cpp
    SourceFiles/window/notifications_manager_default.cpp
    SourceFiles/window/notifications_utilities.cpp
    SourceFiles/window/player_wrap_widget.cpp
    SourceFiles/window/section_widget.cpp
    SourceFiles/window/top_bar_widget.cpp
    SourceFiles/window/window_main_menu.cpp
    SourceFiles/window/window_slide_animation.cpp
    SourceFiles/window/window_theme.cpp
    SourceFiles/window/window_theme_preview.cpp
    SourceFiles/window/window_theme_warning.cpp
)

set(PLAT_SRC)

if (APPLE)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/pspecific_mac.cpp
        SourceFiles/pspecific_mac_p.mm
        SourceFiles/platform/mac/file_dialog_mac.mm
        SourceFiles/platform/mac/mac_utilities.mm
        SourceFiles/platform/mac/main_window_mac.mm
        SourceFiles/platform/mac/notifications_manager_mac.mm
        SourceFiles/platform/mac/window_title_mac.mm
    )
endif()
if (WIN32)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/pspecific_win.cpp
        SourceFiles/platform/win/main_window_win.cpp
        SourceFiles/platform/win/notifications_manager_win.cpp
        SourceFiles/platform/win/window_title_win.cpp
        SourceFiles/platform/win/windows_app_user_model_id.cpp
        SourceFiles/platform/win/windows_dlls.cpp
        SourceFiles/platform/win/windows_event_filter.cpp
    )
endif()
if (WINRT)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/pspecific_winrt.cpp
        SourceFiles/platform/winrt/main_window_winrt.cpp
    )
endif()
if (LINUX)
    set(PLAT_SRC ${PLAT_SRC}
        SourceFiles/pspecific_linux.cpp
        SourceFiles/platform/linux/file_dialog_linux.cpp
        SourceFiles/platform/linux/linux_gdk_helper.cpp
        SourceFiles/platform/linux/linux_libnotify.cpp
        SourceFiles/platform/linux/linux_libs.cpp
        SourceFiles/platform/linux/main_window_linux.cpp
        SourceFiles/platform/linux/notifications_manager_linux.cpp
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

##======================
## Telegram
##======================

include_directories(SourceFiles SourceFiles/core)

add_executable(Telegram ${APP_SRC} ${PLAT_SRC} ${THIRD_PARTY_SRC})
target_link_libraries(Telegram Qt5::Core Qt5::Widgets) # crashpad::crashpad_client)
target_link_libraries(Telegram ${LIBLZMA_LIBRARIES})
qt5_use_modules(Telegram Core Widgets)

set_target_properties(Telegram PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "SourceFiles/stdafx.h")
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

