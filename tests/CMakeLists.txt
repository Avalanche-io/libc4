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

# Cross-implementation chain-vector conformance test (grammar erratum)
add_executable(c4m_chain_vector_test c4m_chain_vector_test.cpp)
target_link_libraries(c4m_chain_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_chain_vector_test PRIVATE
    C4M_CHAIN_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/chain-vector")
catch_discover_tests(c4m_chain_vector_test)

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
target_compile_definitions(c4m_deepen_vector_test PRIVATE C4M_DEEPEN_VECTOR_CHAIN=1)

# Cross-implementation known-zero-against-null conformance test (2026-11-09),
# plus the real 1.0.21 file whose directories carry the legacy ten-dash null.
add_executable(c4m_zero_null_vector_test c4m_zero_null_vector_test.cpp)
target_link_libraries(c4m_zero_null_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_zero_null_vector_test PRIVATE
    C4M_ZERO_NULL_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/zero-null-vector"
    C4M_LEGACY_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/legacy")
catch_discover_tests(c4m_zero_null_vector_test)

# Cross-implementation conformance for the 1.0.26 decoder rules (link-target,
# backslash-name and damaged-line vectors, 2026-11-10).
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

# Cross-implementation childless-directory conformance test (2026-11-11).
add_executable(c4m_childless_dir_vector_test c4m_childless_dir_vector_test.cpp)
target_link_libraries(c4m_childless_dir_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_childless_dir_vector_test PRIVATE
    C4M_CHILDLESS_DIR_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/childless-dir-vector")
catch_discover_tests(c4m_childless_dir_vector_test)

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

# Cross-implementation null-size conformance test (2026-11-13): a size of 0
# beside a C4 ID other than the empty content's is a null size.
add_executable(c4m_null_size_vector_test c4m_null_size_vector_test.cpp)
target_link_libraries(c4m_null_size_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_null_size_vector_test PRIVATE
    C4M_NULL_SIZE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/null-size-vector")
catch_discover_tests(c4m_null_size_vector_test)

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

# Every function a public C header declares links, and the headers compile
# as C (2026-11-13). The second test fails if a header declares a function
# the link test does not name.
add_executable(c_api_link_test c_api_link_test.c)
set_target_properties(c_api_link_test PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(c_api_link_test PRIVATE c4)
add_test(NAME c_api_link_test COMMAND c_api_link_test)
add_test(NAME c_api_declared
    COMMAND ${CMAKE_COMMAND}
        -DINCLUDE_DIR=${PROJECT_SOURCE_DIR}/include/c4
        -DLINK_TEST=${CMAKE_CURRENT_SOURCE_DIR}/c_api_link_test.c
        -P ${CMAKE_CURRENT_SOURCE_DIR}/c_api_declared.cmake)

# Cross-implementation sequence null-size conformance test (2026-11-13): a
# folded sequence's size is null if any frame's size is null (c4 #155; the
# vector is c4 commit 7a6ba94, #156). libc4 does not fold sequences, so only
# the decode half applies.
add_executable(c4m_sequence_null_size_vector_test c4m_sequence_null_size_vector_test.cpp)
target_link_libraries(c4m_sequence_null_size_vector_test PRIVATE c4 Catch2::Catch2WithMain)
target_compile_definitions(c4m_sequence_null_size_vector_test PRIVATE
    C4M_SEQUENCE_NULL_SIZE_VECTOR_DIR="${CMAKE_CURRENT_SOURCE_DIR}/testdata/sequence-null-size-vector")
catch_discover_tests(c4m_sequence_null_size_vector_test)

# Every c4m file in every vendored shared vector re-encodes to the same
# bytes, compared by OpenSSL's SHA-512, not libc4's C4 ID code (2026-11-13).
add_executable(c4m_vector_bytes_test c4m_vector_bytes_test.cpp)
target_link_libraries(c4m_vector_bytes_test PRIVATE c4 OpenSSL::Crypto Catch2::Catch2WithMain)
target_compile_definitions(c4m_vector_bytes_test PRIVATE
    C4M_TESTDATA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/testdata")
catch_discover_tests(c4m_vector_bytes_test)
