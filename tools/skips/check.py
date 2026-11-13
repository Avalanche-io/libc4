#!/usr/bin/env python3
"""Fail when a test skipped that the allow-list does not name.

A port once diverged from a shared vector because the comparison test was
skipped and nothing noticed. This reads what ctest wrote with
--output-junit (CMake 3.21 or later) and prints every test that did not
run, with the reason it gave, and every Catch2 test hidden with a [.] tag,
which ctest never sees:

    ctest --test-dir build --output-junit junit.xml
    python3 tools/skips/check.py --junit build/junit.xml --build build

A Catch2 SKIP(), in the test or in one of its sections, exits 4, which
catch_discover_tests makes a ctest skip; its message is the reason. A test
whose executable is missing, or that is DISABLED, is a skip too.

The allow-list (tools/skips/allowed-skips.txt) holds one skip per line: the
OS it is allowed on ("linux", "darwin", "windows", or "*"), then the test's
name as ctest and Catch2 give it, with the reason after "#". The script
exits 1 when a skip or a hidden test is not listed for this OS (with
--report it prints the same and exits 0); a listed skip that did not happen
is reported, not failed on, since a skip depends on what the machine has.
It exits 2 when it cannot read its inputs. Standard library only.
"""

import argparse
import json
import os
import platform
import subprocess
import sys
import xml.etree.ElementTree as ET

OSES = ("linux", "darwin", "windows")


def this_os():
    return {"Linux": "linux", "Darwin": "darwin", "Windows": "windows"}.get(
        platform.system(), platform.system().lower())


def read_allow(path, osname):
    """Return the test names the list allows to skip on osname."""
    out = set()
    with open(path, encoding="utf-8") as f:
        for n, raw in enumerate(f, 1):
            line = raw.split("#", 1)[0].strip()
            if not line:
                continue
            fields = line.split(None, 1)
            if len(fields) != 2:
                raise ValueError("%s:%d: want an OS and a test name, have %r"
                                 % (path, n, raw.rstrip("\n")))
            if fields[0] != "*" and fields[0] not in OSES:
                raise ValueError("%s:%d: OS %r is not one of *, %s"
                                 % (path, n, fields[0], ", ".join(OSES)))
            if fields[0] in ("*", osname):
                out.add(fields[1])
    return out


def skip_reason(case):
    """The reason a skipped testcase gave: Catch2's SKIP messages, else
    what ctest said."""
    text = case.findtext("system-out") or ""
    reasons = []
    lines = text.splitlines()
    for i, l in enumerate(lines):
        if l.rstrip().endswith(" SKIPPED:"):
            j = i + 1
            if j < len(lines) and lines[j].startswith("explicitly with message"):
                msg = []
                j += 1
                while j < len(lines) and lines[j].startswith("  "):
                    msg.append(lines[j].strip())
                    j += 1
                reasons.append(" ".join(msg))
            else:
                reasons.append("SKIP() with no message")
    if reasons:
        return "; ".join(reasons)
    sk = case.find("skipped")
    if sk is not None and sk.get("message"):
        return sk.get("message")
    return "status " + (case.get("status") or "unknown")


def junit_skips(path):
    """Return [(name, reason)] for every testcase that did not run, and the
    number of testcases."""
    root = ET.parse(path).getroot()
    cases = root.iter("testcase")
    skips, n = [], 0
    for c in cases:
        n += 1
        if c.get("status") in ("notrun", "disabled") or c.find("skipped") is not None:
            skips.append((c.get("name"), skip_reason(c)))
    return skips, n


def catch2_executables(build):
    """The executables of the tests catch_discover_tests made: those whose
    SKIP_RETURN_CODE is Catch2's 4."""
    out = subprocess.run(["ctest", "--test-dir", build, "--show-only=json-v1"],
                         stdout=subprocess.PIPE, check=True).stdout
    exes = set()
    for t in json.loads(out.decode("utf-8")).get("tests", []):
        props = {p["name"]: p["value"] for p in t.get("properties", [])}
        if props.get("SKIP_RETURN_CODE") == 4 and t.get("command"):
            exes.add(t["command"][0])
    return sorted(exes)


def hidden_tests(exe):
    """Return the names of the test cases a Catch2 executable hides with a
    [.] tag; ctest's discovery never lists them, so they never run."""
    out = subprocess.run([exe, "--list-tests", "--reporter", "xml", "[.]"],
                         stdout=subprocess.PIPE, check=False).stdout
    root = ET.fromstring(out)
    return [tc.findtext("Name") for tc in root.iter("TestCase")]


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--junit", required=True, help="ctest --output-junit file")
    ap.add_argument("--build", help="the build tree, to find hidden Catch2 tests")
    ap.add_argument("--allow", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "allowed-skips.txt"))
    ap.add_argument("--os", default=this_os(), help="the OS the tests ran on")
    ap.add_argument("--report", action="store_true", help="report, never fail")
    a = ap.parse_args()

    try:
        allowed = read_allow(a.allow, a.os)
        skips, n = junit_skips(a.junit)
        hidden = []
        if a.build:
            for exe in catch2_executables(a.build):
                for name in hidden_tests(exe):
                    hidden.append((name, "hidden by a [.] tag in " + os.path.basename(exe)))
    except (OSError, ValueError, ET.ParseError, subprocess.CalledProcessError) as e:
        print("skips: %s" % e)
        return 2
    if n == 0:
        print("skips: %s holds no testcases" % a.junit)
        return 2

    found = sorted(skips + hidden)
    unlisted = [s for s in found if s[0] not in allowed]
    print("skips: %d tests in %s, %d skipped, %d hidden (os %s)"
          % (n, a.junit, len(skips), len(hidden), a.os))
    for name, reason in found:
        mark = "allowed" if name in allowed else "NOT ALLOWED"
        print("  %-11s %s: %s" % (mark, name, reason))
    happened = {s[0] for s in found}
    for name in sorted(allowed - happened):
        print("  listed, did not skip: %s" % name)
    if unlisted:
        print("skips: %d skipped tests are not in %s for %s%s"
              % (len(unlisted), os.path.relpath(a.allow), a.os, " (report mode)" if a.report else ""))
        return 0 if a.report else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
