// SPDX-License-Identifier: Apache-2.0
// C4M format tests — parser, encoder, natural sort, mode/timestamp formatting.
// Test vectors match the Go reference implementation.

#include "c4/c4m.hpp"
#include "c4/c4.hpp"

#include <catch2/catch_test_macros.hpp>

#include <sstream>
#include <string>

// =============================================================
// Mode formatting / parsing
// =============================================================

TEST_CASE("C4M: format mode regular 644", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(0644) == "-rw-r--r--");
}

TEST_CASE("C4M: format mode regular 755", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(0755) == "-rwxr-xr-x");
}

TEST_CASE("C4M: format mode directory 755", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(c4m::ModeDir | 0755) == "drwxr-xr-x");
}

TEST_CASE("C4M: format mode symlink 777", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(c4m::ModeSymlink | 0777) == "lrwxrwxrwx");
}

TEST_CASE("C4M: format mode setuid", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(c4m::ModeSetuid | 0755) == "-rwsr-xr-x");
}

TEST_CASE("C4M: format mode setgid", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(c4m::ModeSetgid | 0755) == "-rwxr-sr-x");
}

TEST_CASE("C4M: format mode sticky", "[c4m][mode]") {
    REQUIRE(c4m::FormatMode(c4m::ModeDir | c4m::ModeSticky | 0777) == "drwxrwxrwt");
}

TEST_CASE("C4M: parse mode round-trip", "[c4m][mode]") {
    std::vector<uint32_t> modes = {
        0644, 0755, 0600, 0777, 0400,
        c4m::ModeDir | 0755,
        c4m::ModeSymlink | 0777,
        c4m::ModeSetuid | 0755,
        c4m::ModeSetgid | 0755,
        c4m::ModeDir | c4m::ModeSticky | 0777,
        c4m::ModeNamedPipe | 0644,
        c4m::ModeSocket | 0755,
        c4m::ModeDevice | 0660,
        c4m::ModeCharDevice | 0660,
    };
    for (uint32_t m : modes) {
        std::string s = c4m::FormatMode(m);
        REQUIRE(s.size() == 10);
        uint32_t parsed = c4m::ParseMode(s);
        REQUIRE(parsed == m);
    }
}

TEST_CASE("C4M: parse mode rejects invalid", "[c4m][mode]") {
    REQUIRE_THROWS_AS(c4m::ParseMode("short"), std::invalid_argument);
    REQUIRE_THROWS_AS(c4m::ParseMode("xrwxr-xr-x"), std::invalid_argument);
}

// =============================================================
// Timestamp formatting / parsing
// =============================================================

TEST_CASE("C4M: format timestamp", "[c4m][timestamp]") {
    // 2024-01-15T10:30:00Z
    REQUIRE(c4m::FormatTimestamp(1705314600) == "2024-01-15T10:30:00Z");
}

TEST_CASE("C4M: format null timestamp", "[c4m][timestamp]") {
    REQUIRE(c4m::FormatTimestamp(0) == "-");
}

TEST_CASE("C4M: parse timestamp canonical", "[c4m][timestamp]") {
    int64_t ts = c4m::ParseTimestamp("2024-01-15T10:30:00Z");
    REQUIRE(ts == 1705314600);
}

TEST_CASE("C4M: parse timestamp with timezone offset", "[c4m][timestamp]") {
    // 2024-06-15T10:30:00+05:00 -> UTC 05:30:00
    int64_t ts = c4m::ParseTimestamp("2024-06-15T10:30:00+05:00");
    std::string formatted = c4m::FormatTimestamp(ts);
    REQUIRE(formatted == "2024-06-15T05:30:00Z");
}

TEST_CASE("C4M: parse timestamp negative offset", "[c4m][timestamp]") {
    // 2024-06-15T10:30:00-07:00 -> UTC 17:30:00
    int64_t ts = c4m::ParseTimestamp("2024-06-15T10:30:00-07:00");
    REQUIRE(c4m::FormatTimestamp(ts) == "2024-06-15T17:30:00Z");
}

TEST_CASE("C4M: parse null timestamps", "[c4m][timestamp]") {
    REQUIRE(c4m::ParseTimestamp("-") == 0);
    REQUIRE(c4m::ParseTimestamp("0") == 0);
}

TEST_CASE("C4M: timestamp round-trip", "[c4m][timestamp]") {
    int64_t original = 1705314600;
    std::string s = c4m::FormatTimestamp(original);
    int64_t parsed = c4m::ParseTimestamp(s);
    REQUIRE(parsed == original);
}

