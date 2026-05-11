"""
gen_filters.py - Generate .vcxproj.filters from folder structure.

Usage:
    python gen_filters.py <path/to/project.vcxproj>

Re-run this whenever files are added/removed from a .vcxproj to keep
the Solution Explorer tree in sync with the folder layout on disk.

Filter paths mirror the relative directory of each file.
Files sitting at the project root get no filter (they appear at the
top of Solution Explorer, which is the VS default for unfiltered files).

UUIDs are derived deterministically from the filter path so that
re-running the script produces a stable output (no spurious diffs).
"""

import sys
import uuid
import xml.etree.ElementTree as ET
from pathlib import PurePosixPath, PureWindowsPath
from collections import OrderedDict

NS = "http://schemas.microsoft.com/developer/msbuild/2003"
NS_UUID_SEED = uuid.UUID("6ba7b810-9dad-11d1-80b4-00c04fd430c8")  # dns namespace


def filter_uuid(filter_path: str) -> str:
    """Deterministic UUID based on filter path (case-insensitive)."""
    return "{" + str(uuid.uuid5(NS_UUID_SEED, filter_path.lower())) + "}"


def windows_dir(file_include: str) -> str | None:
    """Return the Windows-style parent directory of a file path, or None if root."""
    # Normalise slashes then take parent
    p = PureWindowsPath(file_include)
    parent = p.parent
    if str(parent) in (".", ""):
        return None
    return str(parent)


def collect_files(vcxproj_path: str):
    """Parse vcxproj and return (tag, include_path) pairs for source files."""
    tree = ET.parse(vcxproj_path)
    root = tree.getroot()

    source_tags = {"ClCompile", "ClInclude", "None", "Text", "Natvis"}
    files = []

    for item_group in root.findall(f"{{{NS}}}ItemGroup"):
        for child in item_group:
            tag = child.tag.replace(f"{{{NS}}}", "")
            if tag in source_tags:
                inc = child.get("Include", "")
                if inc:
                    files.append((tag, inc))

    return files


def build_filter_tree(files):
    """
    Return an ordered dict of filter_path -> uuid for all needed filters,
    plus a list of (tag, include_path, filter_path_or_None) for every file.
    """
    filters: OrderedDict[str, str] = OrderedDict()
    annotated = []

    for tag, inc in files:
        d = windows_dir(inc)
        if d:
            # Ensure every ancestor exists too
            parts = PureWindowsPath(d).parts
            for i in range(1, len(parts) + 1):
                ancestor = str(PureWindowsPath(*parts[:i]))
                if ancestor not in filters:
                    filters[ancestor] = filter_uuid(ancestor)
        annotated.append((tag, inc, d))

    return filters, annotated


def generate_filters_xml(filters: dict, annotated: list) -> str:
    lines = ['<?xml version="1.0" encoding="utf-8"?>',
             '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">']

    # --- Filter definitions ---
    if filters:
        lines.append("  <ItemGroup>")
        for fpath, fid in filters.items():
            lines.append(f'    <Filter Include="{fpath}">')
            lines.append(f"      <UniqueIdentifier>{fid}</UniqueIdentifier>")
            lines.append("    </Filter>")
        lines.append("  </ItemGroup>")

    # --- File entries grouped by tag ---
    tag_order = ["ClCompile", "ClInclude", "None", "Text", "Natvis"]
    by_tag: dict[str, list] = {t: [] for t in tag_order}

    for tag, inc, fpath in annotated:
        if tag not in by_tag:
            by_tag[tag] = []
        by_tag[tag].append((inc, fpath))

    for tag in tag_order:
        entries = by_tag.get(tag, [])
        if not entries:
            continue
        lines.append("  <ItemGroup>")
        for inc, fpath in entries:
            if fpath:
                lines.append(f'    <{tag} Include="{inc}">')
                lines.append(f"      <Filter>{fpath}</Filter>")
                lines.append(f"    </{tag}>")
            else:
                lines.append(f'    <{tag} Include="{inc}" />')
        lines.append("  </ItemGroup>")

    lines.append("</Project>")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 2:
        print("Usage: python gen_filters.py <project.vcxproj>")
        sys.exit(1)

    vcxproj = sys.argv[1]
    filters_path = vcxproj + ".filters"

    print(f"Reading:  {vcxproj}")
    files = collect_files(vcxproj)
    print(f"  Found {len(files)} file entries")

    filters, annotated = build_filter_tree(files)
    print(f"  Generated {len(filters)} filters")

    xml_out = generate_filters_xml(filters, annotated)

    with open(filters_path, "w", encoding="utf-8") as f:
        f.write(xml_out)

    print(f"Written:  {filters_path}")


if __name__ == "__main__":
    main()
