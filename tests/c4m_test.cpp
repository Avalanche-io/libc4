// SPDX-License-Identifier: Apache-2.0
// C4M format tests -- parser, encoder, natural sort, mode/timestamp formatting,
// SafeName, hard links, flow links, interop.
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
    int64_t ts = c4m::ParseTimestamp("2024-06-15T10:30:00+05:00");
    std::string formatted = c4m::FormatTimestamp(ts);
    REQUIRE(formatted == "2024-06-15T05:30:00Z");
}

TEST_CASE("C4M: parse timestamp negative offset", "[c4m][timestamp]") {
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
// Entry formatting -- INTEROP: canonical vs display
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

TEST_CASE("C4M: entry with backslash-escaped name (not quoted)", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "my file.txt";

    std::string line = e.Canonical();
    // Go uses backslash escaping, not quoting
    REQUIRE(line.find("my\\ file.txt") != std::string::npos);
    // Must NOT contain quoted form
    REQUIRE(line.find("\"my file.txt\"") == std::string::npos);
}

TEST_CASE("C4M: canonical null mode is single dash", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0;
    e.timestamp = 0;
    e.size = -1;
    e.name = "unknown.txt";

    std::string canonical = e.Canonical();
    // Canonical: null mode is "-" (single dash)
    REQUIRE(canonical.substr(0, 2) == "- ");
    REQUIRE(canonical.find("----------") == std::string::npos);
}

TEST_CASE("C4M: display null mode is ten dashes", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0;
    e.timestamp = 0;
    e.size = -1;
    e.name = "unknown.txt";

    std::string display = e.Format(0);
    // Format (display): null mode is "----------"
    REQUIRE(display.find("----------") != std::string::npos);
}

TEST_CASE("C4M: null C4 ID always emitted as dash", "[c4m][entry]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    // id is nil by default

    std::string line = e.Canonical();
    // Must end with " -"
    REQUIRE(line.size() >= 2);
    REQUIRE(line.substr(line.size() - 2) == " -");
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
// Hard link formatting
// =============================================================

TEST_CASE("C4M: entry hard link ungrouped", "[c4m][entry][hardlink]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    e.hard_link = -1;

    std::string line = e.Canonical();
    REQUIRE(line.find(" -> ") != std::string::npos);
    // "-> " then " -" at end for null ID
}

TEST_CASE("C4M: entry hard link grouped", "[c4m][entry][hardlink]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    e.hard_link = 3;

    std::string line = e.Canonical();
    REQUIRE(line.find("->3") != std::string::npos);
}

// =============================================================
// Flow link formatting
// =============================================================

TEST_CASE("C4M: entry flow link outbound", "[c4m][entry][flow]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    e.flow_direction = c4m::FlowDirection::Outbound;
    e.flow_target = "studio:plates/";

    std::string line = e.Canonical();
    REQUIRE(line.find("-> studio:plates/") != std::string::npos);
}

TEST_CASE("C4M: entry flow link inbound", "[c4m][entry][flow]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    e.flow_direction = c4m::FlowDirection::Inbound;
    e.flow_target = "nas:archive/";

    std::string line = e.Canonical();
    REQUIRE(line.find("<- nas:archive/") != std::string::npos);
}

TEST_CASE("C4M: entry flow link bidirectional", "[c4m][entry][flow]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1705314600;
    e.size = 100;
    e.name = "file.txt";
    e.flow_direction = c4m::FlowDirection::Bidirectional;
    e.flow_target = "mirror:";

    std::string line = e.Canonical();
    REQUIRE(line.find("<> mirror:") != std::string::npos);
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
    REQUIRE(c4m::NaturalLess("abc", "123"));
}

TEST_CASE("C4M: natural sort shorter first", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("file", "file1"));
}

TEST_CASE("C4M: natural sort padding", "[c4m][sort]") {
    REQUIRE(c4m::NaturalLess("render.1.exr", "render.01.exr"));
}

// =============================================================
// SafeName / UnsafeName
// =============================================================

TEST_CASE("C4M: SafeName passthrough for clean names", "[c4m][safename]") {
    REQUIRE(c4m::SafeName("hello.txt") == "hello.txt");
    REQUIRE(c4m::SafeName("file with spaces.txt") == "file with spaces.txt");
    REQUIRE(c4m::SafeName("unicode_\xc3\xa9.txt") == "unicode_\xc3\xa9.txt"); // e-acute
}

TEST_CASE("C4M: SafeName escapes backslash", "[c4m][safename]") {
    REQUIRE(c4m::SafeName("file\\name") == "file\\\\name");
}

TEST_CASE("C4M: SafeName escapes control chars", "[c4m][safename]") {
    std::string raw = "file\ttab";
    std::string encoded = c4m::SafeName(raw);
    REQUIRE(encoded == "file\\ttab");
}

TEST_CASE("C4M: SafeName escapes null byte", "[c4m][safename]") {
    std::string raw = std::string("file\0null", 9);
    std::string encoded = c4m::SafeName(raw);
    REQUIRE(encoded == "file\\0null");
}

TEST_CASE("C4M: SafeName escapes newline and carriage return", "[c4m][safename]") {
    REQUIRE(c4m::SafeName("a\nb") == "a\\nb");
    REQUIRE(c4m::SafeName("a\rb") == "a\\rb");
}

TEST_CASE("C4M: SafeName round-trip", "[c4m][safename]") {
    // Test various strings round-trip through SafeName/UnsafeName
    std::vector<std::string> inputs = {
        "hello.txt",
        "file with spaces",
        "file\\backslash",
        std::string("file\0null", 9),
        "file\ttab",
        "file\nnewline",
        "file\rreturn",
    };
    for (const auto &raw : inputs) {
        std::string encoded = c4m::SafeName(raw);
        std::string decoded = c4m::UnsafeName(encoded);
        REQUIRE(decoded == raw);
    }
}

TEST_CASE("C4M: SafeName tier 3 braille encoding for non-printable", "[c4m][safename]") {
    // Invalid UTF-8 byte 0xFF should be encoded as tier 3 braille
    std::string raw;
    raw += "abc";
    raw += static_cast<char>(0xFF);
    raw += "def";

    std::string encoded = c4m::SafeName(raw);
    // Should contain currency sign delimiters
    REQUIRE(encoded.find("\xc2\xa4") != std::string::npos); // currency sign U+00A4

    // Round-trip
    REQUIRE(c4m::UnsafeName(encoded) == raw);
}

// =============================================================
// EscapeField / UnescapeField
// =============================================================

TEST_CASE("C4M: EscapeField escapes space", "[c4m][safename]") {
    REQUIRE(c4m::EscapeField("my file", false) == "my\\ file");
}

TEST_CASE("C4M: EscapeField escapes double-quote", "[c4m][safename]") {
    REQUIRE(c4m::EscapeField("say\"hello", false) == "say\\\"hello");
}

TEST_CASE("C4M: EscapeField escapes brackets for non-sequence", "[c4m][safename]") {
    REQUIRE(c4m::EscapeField("file[1]", false) == "file\\[1\\]");
}

TEST_CASE("C4M: EscapeField preserves brackets for sequence", "[c4m][safename]") {
    REQUIRE(c4m::EscapeField("file[1-10]", true) == "file[1-10]");
}

TEST_CASE("C4M: UnescapeField reverses escapes", "[c4m][safename]") {
    REQUIRE(c4m::UnescapeField("my\\ file") == "my file");
    REQUIRE(c4m::UnescapeField("say\\\"hello") == "say\"hello");
    REQUIRE(c4m::UnescapeField("file\\[1\\]") == "file[1]");
}

// =============================================================
// Parser: headerless entry-only format (Go interop)
// =============================================================

TEST_CASE("C4M: parse empty input", "[c4m][parser]") {
    auto m = c4m::Manifest::Parse("");
    REQUIRE(m.EntryCount() == 0);
}

TEST_CASE("C4M: parse single file (no header)", "[c4m][parser]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "file.txt");
    REQUIRE(e.mode == 0644);
    REQUIRE(e.size == 100);
    REQUIRE(e.depth == 0);
    REQUIRE(e.id.IsNil());
}

TEST_CASE("C4M: parse directory (no header)", "[c4m][parser]") {
    std::string input =
        "drwxr-xr-x 2024-01-01T00:00:00Z 4096 src/ -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "src/");
    REQUIRE(e.IsDir());
    REQUIRE(e.mode == (c4m::ModeDir | 0755));
    REQUIRE(e.size == 4096);
}

TEST_CASE("C4M: parse symlink (no header)", "[c4m][parser]") {
    std::string input =
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> target -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.name == "link");
    REQUIRE(e.target == "target");
    REQUIRE(e.IsSymlink());
}

