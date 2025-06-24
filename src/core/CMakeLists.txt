add_library(winuzzf_core STATIC
    fuzzer_impl.cpp
    fuzzer_impl.h
)

target_include_directories(winuzzf_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(winuzzf_core
    winuzzf_mutators
    winuzzf_coverage
    winuzzf_sandbox
    winuzzf_crash
    winuzzf_logging
    winuzzf_corpus
)
