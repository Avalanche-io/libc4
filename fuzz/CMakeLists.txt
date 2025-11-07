# Fuzzing tests for libc4
# Requires clang with fuzzing support (clang -fsanitize=fuzzer)

if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(WARNING "Fuzzing requires Clang compiler. Skipping fuzz targets.")
    return()
endif()

# Fuzzing flags
set(FUZZ_FLAGS "-fsanitize=fuzzer,address,undefined")

# Fuzz target for ID parsing
add_executable(fuzz_id_parse fuzz_id_parse.c)
target_link_libraries(fuzz_id_parse PRIVATE c4)
target_compile_options(fuzz_id_parse PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_id_parse PRIVATE ${FUZZ_FLAGS})

# Fuzz target for encoder
add_executable(fuzz_encoder fuzz_encoder.c)
target_link_libraries(fuzz_encoder PRIVATE c4)
target_compile_options(fuzz_encoder PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_encoder PRIVATE ${FUZZ_FLAGS})

# Fuzz target for digest operations
add_executable(fuzz_digest fuzz_digest.c)
target_link_libraries(fuzz_digest PRIVATE c4)
target_compile_options(fuzz_digest PRIVATE ${FUZZ_FLAGS})
target_link_options(fuzz_digest PRIVATE ${FUZZ_FLAGS})

# Custom target to run all fuzz tests
add_custom_target(fuzz
    COMMENT "Running fuzzing tests (Ctrl+C to stop)"
)

add_custom_command(TARGET fuzz POST_BUILD
    COMMAND echo "Fuzzing tests built successfully."
    COMMAND echo "Run individual fuzz tests:"
    COMMAND echo "  ./fuzz_id_parse"
    COMMAND echo "  ./fuzz_encoder"
    COMMAND echo "  ./fuzz_digest"
    VERBATIM
)
