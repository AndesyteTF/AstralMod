cmake_minimum_required(VERSION 3.25)

project(astral_signature_doctor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CAPSTONE_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTOOL OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_CSTEST OFF CACHE BOOL "" FORCE)
set(CAPSTONE_BUILD_LEGACY_TESTS OFF CACHE BOOL "" FORCE)
set(CAPSTONE_ARCHITECTURE_DEFAULT OFF CACHE BOOL "" FORCE)
set(CAPSTONE_X86_SUPPORT ON CACHE BOOL "" FORCE)
set(CAPSTONE_X86_ATT_DISABLE ON CACHE BOOL "" FORCE)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../../references/capstone-next"
                 "${CMAKE_CURRENT_BINARY_DIR}/capstone" EXCLUDE_FROM_ALL)

add_executable(astral-signature-doctor main.cpp)
target_link_libraries(astral-signature-doctor PRIVATE capstone)
target_compile_definitions(astral-signature-doctor PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(astral-signature-doctor PRIVATE /W4 /permissive-)
else()
    target_compile_options(astral-signature-doctor PRIVATE -Wall -Wextra -Wpedantic)
endif()

