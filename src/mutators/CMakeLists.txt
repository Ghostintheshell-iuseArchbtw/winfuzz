add_library(winuzzf_mutators STATIC
    mutator.cpp
    mutator.h
)

target_include_directories(winuzzf_mutators PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
