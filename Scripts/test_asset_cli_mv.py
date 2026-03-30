"""
Test suite for asset_cli.py mv command to ensure files are moved to correct location.
"""

import pytest
import tempfile
import shutil
from pathlib import Path
from asset_manager import AssetManager
from asset_cli import AssetCLI


class TestMoveCommand:
    """Test the mv command's ability to move files to correct locations"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory with test assets"""
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "Data"
            data_dir.mkdir()

            # Create source directory with test textures
            src_dir = data_dir / "empire_textures"
            src_dir.mkdir()

            # Create a texture asset with multiple files
            (src_dir / "WoodSiding3Color.png").write_text("png data")
            (src_dir / "WoodSiding3Color.dds").write_text("dds data")
            (src_dir / "WoodSiding3Color.tis").write_text("tis data")

            # Create destination directory structure
            dst_base = data_dir / "materials" / "wood_siding"
            dst_base.mkdir(parents=True, exist_ok=True)

            yield data_dir

    def test_move_texture_to_nested_path_without_existing_destination(self, temp_asset_dir):
        """Test moving a texture asset to a nested path where the final directory doesn't exist"""
        manager = AssetManager(temp_asset_dir)

        # Move from empire_textures/WoodSiding3Color to materials/wood_siding/wood_siding_color
        # The destination path materials/wood_siding/wood_siding_color doesn't exist yet
        updated_refs = manager.mv("empire_textures/WoodSiding3Color", "materials/wood_siding/wood_siding_color")

        # Verify files are in the correct location
        assert (temp_asset_dir / "materials" / "wood_siding" / "wood_siding_color.png").exists(), \
            "wood_siding_color.png should exist in materials/wood_siding/"
        assert (temp_asset_dir / "materials" / "wood_siding" / "wood_siding_color.dds").exists(), \
            "wood_siding_color.dds should exist in materials/wood_siding/"
        assert (temp_asset_dir / "materials" / "wood_siding" / "wood_siding_color.tis").exists(), \
            "wood_siding_color.tis should exist in materials/wood_siding/"

        # Verify source files are gone
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists(), \
            "Original WoodSiding3Color.png should be moved"
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.dds").exists(), \
            "Original WoodSiding3Color.dds should be moved"
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.tis").exists(), \
            "Original WoodSiding3Color.tis should be moved"

    def test_move_then_ls_shows_files(self, temp_asset_dir):
        """Test that ls can find files after moving them to a nested path"""
        manager = AssetManager(temp_asset_dir)

        # Move the files
        manager.mv("empire_textures/WoodSiding3Color", "materials/wood_siding/wood_siding_color")

        # Change to materials/wood_siding and ls
        manager.cd("materials/wood_siding")
        assets = manager.ls()

        # Should find wood_siding_color asset
        assert len(assets) > 0, "ls should find assets in materials/wood_siding/"
        asset_names = [asset["asset"] for asset in assets]
        assert "wood_siding_color" in asset_names, \
            f"wood_siding_color should be in assets list. Found: {asset_names}"

    def test_cli_ls_after_move(self, temp_asset_dir):
        """Test that the CLI's ls command shows files after moving"""
        cli = AssetCLI(temp_asset_dir)

        # Move the files via the manager
        cli.manager.mv("empire_textures/WoodSiding3Color", "materials/wood_siding/wood_siding_color")

        # Use ls command to verify files are visible
        output = cli.manager.format_ls("materials/wood_siding")

        assert output, "format_ls should return non-empty output for materials/wood_siding/"
        assert "wood_siding_color" in output, \
            f"wood_siding_color should appear in ls output. Got: {output}"

    def test_simple_rename_in_current_dir(self, temp_asset_dir):
        """Test simple rename (old functionality) still works"""
        manager = AssetManager(temp_asset_dir)

        # Go to source directory
        manager.cd("empire_textures")

        # Simple rename: WoodSiding3Color -> NewName
        manager.mv("WoodSiding3Color", "NewName")

        # Verify files are renamed
        assert (temp_asset_dir / "empire_textures" / "NewName.png").exists()
        assert (temp_asset_dir / "empire_textures" / "NewName.dds").exists()
        assert (temp_asset_dir / "empire_textures" / "NewName.tis").exists()

        # Verify old files are gone
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.dds").exists()
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.tis").exists()

    def test_move_into_existing_directory(self, temp_asset_dir):
        """Test moving into an existing directory (old functionality) still works"""
        manager = AssetManager(temp_asset_dir)

        # Move into an existing directory
        manager.mv("empire_textures/WoodSiding3Color", "materials/wood_siding")

        # Verify files are moved to materials/wood_siding with original name
        assert (temp_asset_dir / "materials" / "wood_siding" / "WoodSiding3Color.png").exists()
        assert (temp_asset_dir / "materials" / "wood_siding" / "WoodSiding3Color.dds").exists()
        assert (temp_asset_dir / "materials" / "wood_siding" / "WoodSiding3Color.tis").exists()

        # Verify source files are gone
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
