from asset_types import AssetType, get_asset_type, get_asset_group, group_files

def test_texture_types():
    """Textures have .tis, .dds, .hdr, .png, .jpeg extensions"""
    assert get_asset_type("texture.tis") == AssetType.TEXTURE
    assert get_asset_type("texture.dds") == AssetType.TEXTURE
    assert get_asset_type("texture.png") == AssetType.TEXTURE
    assert get_asset_type("texture.jpeg") == AssetType.TEXTURE
    assert get_asset_type("texture.hdr") == AssetType.TEXTURE

def test_model_types():
    """Models have .mis, .cmdl, .glb extensions"""
    assert get_asset_type("model.mis") == AssetType.MODEL
    assert get_asset_type("model.cmdl") == AssetType.MODEL
    assert get_asset_type("model.glb") == AssetType.MODEL

def test_map_types():
    """Maps have .tmap extension"""
    assert get_asset_type("map.tmap") == AssetType.MAP

def test_material_types():
    """Materials have .mm, .mi, .glsl extensions"""
    assert get_asset_type("material.mm") == AssetType.MATERIAL
    assert get_asset_type("material.mi") == AssetType.MATERIAL
    assert get_asset_type("material.glsl") == AssetType.MATERIAL

def test_unknown_type():
    """Unknown extensions return None"""
    assert get_asset_type("file.txt") is None

def test_get_asset_group():
    """Get base name without extension for all related files"""
    assert get_asset_group("rock_texture.tis") == "rock_texture"
    assert get_asset_group("rock_texture.png") == "rock_texture"
    assert get_asset_group("rock_texture.dds") == "rock_texture"

def test_group_files_by_asset():
    """Group files by their asset base name and type"""
    files = [
        "rock_texture.tis",
        "rock_texture.png",
        "rock_texture.dds",
        "sword_model.mis",
        "sword_model.glb",
        "sword_model.cmdl",
        "other.txt"
    ]

    groups = group_files(files)

    assert "rock_texture" in groups
    assert groups["rock_texture"] == {
        "asset": "rock_texture",
        "type": AssetType.TEXTURE,
        "files": ["rock_texture.tis", "rock_texture.png", "rock_texture.dds"]
    }

    assert "sword_model" in groups
    assert groups["sword_model"]["type"] == AssetType.MODEL

    # Unknown files filtered out
    assert "other.txt" not in groups

def test_group_files_empty():
    """Empty file list returns empty groups"""
    assert group_files([]) == {}

def test_group_files_with_invalid_input():
    """group_files handles invalid inputs gracefully"""
    # None should raise TypeError with clear message
    try:
        group_files(None)
        assert False, "Expected TypeError for None input"
    except TypeError as e:
        assert "cannot be None" in str(e)

    # Non-string elements should raise TypeError
    try:
        group_files([123, "texture.tis"])
        assert False, "Expected TypeError for non-string element"
    except TypeError as e:
        assert "must contain strings" in str(e)
