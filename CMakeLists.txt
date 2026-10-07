cmake_minimum_required(VERSION 3.20)
project(CrossWorldsFix LANGUAGES C CXX RC)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (MSVC)
    add_compile_options(/std:c++latest /utf-8)
endif()

set(ZYDIS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/zydis")
set(ZYCORE_DIR "${ZYDIS_DIR}/dependencies/zycore")

include_directories(
    src
    src/resources
    external
    external/spdlog/include
    external/inipp
    external/safetyhook/include
    ${ZYDIS_DIR}/include
    ${ZYCORE_DIR}/include
)

add_definitions(
    -DZYDIS_STATIC_BUILD
    -DZYCORE_STATIC_BUILD
    -D_CRT_SECURE_NO_WARNINGS
    -D_UNICODE
    -DUNICODE
)

file(GLOB SAFETYHOOK_SOURCES "external/safetyhook/src/*.cpp")
# filter out os.linux.cpp
list(FILTER SAFETYHOOK_SOURCES EXCLUDE REGEX "os\\.linux\\.cpp")

set(SDK_SOURCES
    src/SDK/Basic.cpp
    src/SDK/CoreUObject_functions.cpp
    src/SDK/Engine_functions.cpp
    src/SDK/UMG_functions.cpp
    src/SDK/WBP_Race_HUD_Player02_functions.cpp
)

set(SOURCES
    src/dllmain.cpp
    ${SAFETYHOOK_SOURCES}
    ${SDK_SOURCES}
    src/resources/version.rc
)

add_library(CrossWorldsFix SHARED ${SOURCES})
set_target_properties(CrossWorldsFix PROPERTIES
    PREFIX ""
    SUFFIX ".asi"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(CrossWorldsFix PRIVATE
    user32
    "${CMAKE_CURRENT_SOURCE_DIR}/.build_zydis/build/Release/Zydis.lib"
    "${CMAKE_CURRENT_SOURCE_DIR}/.build_zydis/build/zycore/Release/Zycore.lib"
)
