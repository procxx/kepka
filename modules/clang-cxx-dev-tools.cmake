# http://www.labri.fr/perso/fleury/posts/programming/using-clang-tidy-and-clang-format.html

# Additional target to perform clang-format/clang-tidy run
# Requires clang-format and clang-tidy
file(GLOB_RECURSE
        ALL_CXX_SOURCE_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles/*.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/Telegram/SourceFiles/*.h
        )
set(clang-format "clang-format")

# Adding clang-format target if executable is found
find_program(CLANG_FORMAT "clang-format")
if(CLANG_FORMAT)
    add_custom_target(
            clang-format
            COMMAND clang-format
            -i
            -style=file
            ${ALL_CXX_SOURCE_FILES}
    )
endif()

# Adding clang-tidy target if executable is found
find_program(CLANG_TIDY "clang-tidy")
if(CLANG_TIDY)
    add_custom_target(
            clang-tidy
            COMMAND /usr/bin/clang-tidy
            ${ALL_CXX_SOURCE_FILES}
            -config=''
            --
            -std=c++11
            ${INCLUDE_DIRECTORIES}
    )
endif()
