add_executable(codegen_numbers
    generator.cpp
    main.cpp
    options.cpp
    parsed_file.cpp
    processor.cpp
)
target_link_libraries(codegen_numbers codegen_common Qt5::Core)
