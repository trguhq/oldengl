#!/usr/bin/env python3
"""Check every OpenGL 1.1 declaration and defined library symbol.

The checked-in list ends at the OpenGL 1.1 boundary; newer Mesa exports do
not count towards this check. Run: python3 tests/check_api.py lib/libGL.a
"""
import argparse
import pathlib
import re
import subprocess
import sys


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=pathlib.Path)
    parser.add_argument("--header", type=pathlib.Path,
                        default=root / "include/GL/gl.h")
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    names = [line for line in (root / "tests/gl11-symbols.txt").read_text().splitlines()
             if line and not line.startswith("#")]
    if len(names) != 336 or len(set(names)) != 336:
        sys.exit("The pinned OpenGL 1.1 list must contain 336 distinct names")
    declarations = set(re.findall(r"GLAPIENTRY\s+(gl\w+)\s*\(",
                                   args.header.read_text()))
    result = subprocess.run([args.nm, "-g", str(args.library)],
                            check=True, text=True, capture_output=True)
    # GNU/BSD/POSIX nm output: only externally defined functions count.
    exports = set()
    for line in result.stdout.splitlines():
        match = re.search(r"\b[TW]\s+_?(gl\w+)\s*$", line)
        if match:
            exports.add(match.group(1))
    missing_headers = sorted(set(names) - declarations)
    missing_exports = sorted(set(names) - exports)
    if missing_headers or missing_exports:
        if missing_headers:
            print("Missing declarations:", ", ".join(missing_headers), file=sys.stderr)
        if missing_exports:
            print("Missing definitions:", ", ".join(missing_exports), file=sys.stderr)
        return 1
    print("PASS: all 336 OpenGL 1.1 entry points are declared and defined")
    return 0


if __name__ == "__main__":
    sys.exit(main())
