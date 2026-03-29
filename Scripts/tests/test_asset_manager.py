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

def test_cp_copies_source_only(temp_asset_dir):
    """cp copies only the source file for an asset"""
    # Create texture with source, import settings, compiled
    (temp_asset_dir / "rock.tis").touch()  # import settings
    (temp_asset_dir / "rock.png").touch()  # source
    (temp_asset_dir / "rock.dds").touch()  # compiled

    dest_dir = temp_asset_dir / "backup"
    dest_dir.mkdir()

    manager = AssetManager(temp_asset_dir)
    manager.cp("rock.png", str(dest_dir / "rock.png"))

    # Only the source file should be copied
    assert (dest_dir / "rock.png").exists()
    assert not (dest_dir / "rock.tis").exists()
    assert not (dest_dir / "rock.dds").exists()

def test_cp_model_copies_source(temp_asset_dir):
    """cp for model copies .glb source"""
    (temp_asset_dir / "sword.mis").touch()
    (temp_asset_dir / "sword.glb").touch()
    (temp_asset_dir / "sword.cmdl").touch()

    dest_dir = temp_asset_dir / "backup"
    dest_dir.mkdir()

    manager = AssetManager(temp_asset_dir)
    manager.cp("sword.glb", str(dest_dir / "sword.glb"))

    assert (dest_dir / "sword.glb").exists()
    assert not (dest_dir / "sword.mis").exists()

def test_cp_file_not_found(temp_asset_dir):
    """cp raises error if file doesn't exist"""
    manager = AssetManager(temp_asset_dir)
    with pytest.raises(FileNotFoundError):
        manager.cp("nonexistent.png", "/tmp/dest.png")

def test_trash_removes_compiled_and_source(temp_asset_dir):
    """trash removes compiled and source, keeps import settings"""
    (temp_asset_dir / "rock.tis").touch()
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "rock.dds").touch()

    manager = AssetManager(temp_asset_dir)
    manager.trash("rock.png")

    # Source and compiled deleted
    assert not (temp_asset_dir / "rock.png").exists()
    assert not (temp_asset_dir / "rock.dds").exists()
    # Import settings kept
    assert (temp_asset_dir / "rock.tis").exists()

def test_trash_file_not_found(temp_asset_dir):
    """trash raises error if file doesn't exist"""
    manager = AssetManager(temp_asset_dir)
    with pytest.raises(FileNotFoundError):
        manager.trash("nonexistent.png")

def test_cat_reads_file(temp_asset_dir):
    """cat reads and returns file contents"""
    test_file = temp_asset_dir / "test.txt"
    test_file.write_text("Hello, World!")

    manager = AssetManager(temp_asset_dir)
    content = manager.cat("test.txt")

    assert content == "Hello, World!"

def test_cat_file_not_found(temp_asset_dir):
    """cat raises error if file doesn't exist"""
    manager = AssetManager(temp_asset_dir)
    with pytest.raises(FileNotFoundError):
        manager.cat("nonexistent.txt")

def test_find_references(temp_asset_dir):
    """find_references returns list of files that reference a given file"""
    # Create asset
    (temp_asset_dir / "rock.dds").touch()

    # Create files that reference the asset
    shader = temp_asset_dir / "shader.glsl"
    shader.write_text("texture rock.dds\nother content")

    material = temp_asset_dir / "material.mi"
    material.write_text("baseTexture: rock.dds")

    config = temp_asset_dir / "config.txt"
    config.write_text("something else")

    manager = AssetManager(temp_asset_dir)
    refs = manager.find_references("rock.dds")

    assert "shader.glsl" in refs
    assert "material.mi" in refs
    assert len(refs) == 2

def test_find_references_returns_empty_if_none(temp_asset_dir):
    """find_references returns empty list if no references"""
    (temp_asset_dir / "unused.dds").touch()

    manager = AssetManager(temp_asset_dir)
    refs = manager.find_references("unused.dds")

    assert refs == []