TEST_CASE("C4M: parse symlink with absolute path target", "[c4m][parser]") {
    std::string input =
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> /absolute/path/target -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].target == "/absolute/path/target");
}

TEST_CASE("C4M: parse backslash-escaped filename", "[c4m][parser]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 2048 my\\ file.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].name == "my file.txt");
    REQUIRE(m.Entries()[0].size == 2048);
}

TEST_CASE("C4M: parse file with C4 ID", "[c4m][parser]") {
    std::string input =
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

TEST_CASE("C4M: parse indented entries (no header)", "[c4m][parser]") {
    std::string input =
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 src/ -\n"
        "  -rw-r--r-- 2024-01-01T00:00:00Z 512 main.cpp -\n"
        "  drwxr-xr-x 2024-01-01T00:00:00Z 0 include/ -\n"
        "    -rw-r--r-- 2024-01-01T00:00:00Z 256 header.hpp -\n";
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

TEST_CASE("C4M: parse multiple entries (no header)", "[c4m][parser]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file1.txt -\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 file2.txt -\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 dir/ -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 3);
}

TEST_CASE("C4M: parse with null values", "[c4m][parser]") {
    // Canonical null mode is single "-"
    std::string input = "- - - unknown.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);

    const auto &e = m.Entries()[0];
    REQUIRE(e.mode == 0);
    REQUIRE(e.timestamp == 0);
    REQUIRE(e.size == -1);
    REQUIRE(e.name == "unknown.txt");
}

TEST_CASE("C4M: parse with ten-dash null mode", "[c4m][parser]") {
    // Display format null mode is "----------"
    std::string input = "---------- - - unknown.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].mode == 0);
}

TEST_CASE("C4M: parse with comma-formatted size", "[c4m][parser]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 1,234,567 large.bin -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].size == 1234567);
}

// =============================================================
// Parser: reject CR (INTEROP FIX 5)
// =============================================================

TEST_CASE("C4M: parser rejects CR in line", "[c4m][parser]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -\r\n";
    REQUIRE_THROWS(c4m::Manifest::Parse(input));
}

// =============================================================
// Parser: directives
// =============================================================

TEST_CASE("C4M: parser rejects @c4m header", "[c4m][parser]") {
    std::string input = "@c4m 1.0\n";
    REQUIRE_THROWS(c4m::Manifest::Parse(input));
}

TEST_CASE("C4M: parser rejects @base directive", "[c4m][parser]") {
    std::string input = "@base c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1\n";
    REQUIRE_THROWS(c4m::Manifest::Parse(input));
}

// =============================================================
// Parser: hard links
// =============================================================

TEST_CASE("C4M: parse hard link ungrouped", "[c4m][parser][hardlink]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -> -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].hard_link == -1);
    REQUIRE(m.Entries()[0].target.empty());
}

TEST_CASE("C4M: parse hard link grouped", "[c4m][parser][hardlink]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt ->3 -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].hard_link == 3);
}

// =============================================================
// Parser: flow links
// =============================================================

TEST_CASE("C4M: parse flow link outbound", "[c4m][parser][flow]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -> studio:plates/ -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].flow_direction == c4m::FlowDirection::Outbound);
    REQUIRE(m.Entries()[0].flow_target == "studio:plates/");
}

TEST_CASE("C4M: parse flow link inbound", "[c4m][parser][flow]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt <- nas:archive/ -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].flow_direction == c4m::FlowDirection::Inbound);
    REQUIRE(m.Entries()[0].flow_target == "nas:archive/");
}

TEST_CASE("C4M: parse flow link bidirectional", "[c4m][parser][flow]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt <> mirror: -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.EntryCount() == 1);
    REQUIRE(m.Entries()[0].flow_direction == c4m::FlowDirection::Bidirectional);
    REQUIRE(m.Entries()[0].flow_target == "mirror:");
}

// =============================================================
// Parser: inline ID list handling
// =============================================================

TEST_CASE("C4M: parser skips inline ID list", "[c4m][parser]") {
    // Two C4 IDs concatenated (180 chars, multiple of 90)
    std::string id1 = "c43zYcLni5LF9rR4Lg4B8h3Jp8SBwjcnyyeh4bc6gTPHndKuKdjUWx1kJPYhZxYt3zV6tQXpDs2shPsPYjgG81wZM1";
    std::string id2 = "c44aMtvPeoSPUFTRQNy6yj44qjrYtaJT4i9SzzNH2hiFHoYpjc5ecDzrz9jzuNBUgbqzHH7pYjSatjeoyh8C1UX4Bp";
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -\n" +
        id1 + id2 + "\n" +
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 file2.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    // Inline ID list should be skipped, only the two entries remain
    REQUIRE(m.EntryCount() == 2);
    REQUIRE(m.Entries()[0].name == "file.txt");
    REQUIRE(m.Entries()[1].name == "file2.txt");
}

// =============================================================
// Encoder
// =============================================================

TEST_CASE("C4M: encode empty manifest", "[c4m][encoder]") {
    c4m::Manifest m;
    std::string out = m.Encode();
    REQUIRE(out.empty());
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
    REQUIRE(out.find("@c4m") == std::string::npos);
    REQUIRE(out.find("-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt") != std::string::npos);
}

TEST_CASE("C4M: encode does not emit base", "[c4m][encoder]") {
    c4m::Manifest m;
    auto id = c4::ID::Identify("test");
    m.SetBase(id);

    std::string out = m.Encode();
    REQUIRE(out.find("@base") == std::string::npos);
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

    m.AddEntry(dir);
    m.AddEntry(file1);
    m.AddEntry(file2);

    std::string out = m.Encode();

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
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file1.txt -\n"
        "-rw-r--r-- 2024-01-01T00:00:00Z 200 file2.txt -\n"
        "drwxr-xr-x 2024-01-01T00:00:00Z 0 src/ -\n"
        "  -rw-r--r-- 2024-01-01T00:00:00Z 512 main.cpp -\n";
    auto m = c4m::Manifest::Parse(input);
    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);

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
        "lrwxrwxrwx 2024-01-01T00:00:00Z 0 link -> target.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);

    REQUIRE(m2.EntryCount() == 1);
    REQUIRE(m2.Entries()[0].name == "link");
    REQUIRE(m2.Entries()[0].target == "target.txt");
    REQUIRE(m2.Entries()[0].IsSymlink());
}

TEST_CASE("C4M: round-trip with backslash-escaped name", "[c4m][roundtrip]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file\\ with\\ spaces.txt -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.Entries()[0].name == "file with spaces.txt");

    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);
    REQUIRE(m2.Entries()[0].name == "file with spaces.txt");
}

