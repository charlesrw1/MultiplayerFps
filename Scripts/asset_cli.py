import cmd
from pathlib import Path
from asset_manager import AssetManager

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
        """Update prompt to show current directory"""
        cwd = self.manager.pwd()
        root = self.manager.asset_root
        try:
            rel = cwd.relative_to(root)
            if rel == Path("."):
                self.prompt = f"{root.name}> "
            else:
                self.prompt = f"{rel}> "
        except ValueError:
            self.prompt = f"{cwd.name}> "

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

    def do_ls(self, arg):
        """List assets in current directory"""
        output = self.manager.format_ls()
        if output:
            print(output)
        else:
            print("No assets found")

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

    def _get_path_completions(self, text, include_files=True, include_dirs=True):
        """Get completion matches for paths in current directory"""
        cwd = self.manager.pwd()
        matches = []

        try:
            # List all items in current directory
            for item in sorted(cwd.iterdir()):
                name = item.name
                # Include if matches text prefix
                if name.startswith(text):
                    if item.is_dir() and include_dirs:
                        matches.append(name + "/")
                    elif item.is_file() and include_files:
                        matches.append(name)
                    elif item.is_dir() and not include_files:
                        matches.append(name + "/")
        except (OSError, PermissionError):
            pass

        return matches

    def complete_cd(self, text, line, begidx, endidx):
        """Tab completion for cd command - directories only"""
        return self._get_path_completions(text, include_files=False, include_dirs=True)

    def complete_cat(self, text, line, begidx, endidx):
        """Tab completion for cat command - files only"""
        return self._get_path_completions(text, include_files=True, include_dirs=False)

    def complete_cp(self, text, line, begidx, endidx):
        """Tab completion for cp command - all paths"""
        return self._get_path_completions(text, include_files=True, include_dirs=True)

    def complete_mv(self, text, line, begidx, endidx):
        """Tab completion for mv command - all paths"""
        return self._get_path_completions(text, include_files=True, include_dirs=True)

    def complete_trash(self, text, line, begidx, endidx):
        """Tab completion for trash command - files only"""
        return self._get_path_completions(text, include_files=True, include_dirs=False)

    def complete_references(self, text, line, begidx, endidx):
        """Tab completion for references command - files only"""
        return self._get_path_completions(text, include_files=True, include_dirs=False)

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
