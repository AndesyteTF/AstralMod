# This file basically dispatches the main build commands done from CLion down the chain to our modules.
# No real compiler work actually gets done here apart from setting up build settings and build defaults.
cmake_minimum_required(VERSION 3.25)

cmake_policy(SET CMP0091 NEW)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_MSVC_RUNTIME_LIBRARY "" CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD_LIBRARIES "" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS "/nologo /DWIN32 /D_WINDOWS" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_DEBUG "/Ob0 /Od" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/DEBUG /INCREMENTAL:NO" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "/O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/INCREMENTAL:NO" CACHE STRING "" FORCE)

set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG" CACHE STRING "" FORCE)
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO" CACHE STRING "" FORCE)
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "/INCREMENTAL:NO" CACHE STRING "" FORCE)

project(astral_se LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# This will more than likely not even be hit due to CMake failing compiler tests because of the above parameters.
# But in case something does get past somehow, this will be our final failsafe to preventing unsupported compilers.
if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    message(STATUS "clang-cl found!")
else ()
    message(FATAL_ERROR "You are using an unsupported compiler. Please install clang-cl from your preferred source.")
endif ()

set(ASTRAL_GIT_COMMIT_HASH "UNK")
set(ASTRAL_GIT_COMMIT_AUTHOR "UNK")

find_package(Git)
if (GIT_FOUND)
    message(STATUS "Git found!")
    # Fetch git commit hash.
    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE ASTRAL_GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    # Fetch git commit author.
    execute_process(
        COMMAND git show -s --format=%an
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        OUTPUT_VARIABLE ASTRAL_GIT_COMMIT_AUTHOR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else ()
    message(STATUS "Git was not found! Certain information will not be available...")
endif ()

message(STATUS "Git hash: ${ASTRAL_GIT_COMMIT_HASH}")
message(STATUS "Git author: ${ASTRAL_GIT_COMMIT_AUTHOR}")

add_library(astral_options INTERFACE)

set(ASTRAL_CPU_ARCH "AVX2" CACHE STRING "CPU baseline for Astral binaries (AVX2 or SSE2)")
set_property(CACHE ASTRAL_CPU_ARCH PROPERTY STRINGS AVX2 SSE2)
string(TOUPPER "${ASTRAL_CPU_ARCH}" ASTRAL_CPU_ARCH)

target_include_directories(astral_options INTERFACE "${PROJECT_SOURCE_DIR}/base")
target_compile_features(astral_options INTERFACE cxx_std_23)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(
        astral_options
        INTERFACE
        _HAS_EXCEPTIONS=0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        CUR_COMMIT_HASH="${ASTRAL_GIT_COMMIT_HASH}"
        CUR_COMMIT_AUTHOR="${ASTRAL_GIT_COMMIT_AUTHOR}"
        ASTRAL_DEV
        ASTRAL_DISABLE_CLOUD
    )
    message(STATUS "Compiling in debug mode.")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    target_compile_definitions(
        astral_options
        INTERFACE
        _HAS_EXCEPTIONS=0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        CUR_COMMIT_HASH="${ASTRAL_GIT_COMMIT_HASH}"
        CUR_COMMIT_AUTHOR="${ASTRAL_GIT_COMMIT_AUTHOR}"
        ASTRAL_DISABLE_CLOUD
    )
    message(STATUS "Compiling in test shipping mode. All realtime protections and cloud integration will be disabled!")
else ()
    target_compile_definitions(
        astral_options
        INTERFACE
        _HAS_EXCEPTIONS=0
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        CUR_COMMIT_HASH="${ASTRAL_GIT_COMMIT_HASH}"
        CUR_COMMIT_AUTHOR="${ASTRAL_GIT_COMMIT_AUTHOR}"
    )
    message(STATUS "Compiling in shipping mode. All realtime protections and cloud integration will be enabled!")

    set(ASTRAL_TF2_VMPROTECT ON)
endif ()

target_compile_options(
    astral_options
    INTERFACE
    /nologo
    /GS-
    /GR-
    /Zl
    /fp:precise
    /clang:-ffreestanding
    /clang:-fno-exceptions
    /clang:-fno-rtti
    /clang:-fno-stack-protector
    /clang:-fno-threadsafe-statics
    /clang:-fno-unwind-tables
    /clang:-fno-asynchronous-unwind-tables
    /clang:-fno-ident
    /clang:-fno-unroll-loops
)

if (ASTRAL_CPU_ARCH STREQUAL "AVX2")
    target_compile_options(
        astral_options INTERFACE
        /clang:-mbmi
        /clang:-mfma
        /clang:-march=haswell
        /clang:-mtune=haswell
    )
    message(STATUS "Astral CPU baseline: AVX2/Haswell")
elseif (ASTRAL_CPU_ARCH STREQUAL "SSE2")
    # SSE2 is guaranteed on x64. Explicitly disable newer instruction sets so
    # this build remains usable on pre-AVX/AVX2 processors.
    target_compile_options(
        astral_options INTERFACE
        /clang:-march=x86-64
        /clang:-mtune=generic
        /clang:-mno-avx
        /clang:-mno-avx2
        /clang:-mno-fma
        /clang:-mno-bmi
        /clang:-mno-bmi2
    )
    message(STATUS "Astral CPU baseline: SSE2/x86-64")
else ()
    message(FATAL_ERROR "ASTRAL_CPU_ARCH must be AVX2 or SSE2 (got '${ASTRAL_CPU_ARCH}').")
endif ()

target_link_options(
    astral_options
    INTERFACE
    /NOLOGO
    /NODEFAULTLIB
    /SUBSYSTEM:WINDOWS
    /MANIFEST:NO
    /DYNAMICBASE
    /NXCOMPAT
    /OPT:REF
    /OPT:ICF
)

add_library(astral_options_x86 INTERFACE)

target_link_libraries(astral_options_x86 INTERFACE astral_options)
target_compile_options(astral_options_x86 INTERFACE -m32)
target_link_options(astral_options_x86 INTERFACE /MACHINE:X86)

# Register our modules.
# This won't compile absolutely everything into 1 dll.
# Instead: CMake targets will allow us to compile in *specifically* what we want for each dll.
# So for example, if you wanted to compile tf2, you would specify "astral_tf2" in build targets.
# It would then compile the tf2 directory and the base directory, and leave everything else untouched.
# It would also then output a "astral-tf2.dll" in the CMake build directory.
# However, it is a necessity for us to add these as subdirectories otherwise they will not be registered
# in the main scope and that's BAD!!!
add_subdirectory(tf2)

add_subdirectory(base)
