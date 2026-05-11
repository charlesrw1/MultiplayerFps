#type:ignore
import subprocess
from pathlib import Path
from asset_manager import AssetManager
from asset_types import group_files, get_asset_type

from prompt_toolkit import PromptSession
from prompt_toolkit.completion import Completer, Completion
from prompt_toolkit.history import FileHistory
from prompt_toolkit.formatted_text import FormattedText

# comment 

class AssetCompleter(Completer):
    """Tab completion for asset CLI commands"""

    COMPLETION_THRESHOLD = 100

    def __init__(self, cli):
        self.cli = cli

    def _count_items_in_dir(self, directory):
        """Count files and directories in a directory without loading all of them"""
        try:
            count = 0
            for _ in directory.iterdir():
                count += 1
                if count > self.COMPLETION_THRESHOLD:
                    return count
            return count
        except (OSError, PermissionError):
            return 0

    def get_completions(self, document, complete_event):
        """Generate completions for the current document"""
        text = document.text_before_cursor
        parts = text.split(None, 1)

        if not parts:
            # Complete command names from empty line
            for cmd in sorted(self.cli.commands.keys()):
                yield Completion(cmd)
            return

        command = parts[0]
        rest = parts[1] if len(parts) > 1 else ""

        # If completing the command itself (no space after command yet)
        if len(parts) == 1 and not text.endswith(" "):
            for cmd in sorted(self.cli.commands.keys()):
                if cmd.startswith(command):
                    yield Completion(cmd, start_position=-len(command))
            return

        # Complete arguments to a command
        if command not in self.cli.commands:
            return

        # Determine target directory for file count check
        cwd = self.cli.manager.pwd()
        target_dir = cwd
        if "/" in rest:
            dir_path = rest.rsplit("/", 1)[0]
            target_dir = cwd / dir_path

        # Skip completions if directory has too many items (slow)
        if self._count_items_in_dir(target_dir) > self.COMPLETION_THRESHOLD:
            return

        # Determine completion rules based on command
        include_files = True
        include_dirs = True

        if command in ["cd", "mkdir"]:
            include_files = False
            include_dirs = True
        elif command == "find":
            include_files = True
            include_dirs = False

        # Get completions - this returns full paths
        completions = self.cli._get_completions_for(rest, include_files, include_dirs)

        # Yield completions, replacing the entire rest text
        for comp in completions:
            if not rest or comp.startswith(rest):
                yield Completion(comp, start_position=-len(rest))


