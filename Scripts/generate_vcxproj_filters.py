"""
Regenerates a .vcxproj.filters file from the actual folder structure of the
files listed in a .vcxproj, so Solution Explorer filters always match disk layout.

Usage:
    py Scripts/generate_vcxproj_filters.py [path/to/Project.vcxproj]

Defaults to Source/CsRemake.vcxproj (ProjectName "Core") if no path is given.
"""
import re
import sys
import uuid
from pathlib import Path

FILTER_NAMESPACE = uuid.UUID("a5c8f1b0-6d3e-4a2b-9c7d-1e2f3a4b5c6d")

ITEM_TYPES = ["ClCompile", "ClInclude", "None"]


def parse_includes(vcxproj_text: str) -> dict[str, list[str]]:
    result: dict[str, list[str]] = {}
    for item_type in ITEM_TYPES:
        result[item_type] = re.findall(
            rf'<{item_type} Include="([^"]+)"', vcxproj_text
        )
    return result


def filter_for_path(include_path: str) -> str | None:
    folder = str(Path(include_path).parent)
    if folder == ".":
        return None
    return folder


def all_ancestor_filters(folder: str) -> list[str]:
    parts = Path(folder).parts
    return [str(Path(*parts[: i + 1])) for i in range(len(parts))]


def filter_guid(name: str) -> str:
    return "{" + str(uuid.uuid5(FILTER_NAMESPACE, name)) + "}"


def build_filters_xml(vcxproj_path: Path) -> str:
    text = vcxproj_path.read_text(encoding="utf-8")
    includes = parse_includes(text)

    all_filters: set[str] = set()
    for item_type in ITEM_TYPES:
        for inc in includes[item_type]:
            folder = filter_for_path(inc)
            if folder:
                all_filters.update(all_ancestor_filters(folder))

    lines = ['<?xml version="1.0" encoding="utf-8"?>']
    lines.append(
        '<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">'
    )
    lines.append("  <ItemGroup>")
    for f in sorted(all_filters):
        lines.append(f'    <Filter Include="{f}">')
        lines.append(f"      <UniqueIdentifier>{filter_guid(f)}</UniqueIdentifier>")
        lines.append("    </Filter>")
    lines.append("  </ItemGroup>")

    for item_type in ITEM_TYPES:
        incs = includes[item_type]
        if not incs:
            continue
        lines.append("  <ItemGroup>")
        for inc in sorted(incs, key=str.lower):
            folder = filter_for_path(inc)
            if folder:
                lines.append(f'    <{item_type} Include="{inc}">')
                lines.append(f"      <Filter>{folder}</Filter>")
                lines.append(f"    </{item_type}>")
            else:
                lines.append(f'    <{item_type} Include="{inc}" />')
        lines.append("  </ItemGroup>")

    lines.append("</Project>")
    lines.append("")
    return "\n".join(lines)


def main() -> None:
    vcxproj_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("Source/CsRemake.vcxproj")
    if not vcxproj_path.exists():
        print(f"error: {vcxproj_path} not found", file=sys.stderr)
        sys.exit(1)

    filters_path = vcxproj_path.with_suffix(vcxproj_path.suffix + ".filters")
    xml = build_filters_xml(vcxproj_path)
    filters_path.write_text(xml, encoding="utf-8", newline="\n")
    print(f"wrote {filters_path}")


if __name__ == "__main__":
    main()