// =============================================================
// Entry formatting
// =============================================================

TEST_CASE("C4M: entry canonical regular file", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 1024;
    e.name = "README.md";
    e.id = c4::ID::Identify("readme content");

    std::string line = e.Canonical();
    REQUIRE(line.substr(0, 10) == "-rw-r--r--");
    REQUIRE(line.find("2024-01-15T10:30:00Z") != std::string::npos);
    REQUIRE(line.find("1024") != std::string::npos);
    REQUIRE(line.find("README.md") != std::string::npos);
    REQUIRE(line.find("c4") != std::string::npos);
}

TEST_CASE("C4M: entry canonical directory", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = c4m::ModeDir | 0755;
    e.timestamp = 1705314600;
    e.size = 4096;
    e.name = "src/";

    std::string line = e.Canonical();
    REQUIRE(line.substr(0, 10) == "drwxr-xr-x");
    REQUIRE(line.find("src/") != std::string::npos);
}

TEST_CASE("C4M: entry canonical symlink", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = c4m::ModeSymlink | 0777;
    e.timestamp = 1705314600;
    e.size = 0;
    e.name = "link.txt";
    e.target = "target.txt";

    std::string line = e.Canonical();
    REQUIRE(line.substr(0, 10) == "lrwxrwxrwx");
    REQUIRE(line.find("-> target.txt") != std::string::npos);
}

TEST_CASE("C4M: entry with quoted name", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "my file.txt";

    std::string line = e.Canonical();
    REQUIRE(line.find("\"my file.txt\"") != std::string::npos);
}

TEST_CASE("C4M: entry with null values", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0;
    e.timestamp = 0;
    e.size = -1;
    e.name = "unknown.txt";

    std::string line = e.Canonical();
    REQUIRE(line.find("----------") != std::string::npos);
    REQUIRE(line.find(" - ") != std::string::npos); // null timestamp and/or size
}

TEST_CASE("C4M: entry format with indentation", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "nested.txt";
    e.depth = 2;

    std::string line = e.Format(2);
    REQUIRE(line.substr(0, 4) == "    ");
}

TEST_CASE("C4M: entry IsDir", "[c4m][entry]") {
    c4m::Entry dir;
    dir.mode = c4m::ModeDir | 0755;
    REQUIRE(dir.IsDir());

    // Also detect by trailing slash
    c4m::Entry dir2;
    dir2.name = "src/";
    REQUIRE(dir2.IsDir());

    c4m::Entry file;
    file.mode = 0644;
    file.name = "file.txt";
    REQUIRE_FALSE(file.IsDir());
}

TEST_CASE("C4M: entry IsSymlink", "[c4m][entry]") {
    c4m::Entry link;
    link.mode = c4m::ModeSymlink | 0777;
    REQUIRE(link.IsSymlink());

    c4m::Entry file;
    file.mode = 0644;
    REQUIRE_FALSE(file.IsSymlink());
}

// =============================================================
// Natural sort
// =============================================================

TEST_CASE("C4M: natural sort basic", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("file1.txt", "file2.txt"));
    REQUIRE(c4m::NaturalLess("file2.txt", "file10.txt"));
    REQUIRE_FALSE(c4m::NaturalLess("file10.txt", "file2.txt"));
}

TEST_CASE("C4M: natural sort equal", "[c4m][sort]") {
    REQUIRE_FALSE(c4m::NaturalLess("file.txt", "file.txt"));
}

TEST_CASE("C4M: natural sort numeric only", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("1", "2"));
    REQUIRE(c4m::NaturalLess("2", "10"));
    REQUIRE(c4m::NaturalLess("9", "10"));
}

TEST_CASE("C4M: natural sort text only", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("abc", "def"));
    REQUIRE(c4m::NaturalLess("a", "b"));
}

TEST_CASE("C4M: natural sort mixed", "[c4m][sort]") {
    // Text sorts before numeric (per spec)
    REQUIRE(c4m::NaturalLess("abc", "123"));
}

TEST_CASE("C4M: natural sort shorter first", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("file", "file1"));
}

TEST_CASE("C4M: natural sort padding", "[c4m][sort]") {
    // Equal numeric value, shorter representation first
    REQUIRE(c4m::NaturalLess("render.1.exr", "render.01.exr"));
}

