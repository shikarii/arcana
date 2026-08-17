#!/usr/bin/env python3
"""
check-limits.py — Enforce project-wide LOC limits for Arcana.

Rules:
  - Max 600 lines per .c / .h file
  - Max 60 lines per function body

Usage:
  py -3 tools/check-limits.py [--root DIR]

Exit code 0 = all pass, 1 = violations found.
"""

import os
import re
import sys
import argparse

FILE_LOC_LIMIT = 600
FUNC_LOC_LIMIT = 60

# Directories to scan (relative to project root)
SCAN_DIRS = ["src", "tests"]

# Patterns that start a function definition (column 0)
# Matches: returntype funcname(...)  {   or   TEST(name) {
FUNC_START_RE = re.compile(
    r"^(?:"
    r"(?:static\s+)?(?:void|int|bool|uint\w+|int\w+|float|double|char|const\s+\w+|Arc\w+\*?|Hir\w+\*?|Mir\w+\*?|Sem\w+)"
    r"\s+\*?\w+\s*\([^;]*\)\s*\{"
    r"|TEST\(\w+\)\s*\{"
    r")"
)


def find_files(root):
    """Find all .c and .h files under SCAN_DIRS."""
    files = []
    for d in SCAN_DIRS:
        dirpath = os.path.join(root, d)
        if not os.path.isdir(dirpath):
            continue
        for dirpath2, _, filenames in os.walk(dirpath):
            for fn in filenames:
                if fn.endswith(".c") or fn.endswith(".h"):
                    files.append(os.path.join(dirpath2, fn))
    return sorted(files)


def count_function_lines(filepath):
    """
    Find function bodies and measure their line count.
    Uses brace-depth tracking from column-0 opening brace.
    Returns list of (func_name, start_line, line_count).
    """
    results = []
    with open(filepath, "r", encoding="utf-8", errors="replace") as f:
        lines = f.readlines()

    i = 0
    while i < len(lines):
        line = lines[i]

        # Look for function start: line containing '{' where we can identify
        # a function signature on this or previous lines
        func_name = None

        # Check for TEST(name) {
        m = re.match(r"^TEST\((\w+)\)\s*\{", line)
        if m:
            func_name = m.group(1)

        # Check for typical C function definition
        if not func_name:
            m = re.match(
                r"^(?:static\s+)?(?:\w[\w\s\*]*?)\s+\*?(\w+)\s*\([^;]*\)\s*\{",
                line,
            )
            if m:
                func_name = m.group(1)

        # Also handle opening brace on next line
        if not func_name and i + 1 < len(lines):
            if re.match(r"^\{\s*$", lines[i + 1]):
                m = re.match(
                    r"^(?:static\s+)?(?:\w[\w\s\*]*?)\s+\*?(\w+)\s*\([^)]*\)\s*$",
                    line,
                )
                if m:
                    func_name = m.group(1)
                    i += 1  # skip to the brace line
                    line = lines[i]

        if func_name:
            # Count lines from opening { to matching }
            start_line = i + 1  # 1-indexed
            depth = 0
            body_lines = 0
            for j in range(i, len(lines)):
                for ch in lines[j]:
                    if ch == "{":
                        depth += 1
                    elif ch == "}":
                        depth -= 1
                if depth == 0:
                    body_lines = j - i + 1
                    i = j + 1
                    break
            else:
                # Unclosed brace — count to EOF
                body_lines = len(lines) - i
                i = len(lines)

            results.append((func_name, start_line, body_lines))
            continue

        i += 1

    return results


def check_limits(root):
    violations = []
    files = find_files(root)

    for filepath in files:
        relpath = os.path.relpath(filepath, root).replace("\\", "/")

        # File LOC check
        with open(filepath, "r", encoding="utf-8", errors="replace") as f:
            line_count = sum(1 for _ in f)

        if line_count > FILE_LOC_LIMIT:
            violations.append(
                f"FILE {relpath}: {line_count} lines (limit {FILE_LOC_LIMIT})"
            )

        # Function LOC check
        funcs = count_function_lines(filepath)
        for func_name, start_line, body_lines in funcs:
            if body_lines > FUNC_LOC_LIMIT:
                violations.append(
                    f"FUNC {relpath}:{start_line} {func_name}(): "
                    f"{body_lines} lines (limit {FUNC_LOC_LIMIT})"
                )

    return violations


def main():
    parser = argparse.ArgumentParser(description="Check LOC limits")
    parser.add_argument("--root", default=os.path.dirname(os.path.dirname(__file__)))
    args = parser.parse_args()

    root = os.path.abspath(args.root)
    violations = check_limits(root)

    if violations:
        print(f"LOC LIMIT VIOLATIONS ({len(violations)}):\n")
        for v in violations:
            print(f"  {v}")
        print(f"\nLimits: {FILE_LOC_LIMIT} lines/file, {FUNC_LOC_LIMIT} lines/function")
        sys.exit(1)
    else:
        print(f"LOC limits OK: all files <= {FILE_LOC_LIMIT}, "
              f"all functions <= {FUNC_LOC_LIMIT}")
        sys.exit(0)


if __name__ == "__main__":
    main()
