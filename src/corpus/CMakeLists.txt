add_library(winuzzf_corpus STATIC
    corpus_manager.cpp
    corpus_manager.h
)

target_include_directories(winuzzf_corpus PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)
