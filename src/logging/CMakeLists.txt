add_library(winuzzf_logging STATIC
    logger.cpp
    logger.h
)

target_include_directories(winuzzf_logging PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
