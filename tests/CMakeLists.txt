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
