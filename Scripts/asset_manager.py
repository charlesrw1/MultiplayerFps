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
        """Format ls output for display"""
        assets = self.ls()
        if not assets:
            return ""

        lines = []
        for asset in assets:
            type_str = asset["type"].value.upper()
            files_str = ", ".join(sorted(asset["files"]))
            lines.append(f"{asset['asset']} [{type_str}]\n  {files_str}")

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
