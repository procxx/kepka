add_executable(codegen_style
    generator.cpp
    main.cpp
    module.cpp
    options.cpp
    parsed_file.cpp
    processor.cpp
    structure_types.cpp
)
target_link_libraries(codegen_style codegen_common Qt5::Core Qt5::Gui)