// =============================================================
// Parser: basic manifests
// =============================================================

TEST_CASE("C4M: parse empty manifest", "[c4m][parser]") {
    auto m = c4m::Manifest::Parse("@c4m 1.0\n");
    REQUIRE(m.Version() == "1.0");
    REQUIRE(m.EntryCount() == 0);
}

TEST_CASE("C4M: parse single file", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "file.txt");
    REQUIRE(e.mode == 0644);
    REQUIRE(e.size == 100);
    REQUIRE(e.depth == 0);
}

TEST_CASE("C4M: parse directory", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 4096 src/\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "src/");
    REQUIRE(e.IsDir());
    REQUIRE(e.mode == (c4m::ModeDir | 0755));
    REQUIRE(e.size == 4096);
}

TEST_CASE("C4M: parse symlink", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> target\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "link");
    REQUIRE(e.target == "target");
    REQUIRE(e.IsSymlink());
}

TEST_CASE("C4M: parse symlink with absolute path target", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> /absolute/path/target\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].target == "/absolute/path/target");
}

TEST_CASE("C4M: parse quoted filename", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 2048 \"my file.txt\"\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].name == "my file.txt");
    REQUIRE(m.Entries()[0].size == 2048);
}

TEST_CASE("C4M: parse file with C4 ID", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 1024 README.md "
        "c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "README.md");
    REQUIRE_FALSE(e.id.IsNil());
    REQUIRE(e.id.String() ==
        "c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1");
}

TEST_CASE("C4M: parse indented entries", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 src/\n"
        "  -rw-r--r-- 2024-01-01T00:00:00Z 512 main.cpp\n"
        "  drwxr-xr-x 2024-01-01T00:00:00Z 0 include/\n"
        "    -rw-r--r-- 2024-01-01T00:00:00Z 256 header.hpp\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 4);

    REQUIRE(m.Entries()[0].depth == 0);
    REQUIRE(m.Entries()[0].name == "src/");
    REQUIRE(m.Entries()[1].depth == 1);
    REQUIRE(m.Entries()[1].name == "main.cpp");
    REQUIRE(m.Entries()[2].depth == 1);
    REQUIRE(m.Entries()[2].name == "include/");
    REQUIRE(m.Entries()[3].depth == 2);
    REQUIRE(m.Entries()[3].name == "header.hpp");
}

TEST_CASE("C4M: parse multiple entries", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file1.txt\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 file2.txt\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 dir/\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 3);
}

TEST_CASE("C4M: parse with @base directive", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "@base c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE_FALSE(m.Base().IsNil());
    REQUIRE(m.EntryCount() == 1);
}

TEST_CASE("C4M: parse with null values", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "---------- - - unknown.txt\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.mode == 0);
    REQUIRE(e.timestamp == 0);
    REQUIRE(e.size == -1);
    REQUIRE(e.name == "unknown.txt");
}

TEST_CASE("C4M: parse with comma-formatted size", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 1,234,567 large.bin\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].size == 1234567);
}

TEST_CASE("C4M: parse CRLF line endings", "[c4m][parser]") {
    std::string input =
        "@c4m 1.0\r\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt\r\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].name == "file.txt");
}

// =============================================================
// Parser: error cases
// =============================================================

TEST_CASE("C4M: parse rejects empty input", "[c4m][parser]") {
    REQUIRE_THROWS(c4m::Manifest::Parse(""));
}

TEST_CASE("C4M: parse rejects invalid header", "[c4m][parser]") {
    REQUIRE_THROWS(c4m::Manifest::Parse("not a c4m file\n"));
}

TEST_CASE("C4M: parse rejects unsupported version", "[c4m][parser]") {
    REQUIRE_THROWS(c4m::Manifest::Parse("@c4m 2.0\n"));
}

// =============================================================
// Encoder
// =============================================================

TEST_CASE("C4M: encode empty manifest", "[c4m][encoder]") {
    c4m::Manifest m;
    std::string out = m.Encode();
    REQUIRE(out == "@c4m 1.0\n");
}

TEST_CASE("C4M: encode single file", "[c4m][encoder]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1704067200; // 2024-01-01T00:00:00Z
    e.size = 100;
    e.name = "file.txt";
    m.AddEntry(std::move(e));

    std::string out = m.Encode();
    REQUIRE(out.find("@c4m 1.0\n") == 0);
    REQUIRE(out.find("-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt") != std::string::npos);
}

