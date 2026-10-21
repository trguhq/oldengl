#!/usr/bin/env python3
"""Check the pinned Mesa GLU 1.3 C API against its header and library."""
import argparse
import pathlib
import re
import subprocess
import sys


def main():
    root = pathlib.Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library", type=pathlib.Path)
    parser.add_argument("--nm", default="nm")
    args = parser.parse_args()
    names = [line for line in (root / "tests/glu13-symbols.txt").read_text().splitlines()
             if line and not line.startswith("#")]
    if len(names) != 59 or len(set(names)) != 59:
        sys.exit("The pinned Mesa GLU 1.3 list must contain 59 distinct names")
    declarations = set(re.findall(r"GLAPIENTRY\s+(glu\w+)\s*\(",
                                   (root / "include/GL/glu.h").read_text()))
    result = subprocess.run([args.nm, "-g", str(args.library)],
                            check=True, text=True, capture_output=True)
    exports = set()
    for line in result.stdout.splitlines():
        match = re.search(r"\b[TW]\s+_?(glu\w+)\s*$", line)
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
    print("PASS: all 59 Mesa GLU 1.3 entry points are declared and defined")
    return 0


if __name__ == "__main__":
    sys.exit(main())
