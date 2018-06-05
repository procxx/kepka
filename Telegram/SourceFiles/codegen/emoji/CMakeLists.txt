add_executable(codegen_emoji
    data.cpp
    generator.cpp
    main.cpp
    options.cpp
    replaces.cpp
)
target_link_libraries(codegen_emoji codegen_common Qt5::Core Qt5::Gui)

