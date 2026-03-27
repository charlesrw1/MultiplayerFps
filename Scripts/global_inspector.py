#!/usr/bin/env python3
"""
global_inspector.py - C++ global variable access analyzer

Usage:
    python global_inspector.py <src_dir> [--output FILE] [--exclude DIR] [--no-pretty] [--include-headers]

Requirements:
    pip install libclang
    LLVM installed (provides libclang.dll on Windows)
"""

import argparse
import json
import os
import sys
from collections import defaultdict, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Set


# ---------------------------------------------------------------------------
# libclang loading
# ---------------------------------------------------------------------------

def load_libclang():
    """Import clang.cindex, auto-detecting libclang.dll on Windows."""
    try:
        import clang.cindex as cl
    except ImportError:
        print("ERROR: libclang Python bindings not found.")
        print("  Install with: pip install libclang")
        sys.exit(1)

    # Try default (works if libclang is on PATH or bundled with pip package)
    try:
        cl.Index.create()
        return cl
    except Exception:
        pass

    # Windows: search common LLVM install locations
    candidate_paths = [
        r"C:\Program Files\LLVM\bin\libclang.dll",
        r"C:\Program Files (x86)\LLVM\bin\libclang.dll",
    ]
    for path in candidate_paths:
        if os.path.exists(path):
            cl.Config.set_library_file(path)
            try:
                cl.Index.create()
                return cl
            except Exception:
                continue

    print("ERROR: libclang.dll not found.")
    print("  Install LLVM from: https://releases.llvm.org/download.html")
    print("  Then re-run. Or set LIBCLANG_PATH env var to the DLL path.")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class GlobalVar:
    name: str
    qualified_name: str
    type_str: str
    file: str
    line: int
    is_static: bool


@dataclass
class FunctionInfo:
    qualified_name: str
    file: str
    direct_globals: Set[str] = field(default_factory=set)   # qualified_names
    calls: Set[str] = field(default_factory=set)             # qualified_names


# ---------------------------------------------------------------------------
# Main entry point (filled in later tasks)
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Analyze C++ source for global variable access (direct + transitive)."
    )
    parser.add_argument("src_dir", help="Root source directory to analyze")
    parser.add_argument("--output", default="globals_report.json",
                        help="JSON output file (default: globals_report.json)")
    parser.add_argument("--exclude", action="append", default=["External"],
                        metavar="DIR",
                        help="Directory name to exclude (repeatable, default: External)")
    parser.add_argument("--no-pretty", action="store_true",
                        help="Skip console pretty-print, only write JSON")
    parser.add_argument("--include-headers", action="store_true",
                        help="Also analyze .h/.hpp files for function bodies")
    args = parser.parse_args()

    cl = load_libclang()
    print(f"Scanning: {args.src_dir}")
    print(f"Excluding dirs containing: {args.exclude}")


if __name__ == "__main__":
    main()
