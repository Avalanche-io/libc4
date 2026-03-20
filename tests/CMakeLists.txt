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

# C4M tests (placeholder)
add_executable(c4m_test c4m_test.cpp)
target_link_libraries(c4m_test PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4m_test)

# C4M operations tests (PatchDiff, ApplyPatch, Diff, Merge, chain)
add_executable(c4m_operations_test c4m_operations_test.cpp)
target_link_libraries(c4m_operations_test PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4m_operations_test)

# Benchmark test
add_executable(c4_bench c4_bench.cpp)
target_link_libraries(c4_bench PRIVATE c4 Catch2::Catch2WithMain)
catch_discover_tests(c4_bench)
