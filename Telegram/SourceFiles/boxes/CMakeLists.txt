# Unused: cotire PCH needs more rigid structure.
# To do: get rid of the PCH requirement for more flexible build structure.
add_library(tg_boxes STATIC
    about_box.cpp
    abstract_box.cpp
    add_contact_box.cpp
    autolock_box.cpp
    background_box.cpp
    calendar_box.cpp
    change_phone_box.cpp
    confirm_box.cpp
    confirm_phone_box.cpp
    connection_box.cpp
    download_path_box.cpp
    edit_color_box.cpp
    edit_participant_box.cpp
    edit_privacy_box.cpp
    language_box.cpp
    local_storage_box.cpp
    mute_settings_box.cpp
    notifications_box.cpp
    passcode_box.cpp
    peer_list_box.cpp
    peer_list_controllers.cpp
    photo_crop_box.cpp
    rate_call_box.cpp
    report_box.cpp
    self_destruction_box.cpp
    send_files_box.cpp
    sessions_box.cpp
    share_box.cpp
    sticker_set_box.cpp
    stickers_box.cpp
    username_box.cpp
)
add_dependencies(tg_boxes boxes_styles_output)
