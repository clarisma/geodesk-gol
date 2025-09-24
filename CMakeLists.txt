cmake_minimum_required(VERSION 3.14)
project(geodesk-gol)

option(GOL_DIAGNOSTICS "Enable diagnostic commands" OFF)
option(GOL_EXPERIMENTAL "Enable experimental features" OFF)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    # set(SANITIZE_FLAGS -fsanitize=address -fno-omit-frame-pointer)
elseif(MSVC)
    set(SANITIZE_FLAGS /fsanitize=address /GS /RTC1 /analyze)
endif()

# Apply only to Debug and RelWithDebInfo builds
# if(CMAKE_BUILD_TYPE MATCHES "Debug|RelWithDebInfo")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    message(STATUS "Enabled debugging options: ${SANITIZE_FLAGS}")
    add_compile_options(${SANITIZE_FLAGS})
    add_link_options(${SANITIZE_FLAGS})
endif()

if (MSVC)
    add_compile_options(/Zc:__cplusplus)
    if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
else()
    if (CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        message(STATUS "Applying optimization flags for size reduction")
        add_compile_options(-ffunction-sections -fdata-sections)
        if(APPLE)
            add_link_options(-Wl,-dead_strip)
        else() # Linux, BSD with GNU ld/lld
            add_link_options(-Wl,--gc-sections)
            add_link_options(-s)
        endif()
    endif()
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        message(STATUS "Stripping symbols for size reduction")

    endif()
endif()

# Set the CMAKE_CXX_FLAGS_RELWITHDEBINFO variable to remove the NDEBUG definition
# This approach ensures that assertions are enabled in RelWithDebInfo configuration
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -U NDEBUG")

set(BUILD_SHARED_LIBS OFF)
set(GEODESK_LIBERO ON)

include(FetchContent)

FetchContent_Declare(zlib
    URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(zlib)

# For non-Windows platforms, we use cpp-httplib
if (NOT WIN32)
    message(STATUS "Configuring cpp-httplib with OpenSSL support")
    FetchContent_Declare(
            cpphttplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            # GIT_TAG        v0.10.6  # choose the version you need
    )
    FetchContent_MakeAvailable(cpphttplib)
    # Find OpenSSL and enable its usage for cpp-httplib
    find_package(OpenSSL REQUIRED)
    add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT)
    message(STATUS "cpphttplib_SOURCE_DIR = ${cpphttplib_SOURCE_DIR}")
endif()

# Include the libgeodesk subdirectory
if(GEODESK_SOURCE_DIR)
    message("GOL: Including GeoDesk locally from ${GEODESK_SOURCE_DIR}")
    add_subdirectory(${GEODESK_SOURCE_DIR} ${CMAKE_BINARY_DIR}/_libgeodesk)
else()
    include(FetchContent)
    FetchContent_Declare(geodesk GIT_REPOSITORY
            https://github.com/clarisma/libgeodesk.git
            GIT_TAG v2)
    FetchContent_MakeAvailable(geodesk)
endif()

file(GLOB_RECURSE SOURCE_FILES "src/*.cpp")

# Create the executable target
add_executable(gol ${SOURCE_FILES})
target_compile_definitions(gol PRIVATE GOL_BUILD_STATS)
message("GOL_DIAGNOSTICS: ${GOL_DIAGNOSTICS}")
if(GOL_DIAGNOSTICS)
    message("GOL: Building with diagnostic commands")
    target_compile_definitions(gol PRIVATE GOL_DIAGNOSTICS)
endif()
if(GOL_EXPERIMENTAL)
    message("GOL: Building with experimental features")
    target_compile_definitions(gol PRIVATE GOL_EXPERIMENTAL)
endif()

# Enable zlib wrapper classes
target_compile_definitions(gol PRIVATE CLARISMA_WITH_ZLIB)
# Include directories for geodesk-gol
target_include_directories(gol PRIVATE include src lib/libgeodesk/include
    ${zlib_SOURCE_DIR} ${cpphttplib_SOURCE_DIR})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message("GOL: Disabled warnings about non-standard class layout")
    add_compile_options(-Wno-invalid-offsetof)
endif()

if (MINGW AND CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
    # Make the whole runtime static
    target_link_options(gol  PRIVATE -static -static-libgcc -static-libstdc++)
endif()

# Link geodesk-gol with libgeodesk
target_link_libraries(gol PRIVATE geodesk zlibstatic gtl)
# Link the libraries for SSL support: WinHTTP or OpenSSL
if (WIN32)
    target_link_libraries(gol PRIVATE winhttp)
else()
    # target_link_libraries(gol PRIVATE cpphttplib)
    target_link_libraries(gol PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# TODO: Use a separate build option
if(GEODESK_SOURCE_DIR)
    if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
        set(INSTALL_TARGET "C:/geodesk/tests/gol.exe")
    else()
        set(INSTALL_TARGET "/home/md/geodesk/tests/gol")
    endif()

    add_custom_command(TARGET gol POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
        $<TARGET_FILE:gol>
        ${INSTALL_TARGET}
    )
endif()

list(REMOVE_ITEM SOURCE_FILES "${CMAKE_SOURCE_DIR}/src/gol/main.cpp")
add_executable(test ${SOURCE_FILES} src/test.cxx)
target_include_directories(test PRIVATE src lib/libgeodesk/include ${zlib_SOURCE_DIR})
target_link_libraries(test PRIVATE geodesk)