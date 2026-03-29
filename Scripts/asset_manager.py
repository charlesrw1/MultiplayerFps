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

    def ls(self, path: str = "") -> List[Dict]:
        """List assets in specified directory (or current if empty), grouped by asset"""
        if path:
            # List a specific directory
            target_dir = self.current_dir / path
            if not target_dir.is_dir():
                raise NotADirectoryError(f"Not a directory: {path}")
        else:
            target_dir = self.current_dir

        if not target_dir.exists():
            return []

        files = [f.name for f in target_dir.iterdir() if f.is_file()]
        grouped = group_files(files)

        # Return as sorted list
        return [grouped[key] for key in sorted(grouped.keys())]

    def format_ls(self, path: str = "") -> str:
        """Format ls output in 2-column layout with colors for folders and assets"""
        if path:
            # List a specific directory
            target_dir = self.current_dir / path
            if not target_dir.is_dir():
                raise NotADirectoryError(f"Not a directory: {path}")
            cwd = target_dir
        else:
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
        assets = self.ls(path)
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
                # For materials, show if it's an instance (.mi) or master (.mm)
                display_type = type_str
                if type_str == "MATERIAL":
                    display_type = self._get_material_type(cwd, name)

                colored_name = f"{GREEN}{name:<{max_name_len}}{RESET}"
                lines.append(f"{colored_name} {display_type}")

        return "\n".join(lines)

    def _get_material_type(self, directory: Path, material_name: str) -> str:
        """Determine if material is a master (.mm) or instance (.mi)"""
        mi_file = directory / (material_name + ".mi")
        mm_file = directory / (material_name + ".mm")

        if mi_file.exists():
            return "MATERIAL (instance)"
        elif mm_file.exists():
            return "MATERIAL (master)"
        else:
            return "MATERIAL"

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
        Can accept either a filename (my_model.glb) or asset name (my_model).
        Returns list of filenames that contain references.
        Search is scoped to asset_root only.
        """
        # Check if input is a filename or asset name
        file_path = self.current_dir / filename
        files_to_search = []

        if file_path.exists():
            # It's a filename - search for this specific file
            files_to_search = [filename]
        else:
            # Might be an asset name - find all files in this asset group
            asset_type = get_asset_type(filename)
            if asset_type is not None:
                # It's a known asset type - search for all related files
                asset_group = get_asset_group(filename)
                # Get all files in current directory
                all_files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
                groups = group_files(all_files)
                if asset_group in groups:
                    files_to_search = groups[asset_group]["files"]

            if not files_to_search:
                # Try as asset name without extension
                asset_group = filename
                all_files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
                groups = group_files(all_files)
                if asset_group in groups:
                    files_to_search = groups[asset_group]["files"]

            if not files_to_search:
                raise FileNotFoundError(f"File or asset not found: {filename}")

        # Search for references to any of the files
        all_refs = set()
        for search_file in files_to_search:
            # Escape filename for regex, handle backslashes on Windows
            escaped_name = search_file.replace("\\", "\\\\")

            try:
                result = subprocess.run(
                    ["rg", "--files-with-matches", escaped_name, str(self.asset_root)],
                    capture_output=True,
                    text=True,
                    timeout=30
                )

                if result.returncode == 0:
                    # Convert absolute paths to relative filenames
                    for line in result.stdout.strip().split("\n"):
                        if line:
                            ref_path = Path(line)
                            all_refs.add(ref_path.name)

            except FileNotFoundError:
                # ripgrep not installed
                raise RuntimeError("ripgrep (rg) is not installed. Install it to use reference finding.")
            except subprocess.TimeoutExpired:
                raise RuntimeError("Reference search timed out")

        return sorted(list(all_refs))

    def mkdir(self, path: str) -> None:
        """Create a directory within asset root"""
        target = self.current_dir / path

        # Ensure target is within asset root
        try:
            target.resolve().relative_to(self.asset_root)
        except ValueError:
            raise ValueError(f"Cannot create directory outside asset root: {target}")

        if target.exists():
            raise FileExistsError(f"Directory already exists: {path}")

        target.mkdir(parents=True, exist_ok=False)

    def mv(self, src: str, dst: str) -> None:
        """
        Move file and ALL related asset files, then update all references.
        Can accept filename (my_model.glb) or asset name (my_model).
        For example: mv rock stone (renames rock.* to stone.*)
        Or: mv rock materials (moves rock into materials directory)
        Updates all references to moved files throughout the asset root.
        """
        src_path = self.current_dir / src
        dst_path = Path(dst)

        # Check if src is a filename or asset name
        if src_path.exists() and src_path.is_file():
            # It's a filename that exists
            asset_type = get_asset_type(src)
            src_group = get_asset_group(src)
        else:
            # Try to find as asset name
            asset_type = get_asset_type(src)
            src_group = get_asset_group(src)

            # If not a known file type, try as asset name
            if asset_type is None:
                src_group = src
                # Find the asset group to determine type
                all_files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
                groups = group_files(all_files)
                if src_group in groups:
                    asset_type = groups[src_group]["type"]
                else:
                    raise FileNotFoundError(f"File or asset not found: {src}")
            else:
                # Known type but file doesn't exist - try to find related files
                all_files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
                groups = group_files(all_files)
                if src_group not in groups:
                    raise FileNotFoundError(f"File or asset not found: {src}")

        # Now we have asset_type and src_group, continue with the move

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
