cmake_minimum_required(VERSION 3.18.1)

project("umamusume-localify-android")

macro(SET_OPTION option value)
    set(${option} ${value} CACHE INTERNAL "" FORCE)
endmacro()

SET_OPTION(DOBBY_DEBUG OFF)
SET_OPTION(FullFloatingPointRegisterPack ON)
SET_OPTION(Plugin.Android.BionicLinkerUtil OFF)
SET_OPTION(Plugin.SymbolResolver ON)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/Dobby EXCLUDE_FROM_ALL)

include_directories(${CMAKE_CURRENT_LIST_DIR}/rapidjson/include)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/hmac_sha256)

# msgpack11
add_library(msgpack11 ${CMAKE_CURRENT_LIST_DIR}/msgpack11/msgpack11.cpp)
include_directories(${CMAKE_CURRENT_LIST_DIR}/msgpack11)
target_include_directories(msgpack11 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(msgpack11 PRIVATE -fno-rtti)
if (NOT MSVC)
    target_compile_options(msgpack11 PRIVATE -Wall -Wextra -Werror)
endif ()

set(HTTPLIB_USE_ZSTD_IF_AVAILABLE OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/httplib)

add_compile_definitions(PHMAP_HAVE_SSE2=0 PHMAP_HAVE_SSSE3=0)
SET_OPTION(LSPLANT_BUILD_SHARED OFF)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/LSPlant/lsplant/src/main/jni)

if (TARGET lsplant_static)
    target_compile_options(lsplant_static PUBLIC -Wno-gnu-anonymous-struct)
    set_target_properties(lsplant_static PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS ON
    )
endif ()

set(XZ_SOURCES
        xz_crc32.c
        xz_crc64.c
        xz_dec_lzma2.c
        xz_dec_stream.c)
set(XZ_INCLUDES ${CMAKE_CURRENT_LIST_DIR}/xz-embedded/linux/include/linux ${CMAKE_CURRENT_LIST_DIR}/xz-embedded/userspace)
list(TRANSFORM XZ_SOURCES PREPEND ${CMAKE_CURRENT_LIST_DIR}/xz-embedded/linux/lib/xz/)
add_library(xz_static STATIC ${XZ_SOURCES})
target_compile_options(xz_static PRIVATE -DXZ_USE_CRC64)
target_include_directories(xz_static PRIVATE ${XZ_INCLUDES})

file(GLOB_RECURSE PROJECT_SOURCES
        RELATIVE ${CMAKE_CURRENT_LIST_DIR}
        "*.cpp"
        "*.c"
)

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^Dobby/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^LSPlant/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^hmac_sha256/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^httplib/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^rapidjson/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^msgpack11/.*")
list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^xz-embedded/.*")

list(FILTER PROJECT_SOURCES EXCLUDE REGEX "^_il2cpp_hook\\.cpp$")

include_directories(${CMAKE_CURRENT_LIST_DIR})
include_directories(${CMAKE_CURRENT_LIST_DIR}/config)
include_directories(${CMAKE_CURRENT_LIST_DIR}/scripts)
include_directories(${CMAKE_CURRENT_LIST_DIR}/il2cpp)
include_directories(${CMAKE_CURRENT_LIST_DIR}/local)
include_directories(${CMAKE_CURRENT_LIST_DIR}/localify)
include_directories(${CMAKE_CURRENT_LIST_DIR}/logger)
include_directories(${CMAKE_CURRENT_LIST_DIR}/masterdb)
include_directories(${CMAKE_CURRENT_LIST_DIR}/msgpack)
include_directories(${CMAKE_CURRENT_LIST_DIR}/notifier)
include_directories(${CMAKE_CURRENT_LIST_DIR}/zygoteloader)

include_directories(include)

include_directories(${CMAKE_CURRENT_LIST_DIR}/xz-embedded/linux/include)

message("Build type: ${CMAKE_BUILD_TYPE}")

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

set(LINKER_FLAGS "-ffixed-x18 -Wl,--hash-style=both")
set(C_FLAGS "-Werror=format -fdata-sections -ffunction-sections")
set(CXX_FLAGS "${CXX_FLAGS} -fexceptions -fno-rtti")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(C_FLAGS "${C_FLAGS} -O2 -fvisibility=hidden -fvisibility-inlines-hidden")
    set(LINKER_FLAGS "${LINKER_FLAGS} -Wl,-exclude-libs,ALL -Wl,--gc-sections -Wl,--strip-all")
else ()
    set(C_FLAGS "${C_FLAGS} -O0")
endif ()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${C_FLAGS} ${CXX_FLAGS}")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${LINKER_FLAGS}")
set(CMAKE_MODULE_LINKER_FLAGS "${CMAKE_MODULE_LINKER_FLAGS} ${LINKER_FLAGS}")

add_library(${MODULE_NAME} SHARED ${PROJECT_SOURCES})
target_link_libraries(${MODULE_NAME} log xz_static dobby_static msgpack11 lsplant_static hmac_sha256 httplib)

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_custom_command(TARGET ${MODULE_NAME} POST_BUILD
            COMMAND ${CMAKE_STRIP} --strip-all --remove-section=.comment "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/lib${MODULE_NAME}.so")
endif ()