class AssetCLI:
    """Asset-aware file management REPL"""

    def __init__(self, asset_root=None):
        if asset_root:
            self.manager = AssetManager(Path(asset_root))
        else:
            # Default to Data/ subdirectory
            default_root = Path.cwd() / "Data"
            if not default_root.exists():
                default_root = Path.cwd()
            self.manager = AssetManager(default_root)

        # Undo support - holds the last operation's UndoRecord
        self._undo_record = None

        # Command dispatch map
        self.commands = {
            "pwd": self.do_pwd,
            "cd": self.do_cd,
            "ls": self.do_ls,
            "mkdir": self.do_mkdir,
            "cat": self.do_cat,
            "cp": self.do_cp,
            "mv": self.do_mv,
            "trash": self.do_trash,
            "find": self.do_find,
            "references": self.do_references,
            "undo": self.do_undo,
            "help": self.do_help,
            "exit": self.do_exit,
            "quit": self.do_quit,
        }

    def _get_prompt(self):
        """Get the prompt text with color formatting (for prompt_toolkit)"""
        cwd = self.manager.pwd()
        root = self.manager.asset_root
        try:
            rel = cwd.relative_to(root)
            if rel == Path("."):
                path_str = f"{root.name}"
            else:
                path_str = f"{root.name}/{rel}"
        except ValueError:
            path_str = f"{cwd.name}"

        return FormattedText([("ansicyan", f"{path_str}>"), ("", " ")])

    def _get_prompt_text(self):
        """Get the prompt text as plain string (for fallback input)"""
        cwd = self.manager.pwd()
        root = self.manager.asset_root
        try:
            rel = cwd.relative_to(root)
            if rel == Path("."):
                path_str = f"{root.name}"
            else:
                path_str = f"{root.name}/{rel}"
        except ValueError:
            path_str = f"{cwd.name}"

        return f"{path_str}> "

    def run(self):
        """Start the REPL"""
        print("Asset File Manager - asset-aware file operations")
        print("Type 'help' for commands")

        # Try to use prompt_toolkit; fall back to simple input if console unavailable
        try:
            from prompt_toolkit.output.win32 import NoConsoleScreenBufferError
            history_file = Path.home() / ".asset_cli_history"
            session = PromptSession(
                history=FileHistory(str(history_file)),
                completer=AssetCompleter(self),
            )
            use_prompt_toolkit = True
        except (ImportError, NoConsoleScreenBufferError):
            use_prompt_toolkit = False

        while True:
            try:
                # Get input from either prompt_toolkit or built-in input()
                if use_prompt_toolkit:
                    try:
                        line = session.prompt(self._get_prompt())
                    except NoConsoleScreenBufferError:
                        use_prompt_toolkit = False
                        line = input(self._get_prompt_text())
                else:
                    line = input(self._get_prompt_text())

                line = line.strip()

                if not line:
                    continue

                # Handle shell commands
                if line.startswith("!"):
                    self.do_shell(line[1:])
                    continue

                # Parse command and arguments
                parts = line.split(None, 1)
                command = parts[0]
                arg = parts[1] if len(parts) > 1 else ""

                # Dispatch command
                if command in self.commands:
                    result = self.commands[command](arg)
                    if result is True:  # Exit sentinel
                        break
                else:
                    print(f"Unknown command: {command}")

            except (KeyboardInterrupt, EOFError):
                break

    def do_pwd(self, arg):
        """Print working directory"""
        print(self.manager.pwd())

    def do_cd(self, arg):
        """Change directory: cd <path>"""
        if not arg:
            print("Usage: cd <path>")
            return

        try:
            self.manager.cd(arg)
        except (FileNotFoundError, NotADirectoryError, ValueError) as e:
            print(f"Error: {e}")

    def do_mkdir(self, arg):
        """Create a directory: mkdir <path>"""
        if not arg:
            print("Usage: mkdir <path>")
            return

        try:
            undo_record = self.manager.mkdir(arg)
            self._undo_record = undo_record
            print(f"Created directory: {arg}")
        except (FileExistsError, ValueError) as e:
            print(f"Error: {e}")

    def do_ls(self, arg):
        """List assets in current directory or specified path: ls [path]"""
        try:
            output = self.manager.format_ls(arg)
            if output:
                print(output)
            else:
                print("No assets or directories found")
        except NotADirectoryError as e:
            print(f"Error: {e}")

    def do_cat(self, arg):
        """Show file contents: cat <filename>"""
        if not arg:
            print("Usage: cat <filename>")
            return

        try:
            print(self.manager.cat(arg))
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_cp(self, arg):
        """Copy file: cp <src> <dst>"""
        parts = arg.split()
        if len(parts) != 2:
            print("Usage: cp <src> <dst>")
            return

        src, dst = parts
        try:
            undo_record = self.manager.cp(src, dst)
            self._undo_record = undo_record
            print(f"Copied {src} to {dst}")
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_mv(self, arg):
        """Move file and all related files: mv <src> <dst>
        Supports wildcards: mv pattern* directory/
        """
        parts = arg.split()
        if len(parts) != 2:
            print("Usage: mv <src> <dst>")
            return

        src, dst = parts
        try:
            # Check if src contains wildcards
            if "*" in src or "?" in src:
                # Wildcard move: find all matching assets and move each
                matches = self.manager.find_assets(src)

                if not matches:
                    print(f"No assets matching: {src}")
                    return

                # Collect results from all moves
                all_updated_refs = []
                all_file_moves = []
                all_reference_edits = []

                for asset_info in matches:
                    asset_path = asset_info.get("path", asset_info["asset"])

                    try:
                        updated_refs, undo_record = self.manager.mv(asset_path, dst)
                        all_updated_refs.extend(updated_refs)
                        all_file_moves.extend(undo_record.file_moves)
                        all_reference_edits.extend(undo_record.reference_edits)
                    except (FileNotFoundError, RuntimeError) as e:
                        print(f"Error moving {asset_path}: {e}")

                # Create composite undo record
                from asset_manager import UndoRecord
                composite_record = UndoRecord(
                    operation="mv",
                    file_moves=all_file_moves,
                    reference_edits=all_reference_edits
                )
                self._undo_record = composite_record

                print(f"Moved {len(matches)} asset(s) matching '{src}' to {dst}")
                if all_updated_refs:
                    print(f"Updated references in {len(set(all_updated_refs))} file(s)")
            else:
                # Single file move
                updated_refs, undo_record = self.manager.mv(src, dst)
                self._undo_record = undo_record
                print(f"Moved {src} and related files to {dst}")

                if updated_refs:
                    print(f"Updated references in {len(updated_refs)} file(s):")
                    for ref_file in updated_refs:
                        print(f"  {ref_file}")
                else:
                    print("No references found to update")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_trash(self, arg):
        """Delete file and compiled versions: trash <filename>"""
        if not arg:
            print("Usage: trash <filename>")
            return

        try:
            undo_record = self.manager.trash(arg)
            self._undo_record = undo_record
            print(f"Deleted {arg}")
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_references(self, arg):
        """Find files that reference this asset: references <filename>"""
        if not arg:
            print("Usage: references <filename>")
            return

        try:
            refs = self.manager.find_references(arg)
            if refs:
                print("Referenced by:")
                for ref in sorted(set(refs)):
                    print(f"  {ref}")
            else:
                print("No references found")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_undo(self, arg):
        """Undo the last modifying operation (mv, cp, mkdir, or trash)"""
        if self._undo_record is None:
            print("Nothing to undo")
            return

        try:
            self.manager.undo(self._undo_record)
            print(f"Undid {self._undo_record.operation}")
            self._undo_record = None
        except (FileNotFoundError, OSError, ValueError) as e:
            print(f"Error: {e}")

    def do_shell(self, arg):
        """Execute PowerShell command: !<command>"""
        try:
            result = subprocess.run(
                ["powershell", "-Command", arg],
                capture_output=True,
                text=True,
                cwd=self.manager.pwd()
            )
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
        except Exception as e:
            print(f"Error: {e}")

    def do_find(self, arg):
        """Find assets matching a pattern: find <pattern>
        Supports wildcards: * (any chars), ? (single char)
        Shows results in 2-column format like ls (asset name and type)
        Examples: find "*.png", find "sword*", find "models/*"
        """
        if not arg:
            print("Usage: find <pattern>")
            print("Examples:")
            print("  find *.png       - find all PNG files")
            print("  find sword*      - find files starting with 'sword'")
            print("  find models/*    - find files in models directory")
            return

        try:
            results = self.manager.find_assets(arg)
            if results:
                GREEN = "\033[32m"
                RESET = "\033[0m"

                lines = []
                max_name_len = max(len(asset.get("path", asset["asset"])) for asset in results) + 2

                for asset in results:
                    asset_path = asset.get("path", asset["asset"])
                    type_str = asset["type"].value.upper()

                    if type_str == "MATERIAL":
                        asset_group = asset["asset"]
                        target_dir = self.manager.pwd()
                        if "/" in asset_path:
                            dir_part = asset_path.rsplit("/", 1)[0]
                            target_dir = self.manager.asset_root / dir_part

                        mi_file = target_dir / (asset_group + ".mi")
                        mm_file = target_dir / (asset_group + ".mm")

                        if mi_file.exists():
                            type_str = "MATERIAL (instance)"
                        elif mm_file.exists():
                            type_str = "MATERIAL (master)"

                    colored_name = f"{GREEN}{asset_path:<{max_name_len}}{RESET}"
                    lines.append(f"{colored_name} {type_str}")

                print(f"Found {len(results)} asset(s):")
                print("\n".join(lines))
            else:
                print(f"No assets matching: {arg}")
        except Exception as e:
            print(f"Error: {e}")

    def do_help(self, arg):
        """Show help for all commands or a specific command"""
        if arg:
            if arg in self.commands:
                cmd_func = self.commands[arg]
                doc = cmd_func.__doc__
                print(f"{arg}: {doc if doc else 'No help available'}")
            else:
                print(f"Unknown command: {arg}")
        else:
            print("Available commands:")
            for cmd in sorted(self.commands.keys()):
                if cmd not in ["exit", "quit"]:  # Don't show internal commands in list
                    cmd_func = self.commands[cmd]
                    doc = cmd_func.__doc__
                    print(f"  {cmd:<15} {doc if doc else ''}")
            print("\nType 'help <command>' for more info")
            print("Use '!<command>' to run PowerShell commands")

    def do_exit(self, arg):
        """Exit the CLI"""
        return True

    def do_quit(self, arg):
        """Exit the CLI"""
        return True

    def _get_completions_for(self, partial_arg, include_files=True, include_dirs=True):
        """Get completion matches for paths, supporting subdirectories.
        For files, shows asset groups (e.g., 'rock' not 'rock.tis', 'rock.png', 'rock.dds')
        Supports paths with directories: 'models/my<TAB>' suggests files in models/

        Args:
            partial_arg: the partial text being completed
            include_files: whether to include files/assets
            include_dirs: whether to include directories

        Returns:
            List of completion strings (full paths)
        """
        cwd = self.manager.pwd()
        matches = set()

        try:
            # Handle paths with directories (e.g., "models/my_model")
            if "/" in partial_arg:
                # Split into directory and search text
                path_parts = partial_arg.rsplit("/", 1)
                dir_path = path_parts[0]
                search_text = path_parts[1] if len(path_parts) > 1 else ""

                # Navigate to the subdirectory
                target_dir = cwd / dir_path
                if not target_dir.is_dir():
                    return []

                prefix = dir_path + "/"
            else:
                # Current directory
                target_dir = cwd
                search_text = partial_arg
                prefix = ""

            # Separate directories and files in target directory
            dirs = []
            files = []

            for item in sorted(target_dir.iterdir()):
                if item.is_dir():
                    dirs.append(item.name)
                elif item.is_file():
                    files.append(item.name)

            # Add directories
            if include_dirs:
                for dirname in dirs:
                    if dirname.startswith(search_text):
                        matches.add(prefix + dirname + "/")

            # For files, show asset groups instead of individual files
            if include_files and files:
                groups = group_files(files)
                for asset_name in groups.keys():
                    if asset_name.startswith(search_text):
                        matches.add(prefix + asset_name)

                # If no asset groups match, fall back to non-asset files
                if not any(m.startswith(prefix) for m in matches if not m.endswith("/")):
                    for filename in files:
                        if filename.startswith(search_text) and get_asset_type(filename) is None:
                            matches.add(prefix + filename)

        except (OSError, PermissionError):
            pass

        return sorted(list(matches))


def main():
    """Main entry point - starts REPL or accepts asset root argument"""
    import sys

    asset_root = None
    if len(sys.argv) > 1:
        asset_root = Path(sys.argv[1])

    cli = AssetCLI(asset_root)
    cli.run()


if __name__ == "__main__":
    main()
