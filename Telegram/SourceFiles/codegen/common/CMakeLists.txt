add_library(codegen_common STATIC
    basic_tokenized_file.cpp
    checked_utf8_string.cpp
    clean_file.cpp
    cpp_file.cpp
    logging.cpp
)
qt5_use_modules(codegen_common Core)
