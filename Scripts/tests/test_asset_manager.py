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
