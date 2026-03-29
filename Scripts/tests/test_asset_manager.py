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

def test_mv_moves_file(temp_asset_dir):
    """mv moves file to new location"""
    (temp_asset_dir / "old.txt").write_text("content")
    dest = temp_asset_dir / "new.txt"

    manager = AssetManager(temp_asset_dir)
    manager.mv("old.txt", str(dest))

    assert not (temp_asset_dir / "old.txt").exists()
    assert dest.exists()
    assert dest.read_text() == "content"

def test_mv_texture_moves_all_related_files(temp_asset_dir):
    """mv moves all texture files together: .tis, .png, .dds"""
    # Create all related files for texture asset
    (temp_asset_dir / "rock.tis").write_text("import settings")
    (temp_asset_dir / "rock.png").write_text("source")
    (temp_asset_dir / "rock.dds").write_text("compiled")

    manager = AssetManager(temp_asset_dir)
    # Move just one file; all related files should move
    manager.mv("rock.png", str(temp_asset_dir / "stone.png"))

    # All files moved with new name
    assert not (temp_asset_dir / "rock.tis").exists()
    assert not (temp_asset_dir / "rock.png").exists()
    assert not (temp_asset_dir / "rock.dds").exists()

    assert (temp_asset_dir / "stone.tis").exists()
    assert (temp_asset_dir / "stone.png").exists()
    assert (temp_asset_dir / "stone.dds").exists()

def test_mv_model_moves_all_related_files(temp_asset_dir):
    """mv moves all model files together: .mis, .glb, .cmdl"""
    (temp_asset_dir / "sword.mis").write_text("import")
    (temp_asset_dir / "sword.glb").write_text("source")
    (temp_asset_dir / "sword.cmdl").write_text("compiled")

    manager = AssetManager(temp_asset_dir)
    manager.mv("sword.glb", str(temp_asset_dir / "axe.glb"))

    assert not (temp_asset_dir / "sword.mis").exists()
    assert (temp_asset_dir / "axe.mis").exists()
    assert (temp_asset_dir / "axe.glb").exists()
    assert (temp_asset_dir / "axe.cmdl").exists()

def test_mv_fixes_references(temp_asset_dir):
    """mv updates all references to moved files in asset root"""
    # Create asset
    (temp_asset_dir / "rock.tis").write_text("")
    (temp_asset_dir / "rock.png").write_text("")
    (temp_asset_dir / "rock.dds").write_text("compiled")

    # Create files that reference the asset by various filenames
    shader = temp_asset_dir / "shader.glsl"
    shader.write_text("use rock.dds and rock.png in one file")

    material = temp_asset_dir / "material.mi"
    material.write_text("texture: rock.dds")

    # Move the asset
    manager = AssetManager(temp_asset_dir)
    manager.mv("rock.png", str(temp_asset_dir / "stone.png"))

    # Check that all references were updated
    shader_content = shader.read_text()
    assert "stone.dds" in shader_content
    assert "stone.png" in shader_content
    assert "rock.dds" not in shader_content
    assert "rock.png" not in shader_content

    material_content = material.read_text()
    assert "stone.dds" in material_content
    assert "rock.dds" not in material_content

def test_mv_file_not_found(temp_asset_dir):
    """mv raises error if source doesn't exist"""
    manager = AssetManager(temp_asset_dir)
    with pytest.raises(FileNotFoundError):
        manager.mv("nonexistent.txt", "/tmp/dest.txt")

def test_find_files_by_extension(temp_asset_dir):
    """find_files returns files matching extension pattern"""
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "sword.png").touch()
    (temp_asset_dir / "rock.dds").touch()
    (temp_asset_dir / "config.txt").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("*.png")

    assert len(results) == 2
    assert "rock.png" in results
    assert "sword.png" in results
    assert "rock.dds" not in results
    assert "config.txt" not in results

def test_find_files_by_prefix(temp_asset_dir):
    """find_files returns files matching prefix pattern"""
    (temp_asset_dir / "sword.mis").touch()
    (temp_asset_dir / "sword.glb").touch()
    (temp_asset_dir / "rock.png").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("sword*")

    assert len(results) == 2
    assert "sword.mis" in results
    assert "sword.glb" in results
    assert "rock.png" not in results

def test_find_files_in_subdirectory(temp_asset_dir):
    """find_files supports directory patterns"""
    (temp_asset_dir / "models").mkdir()
    (temp_asset_dir / "models" / "my_model.glb").touch()
    (temp_asset_dir / "models" / "sword.glb").touch()
    (temp_asset_dir / "textures").mkdir()
    (temp_asset_dir / "textures" / "rock.png").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("models/*")

    assert len(results) == 2
    assert "models/my_model.glb" in results
    assert "models/sword.glb" in results
    assert "textures/rock.png" not in results

def test_find_files_nested_path(temp_asset_dir):
    """find_files returns files in nested directories"""
    (temp_asset_dir / "models" / "weapons").mkdir(parents=True)
    (temp_asset_dir / "models" / "weapons" / "sword.glb").touch()
    (temp_asset_dir / "models" / "my_model.glb").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("models/weapons/*")

    assert len(results) == 1
    assert "models/weapons/sword.glb" in results

