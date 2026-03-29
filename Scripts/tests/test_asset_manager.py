import pytest
import tempfile
from pathlib import Path
from asset_manager import AssetManager

@pytest.fixture
def temp_asset_dir():
    """Create temporary asset directory"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir).resolve()

def test_pwd(temp_asset_dir):
    """pwd returns current directory within asset root"""
    manager = AssetManager(temp_asset_dir)
    assert manager.pwd() == temp_asset_dir

def test_cd_relative(temp_asset_dir):
    """cd changes to relative directory within asset root"""
    subdir = temp_asset_dir / "textures"
    subdir.mkdir()

    manager = AssetManager(temp_asset_dir)
    manager.cd("textures")
    assert manager.pwd() == subdir

def test_cd_absolute(temp_asset_dir):
    """cd changes to absolute directory within asset root"""
    subdir = temp_asset_dir / "models"
    subdir.mkdir()

    manager = AssetManager(temp_asset_dir)
    manager.cd(str(subdir))
    assert manager.pwd() == subdir

def test_cd_parent(temp_asset_dir):
    """cd .. goes to parent directory"""
    subdir = temp_asset_dir / "textures" / "rocks"
    subdir.mkdir(parents=True)

    manager = AssetManager(temp_asset_dir)
    manager.cd("textures/rocks")
    manager.cd("..")
    assert manager.pwd() == temp_asset_dir / "textures"

def test_cd_invalid():
    """cd to nonexistent directory raises error"""
    with tempfile.TemporaryDirectory() as tmpdir:
        manager = AssetManager(Path(tmpdir))
        with pytest.raises(FileNotFoundError):
            manager.cd("nonexistent")

def test_ls_groups_assets(temp_asset_dir):
    """ls returns grouped assets, one line per asset"""
    # Create test files
    (temp_asset_dir / "rock.tis").touch()
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "rock.dds").touch()
    (temp_asset_dir / "sword.mis").touch()
    (temp_asset_dir / "sword.glb").touch()
    (temp_asset_dir / "other.txt").touch()

    manager = AssetManager(temp_asset_dir)
    result = manager.ls()

    # Each asset appears once with all its files
    assert len(result) == 2  # rock and sword

    rock_entry = [e for e in result if e["asset"] == "rock"][0]
    assert rock_entry["type"].value == "texture"
    assert set(rock_entry["files"]) == {"rock.tis", "rock.png", "rock.dds"}

    sword_entry = [e for e in result if e["asset"] == "sword"][0]
    assert sword_entry["type"].value == "model"

    # other.txt not in results
    assert not any(e["asset"] == "other" for e in result)

def test_ls_empty(temp_asset_dir):
    """ls returns empty list if no assets"""
    manager = AssetManager(temp_asset_dir)
    assert manager.ls() == []

def test_ls_format_string(temp_asset_dir):
    """format_ls returns human-readable output"""
    (temp_asset_dir / "rock.tis").touch()
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "rock.dds").touch()

    manager = AssetManager(temp_asset_dir)
    output = manager.format_ls()

    # Output should contain asset name and file list
    assert "rock" in output
