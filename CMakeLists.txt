cmake_minimum_required(VERSION 3.0.0)
project(witness-trainer VERSION 0.1.0)

cmake_policy(SET CMP0076 NEW)

include(CTest)
enable_testing()

include(FetchContent)

FetchContent_Declare(
    fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG        6.0.0
)

FetchContent_MakeAvailable(fmt)

add_executable(embedfile embedfile.c)

add_custom_command(
    OUTPUT SegoeUiMono.c
    COMMAND embedfile SegoeUiMono ${PROJECT_SOURCE_DIR}/source/resources/SegoeUiMono.ttf
    DEPENDS source/resources/SegoeUiMono.ttf
)

add_executable(witness-trainer WIN32 SegoeUiMono.c)
add_subdirectory(source)
set_property(TARGET witness-trainer PROPERTY CXX_STANDARD 20)
set_property(TARGET witness-trainer PROPERTY CXX_STANDARD_REQUIRED ON)
target_link_libraries(witness-trainer fmt)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