TEST_CASE("C4M: encode with base", "[c4m][encoder]") {
    c4m::Manifest m;
    auto id = c4::ID::Identify("test");
    m.SetBase(id);

    std::string out = m.Encode();
    REQUIRE(out.find("@base ") != std::string::npos);
    REQUIRE(out.find(id.String()) != std::string::npos);
}

TEST_CASE("C4M: encode sorts entries", "[c4m][encoder]") {
    c4m::Manifest m;

    c4m::Entry dir;
    dir.mode = c4m::ModeDir | 0755;
    dir.timestamp = 1704067200;
    dir.size = 0;
    dir.name = "dir/";

    c4m::Entry file1;
    file1.mode = 0644;
    file1.timestamp = 1704067200;
    file1.size = 100;
    file1.name = "file10.txt";

    c4m::Entry file2;
    file2.mode = 0644;
    file2.timestamp = 1704067200;
    file2.size = 200;
    file2.name = "file2.txt";

    // Add in wrong order
    m.AddEntry(dir);
    m.AddEntry(file1);
    m.AddEntry(file2);

    std::string out = m.Encode();

    // Files before dirs, natural sort
    auto pos_f2 = out.find("file2.txt");
    auto pos_f10 = out.find("file10.txt");
    auto pos_dir = out.find("dir/");
    REQUIRE(pos_f2 < pos_f10);
    REQUIRE(pos_f10 < pos_dir);
}

// =============================================================
// Round-trip: parse -> encode -> parse
// =============================================================

TEST_CASE("C4M: round-trip basic manifest", "[c4m][roundtrip]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file1.txt\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 file2.txt\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 src/\n"
        "  -rw-r--r-- 2024-01-01T00:00:00Z 512 main.cpp\n";
    auto m = c4m::Manifest::Parse(input);
    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);

    REQUIRE(m2.Version() == "1.0");
    REQUIRE(m2.EntryCount() == m.EntryCount());
    for (size_t i = 0; i < m.EntryCount(); i++) {
        REQUIRE(m2.Entries()[i].name == m.Entries()[i].name);
        REQUIRE(m2.Entries()[i].mode == m.Entries()[i].mode);
        REQUIRE(m2.Entries()[i].size == m.Entries()[i].size);
    }
}

TEST_CASE("C4M: round-trip with C4 IDs", "[c4m][roundtrip]") {
    auto id = c4::ID::Identify("hello world");
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-06-15T10:30:00Z 11 hello.txt " + id.String() + "\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].id == id);

    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);
    REQUIRE(m2.Entries()[0].id == id);
}

TEST_CASE("C4M: round-trip with symlink", "[c4m][roundtrip]") {
    std::string input =
        "@c4m 1.0\n"
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> target.txt\n";
    auto m = c4m::Manifest::Parse(input);
    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);

    REQUIRE(m2.EntryCount() == 1);
    REQUIRE(m2.Entries()[0].name == "link");
    REQUIRE(m2.Entries()[0].target == "target.txt");
    REQUIRE(m2.Entries()[0].IsSymlink());
}

TEST_CASE("C4M: round-trip with quoted name", "[c4m][roundtrip]") {
    std::string input =
        "@c4m 1.0\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 \"file with spaces.txt\"\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.Entries()[0].name == "file with spaces.txt");

    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);
    REQUIRE(m2.Entries()[0].name == "file with spaces.txt");
}

// =============================================================
// Manifest operations
// =============================================================

TEST_CASE("C4M: manifest sort files before dirs", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry dir;
    dir.name = "dir/";
    dir.mode = c4m::ModeDir | 0755;
    c4m::Entry file;
    file.name = "file.txt";
    file.mode = 0644;

    m.AddEntry(dir);
    m.AddEntry(file);
    m.SortEntries();

    REQUIRE(m.Entries()[0].name == "file.txt");
    REQUIRE(m.Entries()[1].name == "dir/");
}

TEST_CASE("C4M: manifest sort natural order", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e1;
    e1.name = "file10.txt";
    c4m::Entry e2;
    e2.name = "file2.txt";
    c4m::Entry e3;
    e3.name = "file1.txt";

    m.AddEntry(e1);
    m.AddEntry(e2);
    m.AddEntry(e3);
    m.SortEntries();

    REQUIRE(m.Entries()[0].name == "file1.txt");
    REQUIRE(m.Entries()[1].name == "file2.txt");
    REQUIRE(m.Entries()[2].name == "file10.txt");
}

