add_library(winuzzf_coverage STATIC
    coverage_collector.cpp
    coverage_collector.h
)

target_include_directories(winuzzf_coverage PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(winuzzf_coverage
    advapi32
)
