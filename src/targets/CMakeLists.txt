add_library(winuzzf_targets STATIC
    targets.cpp
)

target_include_directories(winuzzf_targets PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
