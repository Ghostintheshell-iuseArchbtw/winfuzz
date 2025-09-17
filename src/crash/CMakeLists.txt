add_library(winuzzf_crash STATIC
    crash_analyzer.cpp
    crash_analyzer.h
)

target_include_directories(winuzzf_crash PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(winuzzf_crash
        dbghelp
        psapi
    )
endif()
