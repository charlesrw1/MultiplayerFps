import pytest
from asset_types import AssetType, get_asset_type, get_asset_group

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
