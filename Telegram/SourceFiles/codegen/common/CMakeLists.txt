add_library(codegen_common STATIC
    basic_tokenized_file.cpp
    checked_utf8_string.cpp
    clean_file.cpp
    cpp_file.cpp
    logging.cpp
)
target_link_libraries(codegen_common Qt5::Core)
