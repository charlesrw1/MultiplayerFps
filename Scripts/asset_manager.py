from pathlib import Path
from typing import Optional, List, Dict
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