TEST_CASE("C4M: manifest validate", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "file.txt";
    m.AddEntry(e);
    REQUIRE_NOTHROW(m.Validate());
}

TEST_CASE("C4M: manifest validate rejects empty name", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "";
    m.AddEntry(e);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: manifest validate rejects duplicates", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e1;
    e1.name = "file.txt";
    c4m::Entry e2;
    e2.name = "file.txt";
    m.AddEntry(e1);
    m.AddEntry(e2);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: manifest validate rejects path traversal", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "../file.txt";
    m.AddEntry(e);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: manifest GetEntry", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "file.txt";
    m.AddEntry(e);

    REQUIRE(m.GetEntry("file.txt") != nullptr);
    REQUIRE(m.GetEntry("file.txt")->name == "file.txt");
    REQUIRE(m.GetEntry("nonexistent") == nullptr);
}

TEST_CASE("C4M: manifest RootID is deterministic", "[c4m][manifest]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1704067200;
    e.size = 100;
    e.name = "file.txt";
    e.id = c4::ID::Identify("content");
    m.AddEntry(e);

    auto id1 = m.RootID();
    auto id2 = m.RootID();
    REQUIRE_FALSE(id1.IsNil());
    REQUIRE(id1 == id2);
}

// =============================================================
// Cross-language compatibility: Go test vectors
// =============================================================

TEST_CASE("C4M: cross-language mode encoding", "[c4m][compat]") {
    // These must match Go's formatMode() output exactly
    REQUIRE(c4m::FormatMode(0644) == "-rw-r--r--");
    REQUIRE(c4m::FormatMode(0755) == "-rwxr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeDir | 0755) == "drwxr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeSymlink | 0777) == "lrwxrwxrwx");
    REQUIRE(c4m::FormatMode(c4m::ModeSetuid | 0755) == "-rwsr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeSetgid | 0755) == "-rwxr-sr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeDir | c4m::ModeSticky | 0777) == "drwxrwxrwt");
}

TEST_CASE("C4M: Go-compatible manifest encoding", "[c4m][compat]") {
    // Match Go example_test.go ExampleNewEncoder output
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644;
    // 2025-01-01T12:00:00Z = 1735732800
    e.timestamp = 1735732800;
    e.size = 13;
    e.name = "hello.txt";
    e.id = c4::ID::Identify("Hello, World!");
    m.AddEntry(e);

    std::string out = m.Encode();
    // Should contain the canonical entry line
    std::string expected_line = "-rw-r--r-- 2025-01-01T12:00:00Z 13 hello.txt " +
                                 e.id.String();
    REQUIRE(out.find(expected_line) != std::string::npos);
}

TEST_CASE("C4M: parse Go-produced manifest", "[c4m][compat]") {
    // A manifest that Go's encoder would produce (from Go test vectors)
    std::string go_manifest =
        "@c4m 1.0\n"
        "-rw-r--r-- 2025-09-19T12:00:00Z 100 test.txt "
        "c44aMtvPeoSPUFTRQNy6yj44qjrYtaJT4i9SzzNH2hiFHoYpjc5ecDzrz9jzuNBUgbqzHH7pYjSatjeoyh8C1UX4Bp\n"
        "drwxr-xr-x 2025-09-19T12:00:00Z 200 dir/\n"
        "  -rw-r--r-- 2025-09-19T12:00:00Z 200 file.txt "
        "c44aMtvPeoSPUFTRQNy6yj44qjrYtaJT4i9SzzNH2hiFHoYpjc5ecDzrz9jzuNBUgbqzHH7pYjSatjeoyh8C1UX4Bp\n";

    auto m = c4m::Manifest::Parse(go_manifest);
    REQUIRE(m.Version() == "1.0");
    REQUIRE(m.EntryCount() == 3);

    REQUIRE(m.Entries()[0].name == "test.txt");
    REQUIRE(m.Entries()[0].mode == 0644);
    REQUIRE(m.Entries()[0].size == 100);
    REQUIRE_FALSE(m.Entries()[0].id.IsNil());

    REQUIRE(m.Entries()[1].name == "dir/");
    REQUIRE(m.Entries()[1].IsDir());
    REQUIRE(m.Entries()[1].depth == 0);

    REQUIRE(m.Entries()[2].name == "file.txt");
    REQUIRE(m.Entries()[2].depth == 1);
}
