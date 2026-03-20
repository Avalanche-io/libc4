// SPDX-License-Identifier: Apache-2.0
// Simple benchmark test: hashes 10,000 small strings, encodes/decodes IDs,
// and builds a tree. Reports timing to verify optimizations don't regress.

#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::high_resolution_clock;

double elapsed_ms(Clock::time_point start, Clock::time_point end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}

} // anonymous namespace

TEST_CASE("Bench: hash 10000 small strings", "[bench]") {
    constexpr int N = 10000;
    std::vector<c4::ID> ids;
    ids.reserve(N);

    auto start = Clock::now();
    for (int i = 0; i < N; i++) {
        std::string s = "bench-input-" + std::to_string(i);
        ids.push_back(c4::ID::Identify(s));
    }
    auto end = Clock::now();

    double ms = elapsed_ms(start, end);
    std::printf("  Hash %d strings: %.2f ms (%.0f ns/op)\n",
                N, ms, ms * 1e6 / N);

    // Verify some IDs are valid
    REQUIRE(ids.size() == N);
    REQUIRE_FALSE(ids[0].IsNil());
    REQUIRE(ids[0] != ids[1]);
}

TEST_CASE("Bench: encode 10000 IDs to string", "[bench]") {
    constexpr int N = 10000;
    std::vector<c4::ID> ids;
    ids.reserve(N);
    for (int i = 0; i < N; i++) {
        ids.push_back(c4::ID::Identify("encode-bench-" + std::to_string(i)));
    }

    auto start = Clock::now();
    for (int i = 0; i < N; i++) {
        std::string s = ids[i].String();
        REQUIRE(s.size() == 90);
    }
    auto end = Clock::now();

    double ms = elapsed_ms(start, end);
    std::printf("  Encode %d IDs: %.2f ms (%.0f ns/op)\n",
                N, ms, ms * 1e6 / N);
}

TEST_CASE("Bench: decode 10000 ID strings", "[bench]") {
    constexpr int N = 10000;
    std::vector<std::string> strs;
    strs.reserve(N);
    for (int i = 0; i < N; i++) {
        strs.push_back(c4::ID::Identify("decode-bench-" + std::to_string(i)).String());
    }

    auto start = Clock::now();
    for (int i = 0; i < N; i++) {
        auto id = c4::ID::Parse(strs[i]);
        REQUIRE_FALSE(id.IsNil());
    }
    auto end = Clock::now();

    double ms = elapsed_ms(start, end);
    std::printf("  Decode %d IDs: %.2f ms (%.0f ns/op)\n",
                N, ms, ms * 1e6 / N);
}

TEST_CASE("Bench: tree of 1000 IDs", "[bench]") {
    constexpr int N = 1000;
    c4::IDs set;
    for (int i = 0; i < N; i++) {
        set.Append(c4::ID::Identify("tree-bench-" + std::to_string(i)));
    }

    auto start = Clock::now();
    auto tree = set.TreeID();
    auto end = Clock::now();

    double ms = elapsed_ms(start, end);
    std::printf("  Tree of %d IDs: %.2f ms\n", N, ms);

    REQUIRE_FALSE(tree.IsNil());
}

TEST_CASE("Bench: Sum 10000 pairs", "[bench]") {
    constexpr int N = 10000;
    std::vector<c4::ID> ids;
    ids.reserve(N);
    for (int i = 0; i < N; i++) {
        ids.push_back(c4::ID::Identify("sum-bench-" + std::to_string(i)));
    }

    auto start = Clock::now();
    for (int i = 0; i + 1 < N; i++) {
        auto s = ids[i].Sum(ids[i + 1]);
        (void)s;
    }
    auto end = Clock::now();

    double ms = elapsed_ms(start, end);
    std::printf("  Sum %d pairs: %.2f ms (%.0f ns/op)\n",
                N - 1, ms, ms * 1e6 / (N - 1));
}
