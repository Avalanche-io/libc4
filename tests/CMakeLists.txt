include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)

# C4 ID tests
add_executable(c4_id_test c4_id_test.cpp)
target_link_libraries(c4_id_test PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4_id_test)

# C4M parser and encoder tests
add_executable(c4m_test c4m_test.cpp)
target_link_libraries(c4m_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_test PRIVATE
    C4M_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_test)

# C4M operations tests (PatchDiff, ApplyPatch, Diff, Merge, chain)
add_executable(c4m_operations_test c4m_operations_test.cpp)
target_link_libraries(c4m_operations_test PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4m_operations_test)

# c4m-aware identification tests
add_executable(c4m_identify_test c4m_identify_test.cpp)
target_link_libraries(c4m_identify_test PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4m_identify_test)

# Benchmark test
add_executable(c4_bench c4_bench.cpp)
target_link_libraries(c4_bench PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4_bench)

# Cross-implementation deepen-vs-remove conformance test (erratum 2026-07-31).
# C4M_DEEPEN_VECTOR_CHAIN is defined only where the parser resolves chains.
add_executable(c4m_deepen_vector_test c4m_deepen_vector_test.cpp)
target_link_libraries(c4m_deepen_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_deepen_vector_test PRIVATE
    C4M_DEEPEN_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/deepen-vector")
catch_discover_tests(c4m_deepen_vector_test)

# Cross-implementation conformance for the 1.0.26 decoder rules (link-target,
# backslash-name and damaged-line vectors, 2026-11-10). This line reads the
# canonical and patch forms; it refuses the pretty form loudly.
add_executable(c4m_decoder_vectors_1026_test c4m_decoder_vectors_1026_test.cpp)
target_link_libraries(c4m_decoder_vectors_1026_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_decoder_vectors_1026_test PRIVATE
    C4M_VECTOR_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_decoder_vectors_1026_test)

# Cross-implementation empty-tree conformance test (2026-11-11).
add_executable(c4m_empty_tree_vector_test c4m_empty_tree_vector_test.cpp)
target_link_libraries(c4m_empty_tree_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_empty_tree_vector_test PRIVATE
    C4M_EMPTY_TREE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/empty-tree-vector")
catch_discover_tests(c4m_empty_tree_vector_test)

add_executable(c4m_pretty_refused_test c4m_pretty_refused_test.cpp)
target_link_libraries(c4m_pretty_refused_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_pretty_refused_test PRIVATE
    C4M_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_pretty_refused_test)

add_executable(c4m_name_rule_vector_test c4m_name_rule_vector_test.cpp)
target_link_libraries(c4m_name_rule_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_name_rule_vector_test PRIVATE
    C4M_NAME_RULE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/name-rule-vector")
catch_discover_tests(c4m_name_rule_vector_test)

add_executable(c4m_name_escape_vector_test c4m_name_escape_vector_test.cpp)
target_link_libraries(c4m_name_escape_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_name_escape_vector_test PRIVATE
    C4M_NAME_ESCAPE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/name-escape-vector")
catch_discover_tests(c4m_name_escape_vector_test)

# Cross-implementation childless-directory conformance test (2026-11-11).
add_executable(c4m_childless_dir_vector_test c4m_childless_dir_vector_test.cpp)
target_link_libraries(c4m_childless_dir_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_childless_dir_vector_test PRIVATE
    C4M_CHILDLESS_DIR_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/childless-dir-vector")
catch_discover_tests(c4m_childless_dir_vector_test)

# SafeName pinned to Unicode 17.0.0 and the shared SafeName vector
# (2026-11-13).
add_executable(c4m_safename_vector_test c4m_safename_vector_test.cpp)
target_link_libraries(c4m_safename_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_safename_vector_test PRIVATE
    C4M_SAFENAME_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/safename-vector")
catch_discover_tests(c4m_safename_vector_test)

# The checked-in printable table is what tools/gen_printable.cmake makes
# from the shared text table.
add_test(NAME printable_table_generated
    COMMAND ${CMAKE_COMMAND}
        -DTABLE=${CMAKE_CURRENT_SOURCE_DIR}/testdata/safename-vector/printable-17.0.0.txt
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/printable_table.inc
        -P ${PROJECT_SOURCE_DIR}/tools/gen_printable.cmake)
add_test(NAME printable_table_matches
    COMMAND ${CMAKE_COMMAND} -E compare_files
        ${CMAKE_CURRENT_BINARY_DIR}/printable_table.inc
        ${PROJECT_SOURCE_DIR}/src/c4m/printable_table.inc)
set_tests_properties(printable_table_matches PROPERTIES DEPENDS printable_table_generated)

# A file's C4 ID is its bytes, on every identify path (2026-11-11).
add_executable(c4m_file_id_test c4m_file_id_test.cpp)
target_link_libraries(c4m_file_id_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_file_id_test PRIVATE
    C4M_FILE_ID_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/file-id")
catch_discover_tests(c4m_file_id_test)

# A folded range's ID list survives decode and canonical encode (data-loss
# fix back-ported to 1.0.27, 2026-11-13); the fixture is Go's at 1.0.27.
add_executable(c4m_range_data_test c4m_range_data_test.cpp)
target_link_libraries(c4m_range_data_test PRIVATE c4 OpenSSL::Crypto Catch2::Catch2WithMain)
target_compile_definitions(c4m_range_data_test PRIVATE
    C4M_RANGE_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/range-data")
catch_discover_tests(c4m_range_data_test)

# Cross-implementation sequence null-size conformance test (2026-11-13): a
# folded sequence's size is null if any frame's size is null (c4 #155; the
# vector is c4 master b82ec07, #156 regenerated by #161). libc4 does not fold sequences, so only
# the decode half applies.
add_executable(c4m_sequence_null_size_vector_test c4m_sequence_null_size_vector_test.cpp)
target_link_libraries(c4m_sequence_null_size_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_sequence_null_size_vector_test PRIVATE
    C4M_SEQUENCE_NULL_SIZE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/sequence-null-size-vector")
catch_discover_tests(c4m_sequence_null_size_vector_test)

# The range-data and sequence-size vectors, decode and normaliser half
# (2026-11-16; c4 master b82ec07, #159 and #161): a resolved chain and
# ApplyPatch keep the lists of the ranges they hold, and a directory over a
# range counts the range's size, which counts its list.
add_executable(c4m_range_vectors_test c4m_range_vectors_test.cpp)
target_link_libraries(c4m_range_vectors_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_range_vectors_test PRIVATE
    C4M_TESTDATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_range_vectors_test)

# Every c4m file in the three sequence vectors round-trips as its README's
# table says, compared by OpenSSL's SHA-512 (main's c4m_vector_bytes_test,
# scoped to this line; 2026-11-16). Rows that state 1.1 rules are replaced
# by what core release/1.0.27's library writes (testdata/line-1.0.27/).
add_executable(c4m_sequence_vector_bytes_test c4m_sequence_vector_bytes_test.cpp)
target_link_libraries(c4m_sequence_vector_bytes_test PRIVATE c4 OpenSSL::Crypto Catch2::Catch2WithMain)
target_compile_definitions(c4m_sequence_vector_bytes_test PRIVATE
    C4M_TESTDATA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_sequence_vector_bytes_test)

# A changeset libc4 writes, appended to a c4m core wrote, reads back in
# libc4 and in core (2026-11-16). The fixture is core release/1.0.27's.
# -DC4_CORE_BIN=<core's c4 at release/1.0.27> also resolves it with core.
set(C4_CORE_BIN "" CACHE FILEPATH "core's c4 binary at release/1.0.27, for the cross-implementation chain test")
add_executable(c4m_core_chain_test c4m_core_chain_test.cpp)
target_link_libraries(c4m_core_chain_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_core_chain_test PRIVATE
    C4M_CORE_CHAIN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/core-1027-chain")
if(C4_CORE_BIN)
    target_compile_definitions(c4m_core_chain_test PRIVATE C4M_CORE_BIN="${C4_CORE_BIN}")
endif()
catch_discover_tests(c4m_core_chain_test)
