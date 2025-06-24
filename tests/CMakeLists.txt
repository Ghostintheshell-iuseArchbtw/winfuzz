add_library(winuzzf_tests STATIC
    test_main.cpp
    test_mutators.cpp
    test_targets.cpp
)

target_include_directories(winuzzf_tests PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(winuzzf_tests
    winuzzf_core
)

# Add test executable
add_executable(test_executable test_runner.cpp)
target_link_libraries(test_executable winuzzf_tests)

# Register tests
add_test(NAME winuzzf_tests COMMAND test_executable)
