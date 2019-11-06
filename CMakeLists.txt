cmake_minimum_required(VERSION 3.0.0)
project(witness-trainer VERSION 0.1.0)

cmake_policy(SET CMP0076 NEW)

include(CTest)
enable_testing()

add_executable(witness-trainer WIN32)
add_subdirectory(source)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
