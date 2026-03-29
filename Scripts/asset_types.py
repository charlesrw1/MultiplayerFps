from enum import Enum
from pathlib import Path
from typing import Optional, Dict

class AssetType(Enum):
    TEXTURE = "texture"
    MODEL = "model"
    MAP = "map"
    MATERIAL = "material"

# File extension to asset type mapping
EXTENSION_MAP: Dict[str, AssetType] = {
    # Textures
    ".tis": AssetType.TEXTURE,
    ".dds": AssetType.TEXTURE,
    ".hdr": AssetType.TEXTURE,
    ".png": AssetType.TEXTURE,
    ".jpeg": AssetType.TEXTURE,
    # Models
    ".mis": AssetType.MODEL,
    ".cmdl": AssetType.MODEL,
    ".glb": AssetType.MODEL,
    # Maps
    ".tmap": AssetType.MAP,
    # Materials
    ".mm": AssetType.MATERIAL,
    ".mi": AssetType.MATERIAL,
    ".glsl": AssetType.MATERIAL,
}

def get_asset_type(filename: str) -> Optional[AssetType]:
    """Get asset type from filename extension"""
    ext = Path(filename).suffix.lower()
    return EXTENSION_MAP.get(ext)

def get_asset_group(filename: str) -> str:
    """Get the base asset name (without extension)"""
    return Path(filename).stem

def group_files(filenames: list) -> Dict[str, dict]:
    """Group files by asset base name, returning only known asset types"""
    groups = {}

    for filename in filenames:
        asset_type = get_asset_type(filename)
        if asset_type is None:
            continue  # Skip unknown file types

        group_name = get_asset_group(filename)

        if group_name not in groups:
            groups[group_name] = {
                "asset": group_name,
                "type": asset_type,
                "files": []
            }

        groups[group_name]["files"].append(filename)

    return groups
