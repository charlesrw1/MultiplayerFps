import cmd
from pathlib import Path
from asset_manager import AssetManager
from asset_types import group_files, get_asset_type

class AssetCLI(cmd.Cmd):
    """Asset-aware file management REPL"""

    intro = "Asset File Manager - asset-aware file operations\nType 'help' for commands"
    prompt = "assets> "

    def __init__(self, asset_root=None):
        super().__init__()
        if asset_root:
            self.manager = AssetManager(Path(asset_root))
        else:
            # Default to Data/ subdirectory
            default_root = Path.cwd() / "Data"
            if not default_root.exists():
                default_root = Path.cwd()
            self.manager = AssetManager(default_root)
        self.update_prompt()

    def update_prompt(self):
        """Update prompt to show full path relative to asset root with color"""
        cwd = self.manager.pwd()
        root = self.manager.asset_root
        # ANSI color codes
        CYAN = "\033[36m"      # Cyan for path
        RESET = "\033[0m"
        try:
            rel = cwd.relative_to(root)
            if rel == Path("."):
                # At root, show root name only
                path_str = f"{root.name}"
            else:
                # Show root name + relative path with trailing slash
                path_str = f"{root.name}/{rel}"
            self.prompt = f"{CYAN}{path_str}>{RESET} "
        except ValueError:
            self.prompt = f"{CYAN}{cwd.name}>{RESET} "

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
            self.update_prompt()
        except (FileNotFoundError, NotADirectoryError, ValueError) as e:
            print(f"Error: {e}")

    def do_mkdir(self, arg):
        """Create a directory: mkdir <path>"""
        if not arg:
            print("Usage: mkdir <path>")
            return

        try:
            self.manager.mkdir(arg)
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
            self.manager.cp(src, dst)
            print(f"Copied {src} to {dst}")
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_mv(self, arg):
        """Move file and all related files: mv <src> <dst>"""
        parts = arg.split()
        if len(parts) != 2:
            print("Usage: mv <src> <dst>")
            return

        src, dst = parts
        try:
            self.manager.mv(src, dst)
            print(f"Moved {src} and related files to {dst}")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_trash(self, arg):
        """Delete file and compiled versions: trash <filename>"""
        if not arg:
            print("Usage: trash <filename>")
            return

        try:
            self.manager.trash(arg)
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
                for ref in sorted(set(refs)):  # Remove duplicates
                    print(f"  {ref}")
            else:
                print("No references found")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_shell(self, arg):
        """Execute PowerShell command: !<command>"""
        import subprocess
        try:
            result = subprocess.run(
                ["powershell", "-Command", arg],
                capture_output=True,
                text=True
            )
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
        except Exception as e:
            print(f"Error: {e}")

    def default(self, line):
        """Handle ! prefix for shell commands"""
        if line.startswith("!"):
            self.do_shell(line[1:])
        else:
            print(f"Unknown command: {line}")

    def do_exit(self, arg):
        """Exit the CLI"""
        return True

    def do_quit(self, arg):
        """Exit the CLI"""
        return True

    def emptyline(self):
        """Don't repeat last command on empty line"""
        pass

    def _get_path_completions(self, text, line, begidx, endidx, include_files=True, include_dirs=True):
        """Get completion matches for paths, supporting subdirectories.
        For files, shows asset groups (e.g., 'rock' not 'rock.tis', 'rock.png', 'rock.dds')
        Supports paths with directories: 'models/my<TAB>' suggests files in models/

        Args:
            text: the partial text being completed (might be just last word)
            line: the full command line
            begidx: beginning index of text in line
            endidx: ending index of text in line
            include_files: whether to include files/assets
            include_dirs: whether to include directories
        """
        cwd = self.manager.pwd()
        matches = set()

        try:
            # Extract the full argument to the command by parsing the line
            # This handles cases where "/" causes readline to split into separate words
            # line format: "<command> <arg1> [<arg2>]"
            # We need to find which argument position we're completing

            # Split the line to identify the command
            parts = line.split()
            if not parts:
                return []

            cmd = parts[0]  # e.g., "mv", "cp", "cd", etc.

            # Find where the command ends and arguments begin
            cmd_start = line.find(cmd)
            arg_start = cmd_start + len(cmd)
            # Skip whitespace after command
            while arg_start < len(line) and line[arg_start] == ' ':
                arg_start += 1

            # Extract the text from arg_start to the cursor (endidx)
            # This gives us the full path being completed, including any "/"
            full_arg = line[arg_start:endidx]

            # Handle paths with directories (e.g., "models/my_model")
            if "/" in full_arg:
                # Split into directory and search text
                path_parts = full_arg.rsplit("/", 1)
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
                search_text = full_arg
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

    def complete_cd(self, text, line, begidx, endidx):
        """Tab completion for cd command - directories only"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=False, include_dirs=True)

    def complete_cat(self, text, line, begidx, endidx):
        """Tab completion for cat command - files only"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=True, include_dirs=False)

    def complete_cp(self, text, line, begidx, endidx):
        """Tab completion for cp command - all paths"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=True, include_dirs=True)

    def complete_mv(self, text, line, begidx, endidx):
        """Tab completion for mv command - all paths"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=True, include_dirs=True)

    def complete_trash(self, text, line, begidx, endidx):
        """Tab completion for trash command - files only"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=True, include_dirs=False)

    def complete_references(self, text, line, begidx, endidx):
        """Tab completion for references command - files only"""
        return self._get_path_completions(text, line, begidx, endidx, include_files=True, include_dirs=False)

    def complete_ls(self, text, line, begidx, endidx):
        """Tab completion for ls command - shows directories and asset groups, with subdirectory support"""
        cwd = self.manager.pwd()
        matches = set()

        try:
            # Extract the full argument to the ls command
            # line format: "ls <arg>"
            # Split on first whitespace to get command and argument parts
            parts = line.split(None, 1)
            if len(parts) < 2:
                arg = ""
            else:
                # Find where the argument starts in the line
                cmd = parts[0]  # "ls"
                cmd_start = line.find(cmd)
                arg_start = cmd_start + len(cmd)
                # Skip whitespace after command
                while arg_start < len(line) and line[arg_start] == ' ':
                    arg_start += 1
                # Argument is from arg_start to cursor position (endidx)
                arg = line[arg_start:endidx]

            # Handle paths with directories (e.g., "models/my<TAB>")
            if "/" in arg:
                path_parts = arg.rsplit("/", 1)
                dir_path = path_parts[0]
                search_text = path_parts[1] if len(path_parts) > 1 else ""
                target_dir = cwd / dir_path
                prefix = dir_path + "/"
            else:
                target_dir = cwd
                search_text = arg
                prefix = ""

            if not target_dir.is_dir():
                return []

            # List directories
            for item in sorted(target_dir.iterdir()):
                if item.is_dir() and item.name.startswith(search_text):
                    matches.add(prefix + item.name + "/")

            # List asset groups
            files = [f.name for f in target_dir.iterdir() if f.is_file()]
            if files:
                groups = group_files(files)
                for asset_name in groups.keys():
                    if asset_name.startswith(search_text):
                        matches.add(prefix + asset_name)
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
    cli.cmdloop()

if __name__ == "__main__":
    main()
