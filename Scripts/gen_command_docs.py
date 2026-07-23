"""Generates docs/console_commands.json from `@cmd:`/`@usage:` doc comments.

Scans Source/**/*.cpp for a comment block sitting directly above a command registration call
(Auto_Engine_Cmd, ConsoleCmdGroup::add, AGENT_BRIDGE_COMMAND) and pulls the command's description
and usage out of it:

    // @cmd: gives the local player a weapon by name
    // @usage: give_weapon <weapon_name> [ammo_count]
    static Auto_Engine_Cmd cmd_give_weapon("give_weapon", give_weapon_cmd);

Descriptions live here rather than in the C++ registration API itself, so existing registration
call sites never need to change signature - undocumented commands just get an empty description
until someone adds the comment. Run by Source/Core.vcxproj's GenerateCommandDocs MSBuild target
(Inputs/Outputs, runs before ClCompile whenever a .cpp under Source/ changes); can also be run by
hand: `py Scripts/gen_command_docs.py`.
"""
import glob
import json
import os
import re

COMMENT_RE = re.compile(r'^\s*//\s?(.*?)\s*$')
CMD_RE = re.compile(r'^@cmd:\s*(.*?)\s*$')
USAGE_RE = re.compile(r'^@usage:\s*(.*?)\s*$')

# Order doesn't matter - first pattern that matches a line wins.
REGISTRATION_PATTERNS = [
    re.compile(r'Auto_Engine_Cmd\s+\w+\s*\(\s*"([^"]+)"'),
    re.compile(r'(?:\.|->)add\s*\(\s*"([^"]+)"'),
    re.compile(r'AGENT_BRIDGE_COMMAND\s*\(\s*(\w+)\s*,'),
]

EXCLUDED_PATH_PARTS = (os.sep + "External" + os.sep, os.sep + ".generated" + os.sep)


def extract_registered_name(line):
    for pattern in REGISTRATION_PATTERNS:
        m = pattern.search(line)
        if m:
            return m.group(1)
    return None


def parse_comment_block(comment_lines):
    """comment_lines: raw '//'-stripped text of a contiguous run of comment lines directly above
    a registration call. @cmd: starts the description, @usage: starts the usage string; any
    further plain lines before the next @-tag (or the end of the block) are appended to whichever
    one is currently open, so descriptions/usage can wrap across multiple comment lines."""
    description_parts = []
    usage_parts = []
    active = None
    for text in comment_lines:
        cmd_match = CMD_RE.match(text)
        usage_match = USAGE_RE.match(text)
        if cmd_match:
            active = description_parts
            if cmd_match.group(1):
                active.append(cmd_match.group(1))
        elif usage_match:
            active = usage_parts
            if usage_match.group(1):
                active.append(usage_match.group(1))
        elif active is not None:
            active.append(text)
    return " ".join(description_parts).strip(), " ".join(usage_parts).strip()


def scan_file(path, repo_root, docs):
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        lines = f.readlines()

    comment_block = []
    for i, line in enumerate(lines):
        comment_match = COMMENT_RE.match(line) if line.strip().startswith("//") else None
        if comment_match:
            comment_block.append(comment_match.group(1))
            continue

        name = extract_registered_name(line)
        if name:
            description, usage = parse_comment_block(comment_block)
            if description or usage:
                docs[name] = {
                    "description": description,
                    "usage": usage,
                    "source": os.path.relpath(path, repo_root).replace(os.sep, "/") + f":{i + 1}",
                }

        # Any non-comment line (matched registration or not) ends the contiguous comment block -
        # a blank line or unrelated code breaks the association with whatever comes next.
        comment_block = []


def main():
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    source_dir = os.path.join(repo_root, "Source")

    docs = {}
    for path in glob.glob(os.path.join(source_dir, "**", "*.cpp"), recursive=True):
        if any(part in path for part in EXCLUDED_PATH_PARTS):
            continue
        scan_file(path, repo_root, docs)

    out_path = os.path.join(repo_root, "docs", "console_commands.json")
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(docs, f, indent=2, sort_keys=True)
        f.write("\n")

    print(f"wrote {len(docs)} command doc entries to {out_path}")


if __name__ == "__main__":
    main()
