cmake_minimum_required(VERSION 3.20)

project(cpp_high_performance_binary_log_recorder
    VERSION 1.0.0
    DESCRIPTION "C++20 high-performance binary event recorder"
    LANGUAGES CXX)

option(HPBLR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(HPBLR_BUILD_TOOLS "Build command line tools" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(hpblr_core
    src/core/async_recorder.cpp
    src/core/binary_log.cpp
    src/core/crc32.cpp
    src/core/event.cpp
    src/core/event_generator.cpp
    src/core/logger.cpp
    src/core/time_utils.cpp
)

add_library(hpblr::core ALIAS hpblr_core)

target_include_directories(hpblr_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

target_compile_features(hpblr_core PUBLIC cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(hpblr_core PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(HPBLR_WARNINGS_AS_ERRORS)
        target_compile_options(hpblr_core PRIVATE -Werror)
    endif()
endif()

if(HPBLR_BUILD_TOOLS)
    add_executable(hpblr_record apps/hpblr_record.cpp)
    target_link_libraries(hpblr_record PRIVATE hpblr::core)

    add_executable(hpblr_tool apps/hpblr_tool.cpp)
    target_link_libraries(hpblr_tool PRIVATE hpblr::core)

    add_executable(hpblr_bench apps/hpblr_bench.cpp)
    target_link_libraries(hpblr_bench PRIVATE hpblr::core)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        foreach(target hpblr_record hpblr_tool hpblr_bench)
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
            if(HPBLR_WARNINGS_AS_ERRORS)
                target_compile_options(${target} PRIVATE -Werror)
            endif()
        endforeach()
    endif()
endif()

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS hpblr_core
    EXPORT hpblrTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin)
install(DIRECTORY include/ DESTINATION include)

if(HPBLR_BUILD_TOOLS)
    install(TARGETS hpblr_record hpblr_tool hpblr_bench RUNTIME DESTINATION bin)
endif()
