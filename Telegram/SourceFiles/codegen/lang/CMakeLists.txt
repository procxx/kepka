add_executable(codegen_lang
    generator.cpp
    main.cpp
    options.cpp
    parsed_file.cpp
    processor.cpp
)
target_link_libraries(codegen_lang codegen_common Qt5::Core Qt5::Gui)
