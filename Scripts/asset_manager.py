from pathlib import Path
from typing import Optional, List, Dict
import shutil
import subprocess
from asset_types import get_asset_type, get_asset_group, group_files, AssetType

class AssetManager:
    """Manages asset-aware file operations within an asset root directory"""

    def __init__(self, asset_root: Path):
        """Initialize with asset root directory (typically Data/)"""
        self.asset_root = Path(asset_root).resolve()
        self.current_dir = self.asset_root

    def pwd(self) -> Path:
        """Print working directory"""
        return self.current_dir

    def cd(self, path: str) -> None:
        """Change directory (relative or absolute)"""
        if Path(path).is_absolute():
            target = Path(path).resolve()
        else:
            target = (self.current_dir / path).resolve()

        # Ensure target is within asset root
        try:
            target.relative_to(self.asset_root)
        except ValueError:
            raise ValueError(f"Cannot cd outside asset root: {target}")

        if not target.exists():
            raise FileNotFoundError(f"Directory does not exist: {target}")
        if not target.is_dir():
            raise NotADirectoryError(f"Not a directory: {target}")

        self.current_dir = target

    def ls(self) -> List[Dict]:
        """List assets in current directory, grouped by asset"""
        if not self.current_dir.exists():
            return []

        files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
        grouped = group_files(files)

        # Return as sorted list
        return [grouped[key] for key in sorted(grouped.keys())]

    def format_ls(self) -> str:
        """Format ls output in 2-column layout with colors for folders and assets"""
        cwd = self.pwd()

        # Collect directories and assets
        items = []

        try:
            # Get all items in current directory
            for item in sorted(cwd.iterdir()):
                if item.is_dir():
                    items.append(("dir", item.name, None))
        except (OSError, PermissionError):
            pass

        # Get assets from ls()
        assets = self.ls()
        for asset in assets:
            type_str = asset["type"].value.upper()
            items.append(("asset", asset["asset"], type_str))

        if not items:
            return ""

        # Format as 2 columns with colors
        # ANSI colors
        BLUE = "\033[34m"      # Directories
        GREEN = "\033[32m"     # Assets
        RESET = "\033[0m"

        lines = []
        # Calculate column width based on longest name
        max_name_len = max(len(item[1]) for item in items) + 2

        for item_type, name, type_str in items:
            if item_type == "dir":
                # Directory: blue with [DIR] label
                colored_name = f"{BLUE}{name:<{max_name_len}}{RESET}"
                lines.append(f"{colored_name} {BLUE}[DIR]{RESET}")
            else:
                # Asset: green with type
                colored_name = f"{GREEN}{name:<{max_name_len}}{RESET}"
                lines.append(f"{colored_name} {type_str}")

        return "\n".join(lines)

    def cp(self, src: str, dst: str) -> None:
        """
        Copy a source file. For assets, copies only the source file (e.g., .png not .dds)
        """
        src_path = self.current_dir / src

        if not src_path.exists():
            raise FileNotFoundError(f"File not found: {src}")

        # Determine what to copy based on file type
        src_asset_type = get_asset_type(src)

        if src_asset_type == AssetType.TEXTURE:
            # Copy only source files: .png, .jpeg, .hdr
            if src_path.suffix.lower() not in [".png", ".jpeg", ".hdr"]:
                # If user specified import settings or compiled, copy the source instead
                group = get_asset_group(src)
                for ext in [".png", ".jpeg", ".hdr"]:
                    candidate = src_path.parent / (group + ext)
                    if candidate.exists():
                        src_path = candidate
                        break

        elif src_asset_type == AssetType.MODEL:
            # Copy only .glb source
            if src_path.suffix.lower() != ".glb":
                group = get_asset_group(src)
                glb_file = src_path.parent / (group + ".glb")
                if glb_file.exists():
                    src_path = glb_file

        # For maps and materials, copy as-is

        shutil.copy2(src_path, dst)

    def trash(self, path: str) -> None:
        """
        Trash/delete a file. For assets, deletes the compiled and source versions
        (e.g., .dds and .png for textures, but keeps .tis import settings)
        """
        target = self.current_dir / path

        if not target.exists():
            raise FileNotFoundError(f"File not found: {path}")

        asset_type = get_asset_type(path)
        group = get_asset_group(path)

        to_delete = [target]

        if asset_type == AssetType.TEXTURE:
            # Delete .dds, .hdr, .png, .jpeg but keep .tis
            for ext in [".dds", ".hdr", ".png", ".jpeg"]:
                candidate = target.parent / (group + ext)
                if candidate.exists() and candidate != target:
                    to_delete.append(candidate)

        elif asset_type == AssetType.MODEL:
            # Delete .cmdl and .glb but keep .mis
            for ext in [".cmdl", ".glb"]:
                candidate = target.parent / (group + ext)
                if candidate.exists() and candidate != target:
                    to_delete.append(candidate)

        # For maps and materials, delete only the specified file

        for file_path in to_delete:
            file_path.unlink()

    def cat(self, filename: str) -> str:
        """Read and return file contents"""
        file_path = self.current_dir / filename

        if not file_path.exists():
            raise FileNotFoundError(f"File not found: {filename}")

        return file_path.read_text()

    def find_references(self, filename: str) -> List[str]:
        """
        Find all files in asset root that reference this file using ripgrep.
        Returns list of filenames that contain references.
        Search is scoped to asset_root only.
        """
        if not (self.current_dir / filename).exists():
            raise FileNotFoundError(f"File not found: {filename}")

        # Escape filename for regex, handle backslashes on Windows
        escaped_name = filename.replace("\\", "\\\\")

        try:
            result = subprocess.run(
                ["rg", "--files-with-matches", escaped_name, str(self.asset_root)],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                # Convert absolute paths to relative filenames
                refs = []
                for line in result.stdout.strip().split("\n"):
                    if line:
                        ref_path = Path(line)
                        refs.append(ref_path.name)
                return refs
            else:
                return []

        except FileNotFoundError:
            # ripgrep not installed
            raise RuntimeError("ripgrep (rg) is not installed. Install it to use reference finding.")
        except subprocess.TimeoutExpired:
            raise RuntimeError("Reference search timed out")

    def mv(self, src: str, dst: str) -> None:
        """
        Move file and ALL related asset files, then update all references.
        For example, moving rock.png also moves rock.tis and rock.dds to stone.tis/png/dds.
        Or: mv rock materials (moves rock into materials directory keeping same name)
        Updates all references to moved files throughout the asset root.
        """
        src_path = self.current_dir / src
        dst_path = Path(dst)

        if not src_path.exists():
            raise FileNotFoundError(f"File not found: {src}")

        asset_type = get_asset_type(src)
        src_group = get_asset_group(src)

        # If dst is a directory, move into it with same name; otherwise it's a rename
        dst_dir = self.current_dir / dst if not dst_path.is_absolute() else dst_path
        if dst_dir.is_dir():
            # Moving into a directory - keep same asset name
            dst_group = src_group
            move_to_dir = dst_dir
        else:
            # Renaming the asset
            dst_group = get_asset_group(dst_path.name)
            move_to_dir = dst_path.parent if dst_path.is_absolute() else self.current_dir

        # Get all related files for this asset
        files_to_move = []
        old_to_new = {}  # Track old filename -> new filename for reference updates

        if asset_type == AssetType.TEXTURE:
            # Move all texture-related files: .tis, .png, .jpeg, .hdr, .dds
            for ext in [".tis", ".png", ".jpeg", ".hdr", ".dds"]:
                candidate = src_path.parent / (src_group + ext)
                if candidate.exists():
                    files_to_move.append(candidate)
                    old_filename = candidate.name
                    new_filename = dst_group + ext
                    old_to_new[old_filename] = new_filename

        elif asset_type == AssetType.MODEL:
            # Move all model-related files: .mis, .glb, .cmdl
            for ext in [".mis", ".glb", ".cmdl"]:
                candidate = src_path.parent / (src_group + ext)
                if candidate.exists():
                    files_to_move.append(candidate)
                    old_filename = candidate.name
                    new_filename = dst_group + ext
                    old_to_new[old_filename] = new_filename

        elif asset_type == AssetType.MAP:
            # Move .tmap only
            if src_path.exists():
                files_to_move.append(src_path)
                old_to_new[src_path.name] = Path(dst).name

        elif asset_type == AssetType.MATERIAL:
            # Move all material files: .mm, .mi, .glsl
            for ext in [".mm", ".mi", ".glsl"]:
                candidate = src_path.parent / (src_group + ext)
                if candidate.exists():
                    files_to_move.append(candidate)
                    old_filename = candidate.name
                    new_filename = dst_group + ext
                    old_to_new[old_filename] = new_filename
        else:
            # Unknown type, just move the one file
            files_to_move.append(src_path)
            old_to_new[src_path.name] = Path(dst).name

        # Move all related files
        for file_path in files_to_move:
            ext = file_path.suffix
            new_path = move_to_dir / (dst_group + ext)
            file_path.rename(new_path)

        # Fix references: for each old filename, find references and update them
        for old_name, new_name in old_to_new.items():
            try:
                result = subprocess.run(
                    ["rg", "--files-with-matches", old_name, str(self.asset_root)],
                    capture_output=True,
                    text=True,
                    timeout=30
                )

                if result.returncode == 0:
                    for ref_file in result.stdout.strip().split("\n"):
                        if ref_file:
                            ref_path = Path(ref_file)
                            if ref_path.exists() and ref_path not in files_to_move:
                                try:
                                    content = ref_path.read_text()
                                    updated = content.replace(old_name, new_name)
                                    ref_path.write_text(updated)
                                except (IOError, OSError):
                                    # Skip files that can't be read/written
                                    pass
            except (FileNotFoundError, subprocess.TimeoutExpired):
                # ripgrep not available, skip reference updates for this file
                pass
