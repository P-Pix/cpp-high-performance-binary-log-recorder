add_executable(hpblr_tests
    test_main.cpp
    test_crc32.cpp
    test_queue.cpp
    test_binary_log.cpp
    test_async_recorder.cpp
)

target_include_directories(hpblr_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hpblr_tests PRIVATE hpblr::core)
target_compile_features(hpblr_tests PRIVATE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(hpblr_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(HPBLR_WARNINGS_AS_ERRORS)
        target_compile_options(hpblr_tests PRIVATE -Werror)
    endif()
endif()

add_test(NAME hpblr_unit_tests COMMAND hpblr_tests)