TEST_CASE("C4M: round-trip hard link", "[c4m][roundtrip]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt ->3 -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.Entries()[0].hard_link == 3);

    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);
    REQUIRE(m2.Entries()[0].hard_link == 3);
}

TEST_CASE("C4M: round-trip flow link", "[c4m][roundtrip]") {
    std::string input =
        "-rw-r--r-- 2024-01-01T00:00:00Z 100 file.txt -> studio:plates/ -\n";
    auto m = c4m::Manifest::Parse(input);
    REQUIRE(m.Entries()[0].flow_direction == c4m::FlowDirection::Outbound);
    REQUIRE(m.Entries()[0].flow_target == "studio:plates/");

    std::string encoded = m.Encode();
    auto m2 = c4m::Manifest::Parse(encoded);
    REQUIRE(m2.Entries()[0].flow_direction == c4m::FlowDirection::Outbound);
    REQUIRE(m2.Entries()[0].flow_target == "studio:plates/");
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
    REQUIRE(c4m::FormatMode(0644) == "-rw-r--r--");
    REQUIRE(c4m::FormatMode(0755) == "-rwxr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeDir | 0755) == "drwxr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeSymlink | 0777) == "lrwxrwxrwx");
    REQUIRE(c4m::FormatMode(c4m::ModeSetuid | 0755) == "-rwsr-xr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeSetgid | 0755) == "-rwxr-sr-x");
    REQUIRE(c4m::FormatMode(c4m::ModeDir | c4m::ModeSticky | 0777) == "drwxrwxrwt");
}

