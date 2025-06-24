add_library(winuzzf_sandbox STATIC
    sandbox.cpp
    sandbox.h
)

target_include_directories(winuzzf_sandbox PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