def test_find_files_case_insensitive(temp_asset_dir):
    """find_files is case-insensitive"""
    (temp_asset_dir / "Rock.PNG").touch()
    (temp_asset_dir / "Sword.GLB").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("*.png")

    assert "Rock.PNG" in results

def test_find_files_empty_result(temp_asset_dir):
    """find_files returns empty list if no matches"""
    (temp_asset_dir / "rock.png").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("*.dds")

    assert results == []

def test_find_files_wildcard_question_mark(temp_asset_dir):
    """find_files supports ? wildcard for single character"""
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "rock.dds").touch()
    (temp_asset_dir / "ro.png").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_files("ro?k.png")

    assert len(results) == 1
    assert "rock.png" in results
    assert "ro.png" not in results

def test_find_assets_groups_files(temp_asset_dir):
    """find_assets returns assets grouped, not individual files"""
    # Create a texture asset with multiple files
    (temp_asset_dir / "rock.tis").touch()
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "rock.dds").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_assets("rock*")

    # Should return only 1 asset, not 3 files
    assert len(results) == 1
    assert results[0]["asset"] == "rock"
    assert results[0]["type"].value == "texture"
    # Should contain all related files
    assert "rock.tis" in results[0]["files"]
    assert "rock.png" in results[0]["files"]
    assert "rock.dds" in results[0]["files"]

def test_find_assets_with_subdirectories(temp_asset_dir):
    """find_assets returns assets in subdirectories with correct paths"""
    models_dir = temp_asset_dir / "models" / "weapons"
    models_dir.mkdir(parents=True)

    (models_dir / "sword.mis").touch()
    (models_dir / "sword.glb").touch()
    (models_dir / "sword.cmdl").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_assets("models/*")

    assert len(results) == 1
    assert results[0]["asset"] == "sword"
    assert results[0]["path"] == "models/weapons/sword"
    assert results[0]["type"].value == "model"

def test_find_assets_multiple_assets(temp_asset_dir):
    """find_assets returns multiple assets without duplication"""
    models_dir = temp_asset_dir / "models"
    models_dir.mkdir()

    (models_dir / "my_model.mis").touch()
    (models_dir / "my_model.glb").touch()
    (models_dir / "sword.mis").touch()
    (models_dir / "sword.glb").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_assets("models/*")

    # Should return 2 assets, not 4 files
    assert len(results) == 2
    asset_names = {r["asset"] for r in results}
    assert asset_names == {"my_model", "sword"}

def test_find_assets_by_extension(temp_asset_dir):
    """find_assets finds assets by file extension pattern"""
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "sword.glb").touch()
    (temp_asset_dir / "config.txt").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_assets("*.png")

    assert len(results) == 1
    assert results[0]["asset"] == "rock"

def test_find_assets_empty_result(temp_asset_dir):
    """find_assets returns empty list if no matches"""
    (temp_asset_dir / "rock.png").touch()

    manager = AssetManager(temp_asset_dir)
    results = manager.find_assets("*.dds")

    assert results == []

def test_find_assets_fuzzy_matching(temp_asset_dir):
    """find_assets uses fuzzy/substring matching when no wildcards"""
    (temp_asset_dir / "my_model.mis").touch()
    (temp_asset_dir / "my_model.glb").touch()
    (temp_asset_dir / "my_model.cmdl").touch()
    (temp_asset_dir / "sword.glb").touch()

    manager = AssetManager(temp_asset_dir)
    # Fuzzy match: "my_model" should find "my_model" even without wildcards
    results = manager.find_assets("my_model")

    assert len(results) == 1
    assert results[0]["asset"] == "my_model"

def test_find_assets_fuzzy_partial_match(temp_asset_dir):
    """find_assets fuzzy matching works with partial names"""
    (temp_asset_dir / "my_model_v1.mis").touch()
    (temp_asset_dir / "my_model_v1.glb").touch()
    (temp_asset_dir / "my_model_v2.mis").touch()
    (temp_asset_dir / "my_model_v2.glb").touch()
    (temp_asset_dir / "sword.glb").touch()

    manager = AssetManager(temp_asset_dir)
    # Fuzzy: "model" should find both "my_model_v1" and "my_model_v2"
    results = manager.find_assets("model")

    assert len(results) == 2
    asset_names = {r["asset"] for r in results}
    assert asset_names == {"my_model_v1", "my_model_v2"}

def test_find_assets_fuzzy_case_insensitive(temp_asset_dir):
    """find_assets fuzzy matching is case-insensitive"""
    (temp_asset_dir / "MyModel.mis").touch()
    (temp_asset_dir / "MyModel.glb").touch()

    manager = AssetManager(temp_asset_dir)
    # Fuzzy: "mymodel" should find "MyModel"
    results = manager.find_assets("mymodel")

    assert len(results) == 1
    assert results[0]["asset"] == "MyModel"

def test_find_assets_glob_still_works(temp_asset_dir):
    """find_assets glob matching still works with wildcards"""
    (temp_asset_dir / "rock.png").touch()
    (temp_asset_dir / "sword.png").touch()
    (temp_asset_dir / "shield.dds").touch()

    manager = AssetManager(temp_asset_dir)
    # Glob: "*.png" should only find PNG files
    results = manager.find_assets("*.png")

    assert len(results) == 2
    asset_names = {r["asset"] for r in results}
    assert asset_names == {"rock", "sword"}
