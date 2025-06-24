# Tools for WinFuzz

# Corpus analyzer tool
add_executable(corpus_analyzer corpus_analyzer.cpp)
target_link_libraries(corpus_analyzer
    winuzzf_core
    winuzzf_corpus
)

# Crash minimizer tool
add_executable(crash_minimizer crash_minimizer.cpp)
target_link_libraries(crash_minimizer
    winuzzf_core
    winuzzf_crash
)

# Install tools
install(TARGETS corpus_analyzer crash_minimizer DESTINATION bin)
