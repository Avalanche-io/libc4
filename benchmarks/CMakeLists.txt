# Benchmarks for libc4

add_executable(c4_benchmark
    benchmark.c
)

target_link_libraries(c4_benchmark PRIVATE c4)

# Add benchmark target
add_custom_target(bench
    COMMAND c4_benchmark
    DEPENDS c4_benchmark
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running libc4 benchmarks"
)
