#!/usr/bin/env python3
"""Reject module families whose shared structure layouts differ."""
import argparse
from pathlib import Path
import re
import subprocess
import sys

def layout(module, typename):
    proc = subprocess.run(["pahole", "-F", "dwarf", "-C", typename, str(module)],
                          text=True, capture_output=True)
    if proc.returncode or not proc.stdout.startswith("struct "):
        return None
    # Include field declarations and offset/size comments; omit cacheline notes.
    lines = [re.sub(r"\s+", " ", s.strip()) for s in proc.stdout.splitlines()
             if re.search(r"/\*\s*\d+\s+\d+\s*\*/", s)
             or re.search(r"/\* size: \d+", s)]
    if not lines:
        raise ValueError(f"No DWARF field offsets found in {module}: {typename}")
    return lines

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("provider", type=Path)
    ap.add_argument("consumers", type=Path, nargs="+")
    args = ap.parse_args()
    compared = 0
    for typename in ("oplus_task_struct", "oplus_rq"):
        expected = layout(args.provider, typename)
        if expected is None:
            raise ValueError(f"Provider lacks {typename} DWARF")
        for consumer in args.consumers:
            actual = layout(consumer, typename)
            if actual is None:
                continue
            compared += 1
            if actual != expected:
                print(f"INCOMPATIBLE {consumer.name}: {typename}", file=sys.stderr)
                return 1
    if not compared:
        raise ValueError("No consumer layouts were checked")
    print(f"PASS: {compared} shared layouts agree with {args.provider.name}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