TEST_CASE("C4M: Go-compatible manifest encoding", "[c4m][compat]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1735732800;
    e.size = 13;
    e.name = "hello.txt";
    e.id = c4::ID::Identify("Hello, World!");
    m.AddEntry(e);

    std::string out = m.Encode();
    std::string expected_line = "-rw-r--r-- 2025-01-01T12:00:00Z 13 hello.txt " +
                                 e.id.String();
    REQUIRE(out.find(expected_line) != std::string::npos);
}

TEST_CASE("C4M: parse Go-produced manifest (entry-only)", "[c4m][compat]") {
    // Go encoder produces entry-only output (no header)
    std::string go_manifest =
        "-rw-r--r-- 2025-09-19T12:00:00Z 100 test.txt "
        "c44aMtvPeoSPUFTRQNy6yj44qjrYtaJT4i9SzzNH2hiFHoYpjc5ecDzrz9jzuNBUgbqzHH7pYjSatjeoyh8C1UX4Bp\n"
        "drwxr-xr-x 2025-09-19T12:00:00Z 200 dir/ -\n"
        "  -rw-r--r-- 2025-09-19T12:00:00Z 200 file.txt "
        "c44aMtvPeoSPUFTRQNy6yj44qjrYtaJT4i9SzzNH2hiFHoYpjc5ecDzrz9jzuNBUgbqzHH7pYjSatjeoyh8C1UX4Bp\n";

    auto m = c4m::Manifest::Parse(go_manifest);
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

// =============================================================
// Tree index and navigation
// =============================================================

// Helper: build a nested manifest for navigation tests.
// Structure:
//   file1.txt (depth 0)
//   file2.txt (depth 0)
//   src/ (depth 0)
//     main.cpp (depth 1)
//     include/ (depth 1)
//       header.hpp (depth 2)
//   docs/ (depth 0)
//     readme.txt (depth 1)
static c4m::Manifest makeNestedManifest() {
    c4m::Manifest m;

    c4m::Entry file1; file1.name = "file1.txt"; file1.depth = 0;
    c4m::Entry file2; file2.name = "file2.txt"; file2.depth = 0;

    c4m::Entry src; src.name = "src/"; src.mode = c4m::ModeDir | 0755; src.depth = 0;
    c4m::Entry main_cpp; main_cpp.name = "main.cpp"; main_cpp.depth = 1;
    c4m::Entry include; include.name = "include/"; include.mode = c4m::ModeDir | 0755; include.depth = 1;
    c4m::Entry header; header.name = "header.hpp"; header.depth = 2;

    c4m::Entry docs; docs.name = "docs/"; docs.mode = c4m::ModeDir | 0755; docs.depth = 0;
    c4m::Entry readme; readme.name = "readme.txt"; readme.depth = 1;

    m.AddEntry(file1);
    m.AddEntry(file2);
    m.AddEntry(src);
    m.AddEntry(main_cpp);
    m.AddEntry(include);
    m.AddEntry(header);
    m.AddEntry(docs);
    m.AddEntry(readme);
    return m;
}

TEST_CASE("C4M: GetEntry by full path", "[c4m][tree]") {
    auto m = makeNestedManifest();
    REQUIRE(m.GetEntry("file1.txt") != nullptr);
    REQUIRE(m.GetEntry("file1.txt")->name == "file1.txt");
    REQUIRE(m.GetEntry("src/main.cpp") != nullptr);
    REQUIRE(m.GetEntry("src/main.cpp")->name == "main.cpp");
    REQUIRE(m.GetEntry("src/include/header.hpp") != nullptr);
    REQUIRE(m.GetEntry("src/include/header.hpp")->name == "header.hpp");
    REQUIRE(m.GetEntry("docs/readme.txt") != nullptr);
    REQUIRE(m.GetEntry("nonexistent") == nullptr);
}

TEST_CASE("C4M: GetEntryByName bare name", "[c4m][tree]") {
    auto m = makeNestedManifest();
    REQUIRE(m.GetEntryByName("main.cpp") != nullptr);
    REQUIRE(m.GetEntryByName("main.cpp")->name == "main.cpp");
    REQUIRE(m.GetEntryByName("src/") != nullptr);
    REQUIRE(m.GetEntryByName("nonexistent") == nullptr);
}

TEST_CASE("C4M: EntryPath returns full path", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *header = m.GetEntry("src/include/header.hpp");
    REQUIRE(header != nullptr);
    REQUIRE(m.EntryPath(header) == "src/include/header.hpp");

    const auto *src = m.GetEntryByName("src/");
    REQUIRE(m.EntryPath(src) == "src/");
    REQUIRE(m.EntryPath(nullptr) == "");
}

TEST_CASE("C4M: Children of directory", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *src = m.GetEntryByName("src/");
    REQUIRE(src != nullptr);
    auto kids = m.Children(src);
    REQUIRE(kids.size() == 2);
    // Children are main.cpp and include/
    bool found_main = false, found_include = false;
    for (const auto *k : kids) {
        if (k->name == "main.cpp") found_main = true;
        if (k->name == "include/") found_include = true;
    }
    REQUIRE(found_main);
    REQUIRE(found_include);
}

TEST_CASE("C4M: Children of file returns empty", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *file = m.GetEntry("file1.txt");
    REQUIRE(m.Children(file).empty());
    REQUIRE(m.Children(nullptr).empty());
}

TEST_CASE("C4M: Parent of nested entry", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *header = m.GetEntry("src/include/header.hpp");
    REQUIRE(header != nullptr);
    const auto *parent = m.Parent(header);
    REQUIRE(parent != nullptr);
    REQUIRE(parent->name == "include/");

    const auto *grandparent = m.Parent(parent);
    REQUIRE(grandparent != nullptr);
    REQUIRE(grandparent->name == "src/");

    // Root-level entry has no parent
    const auto *root_file = m.GetEntry("file1.txt");
    REQUIRE(m.Parent(root_file) == nullptr);
    REQUIRE(m.Parent(nullptr) == nullptr);
}

TEST_CASE("C4M: Siblings of entry", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *file1 = m.GetEntry("file1.txt");
    auto sibs = m.Siblings(file1);
    // Siblings at root: file2.txt, src/, docs/
    REQUIRE(sibs.size() == 3);

    const auto *main_cpp = m.GetEntry("src/main.cpp");
    auto main_sibs = m.Siblings(main_cpp);
    // Siblings under src/: include/
    REQUIRE(main_sibs.size() == 1);
    REQUIRE(main_sibs[0]->name == "include/");

    REQUIRE(m.Siblings(nullptr).empty());
}

TEST_CASE("C4M: Ancestors inner-to-outer", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *header = m.GetEntry("src/include/header.hpp");
    auto anc = m.Ancestors(header);
    // [include/, src/]
    REQUIRE(anc.size() == 2);
    REQUIRE(anc[0]->name == "include/");
    REQUIRE(anc[1]->name == "src/");

    // Root entry has no ancestors
    REQUIRE(m.Ancestors(m.GetEntry("file1.txt")).empty());
    REQUIRE(m.Ancestors(nullptr).empty());
}

TEST_CASE("C4M: Descendants of directory", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *src = m.GetEntryByName("src/");
    auto descs = m.Descendants(src);
    // main.cpp, include/, header.hpp
    REQUIRE(descs.size() == 3);

    // docs/ has one descendant
    const auto *docs = m.GetEntryByName("docs/");
    REQUIRE(m.Descendants(docs).size() == 1);

    // File has no descendants
    REQUIRE(m.Descendants(m.GetEntry("file1.txt")).empty());
    REQUIRE(m.Descendants(nullptr).empty());
}

TEST_CASE("C4M: Root returns depth-0 entries", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto roots = m.Root();
    REQUIRE(roots.size() == 4); // file1.txt, file2.txt, src/, docs/
    for (const auto *r : roots)
        REQUIRE(r->depth == 0);
}

TEST_CASE("C4M: PathList returns sorted paths", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto paths = m.PathList();
    REQUIRE(paths.size() == 8);
    // Verify sorted
    for (size_t i = 1; i < paths.size(); i++)
        REQUIRE(paths[i - 1] <= paths[i]);
    // Check some specific paths
    bool found = false;
    for (const auto &p : paths) {
        if (p == "src/include/header.hpp") found = true;
    }
    REQUIRE(found);
}

TEST_CASE("C4M: RemoveEntry removes entry and descendants", "[c4m][tree]") {
    auto m = makeNestedManifest();
    REQUIRE(m.EntryCount() == 8);

    const auto *src = m.GetEntryByName("src/");
    m.RemoveEntry(src);
    // src/ + main.cpp + include/ + header.hpp = 4 removed
    REQUIRE(m.EntryCount() == 4);
    REQUIRE(m.GetEntry("src/main.cpp") == nullptr);
    REQUIRE(m.GetEntry("file1.txt") != nullptr);
}

TEST_CASE("C4M: RemoveEntry single file", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *file1 = m.GetEntry("file1.txt");
    m.RemoveEntry(file1);
    REQUIRE(m.EntryCount() == 7);
    REQUIRE(m.GetEntry("file1.txt") == nullptr);
}

TEST_CASE("C4M: MoveEntry to root", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *header = m.GetEntry("src/include/header.hpp");
    m.MoveEntry(header, nullptr, "moved.hpp");

    // Entry should now be at root
    const auto *moved = m.GetEntryByName("moved.hpp");
    REQUIRE(moved != nullptr);
    REQUIRE(moved->depth == 0);
}

TEST_CASE("C4M: MoveEntry directory updates descendants", "[c4m][tree]") {
    auto m = makeNestedManifest();
    const auto *include = m.GetEntry("src/include/");
    const auto *docs = m.GetEntryByName("docs/");
    m.MoveEntry(include, docs, "inc/");

    // include/ should now be under docs/ at depth 1
    const auto *moved = m.GetEntryByName("inc/");
    REQUIRE(moved != nullptr);
    REQUIRE(moved->depth == 1);

    // header.hpp should be at depth 2 (under docs/inc/)
    const auto *hp = m.GetEntryByName("header.hpp");
    REQUIRE(hp != nullptr);
    REQUIRE(hp->depth == 2);
}

TEST_CASE("C4M: Copy creates independent manifest", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto cp = m.Copy();

    REQUIRE(cp.EntryCount() == m.EntryCount());
    REQUIRE(cp.Version() == m.Version());

    // Modifying copy doesn't affect original
    cp.AddEntry(c4m::Entry{});
    REQUIRE(cp.EntryCount() == m.EntryCount() + 1);
    REQUIRE(m.EntryCount() == 8);
}

TEST_CASE("C4M: Canonicalize propagates size", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry dir;
    dir.name = "dir/"; dir.mode = c4m::ModeDir | 0755;
    dir.size = -1; dir.timestamp = 0; dir.depth = 0;

    c4m::Entry f1;
    f1.name = "a.txt"; f1.size = 100; f1.timestamp = 1000; f1.depth = 1;

    c4m::Entry f2;
    f2.name = "b.txt"; f2.size = 200; f2.timestamp = 2000; f2.depth = 1;

    m.AddEntry(dir);
    m.AddEntry(f1);
    m.AddEntry(f2);

    // Compute expected size: children sizes + canonical c4m content size
    int64_t expected_size = f1.size + f2.size;
    expected_size += static_cast<int64_t>(f1.Canonical().size()) + 1; // +1 for '\n'
    expected_size += static_cast<int64_t>(f2.Canonical().size()) + 1;

    m.Canonicalize();

    // Directory size = sum of children + byte length of canonical c4m content
    REQUIRE(m.Entries()[0].size == expected_size);
    // Directory timestamp should be most recent child
    REQUIRE(m.Entries()[0].timestamp == 2000);
}

TEST_CASE("C4M: Canonicalize nil-infectious size", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry dir;
    dir.name = "dir/"; dir.mode = c4m::ModeDir | 0755;
    dir.size = -1; dir.timestamp = 0; dir.depth = 0;

    c4m::Entry f1;
    f1.name = "a.txt"; f1.size = 100; f1.timestamp = 1000; f1.depth = 1;

    c4m::Entry f2;
    f2.name = "b.txt"; f2.size = -1; f2.timestamp = 2000; f2.depth = 1;

    m.AddEntry(dir);
    m.AddEntry(f1);
    m.AddEntry(f2);
    m.Canonicalize();

    // Null size is infectious
    REQUIRE(m.Entries()[0].size == -1);
}

TEST_CASE("C4M: ComputeC4ID is deterministic", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto id1 = m.ComputeC4ID();
    auto id2 = m.ComputeC4ID();
    REQUIRE_FALSE(id1.IsNil());
    REQUIRE(id1 == id2);
}

TEST_CASE("C4M: ComputeC4ID order independent", "[c4m][tree]") {
    // Two manifests with same entries in different order should have same ID
    c4m::Manifest m1, m2;
    c4m::Entry a; a.name = "a.txt"; a.mode = 0644; a.size = 10; a.timestamp = 1000;
    c4m::Entry b; b.name = "b.txt"; b.mode = 0644; b.size = 20; b.timestamp = 2000;

    m1.AddEntry(a); m1.AddEntry(b);
    m2.AddEntry(b); m2.AddEntry(a);

    REQUIRE(m1.ComputeC4ID() == m2.ComputeC4ID());
}

TEST_CASE("C4M: InvalidateIndex forces rebuild", "[c4m][tree]") {
    auto m = makeNestedManifest();
    // Access index
    REQUIRE(m.GetEntry("file1.txt") != nullptr);
    // Invalidate
    m.InvalidateIndex();
    // Should still work after rebuild
    REQUIRE(m.GetEntry("file1.txt") != nullptr);
}

TEST_CASE("C4M: FilterByPath glob matching", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto filtered = m.FilterByPath("*.txt");
    // Matches bare names: file1.txt, file2.txt, readme.txt
    REQUIRE(filtered.EntryCount() == 3);
}

TEST_CASE("C4M: FilterByPath wildcard", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto filtered = m.FilterByPath("*");
    // Matches all bare names
    REQUIRE(filtered.EntryCount() == m.EntryCount());
}

TEST_CASE("C4M: FilterByPrefix", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto filtered = m.FilterByPrefix("src/");
    // src/, src/main.cpp, src/include/, src/include/header.hpp
    REQUIRE(filtered.EntryCount() == 4);
}

TEST_CASE("C4M: FilterByPrefix no match", "[c4m][tree]") {
    auto m = makeNestedManifest();
    auto filtered = m.FilterByPrefix("nonexistent/");
    REQUIRE(filtered.EntryCount() == 0);
}

TEST_CASE("C4M: Validate accepts valid manifest", "[c4m][tree]") {
    auto m = makeNestedManifest();
    REQUIRE_NOTHROW(m.Validate());
}

TEST_CASE("C4M: Validate rejects path traversal names", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "../evil.txt";
    m.AddEntry(e);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: Validate rejects embedded slash", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = "path/file.txt";
    m.AddEntry(e);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: Validate rejects dot names", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry e;
    e.name = ".";
    m.AddEntry(e);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: Validate detects duplicate full paths", "[c4m][tree]") {
    c4m::Manifest m;
    c4m::Entry dir; dir.name = "dir/"; dir.mode = c4m::ModeDir | 0755; dir.depth = 0;
    c4m::Entry f1; f1.name = "file.txt"; f1.depth = 1;
    c4m::Entry f2; f2.name = "file.txt"; f2.depth = 1;
    m.AddEntry(dir);
    m.AddEntry(f1);
    m.AddEntry(f2);
    REQUIRE_THROWS(m.Validate());
}

TEST_CASE("C4M: HasNullValues entry method", "[c4m][tree]") {
    c4m::Entry e;
    e.mode = 0644;
    e.timestamp = 1000;
    e.size = 100;
    REQUIRE_FALSE(e.HasNullValues());

    e.size = -1;
    REQUIRE(e.HasNullValues());

    e.size = 100;
    e.timestamp = 0;
    REQUIRE(e.HasNullValues());

    e.timestamp = 1000;
    e.mode = 0;
    REQUIRE(e.HasNullValues());
}
