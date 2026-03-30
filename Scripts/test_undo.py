"""
Test suite for undo functionality in asset_cli.py
"""

import pytest
import tempfile
from pathlib import Path
from asset_manager import AssetManager
from asset_cli import AssetCLI


class TestUndoMv:
    """Test undo functionality for mv command"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory with test assets and reference files"""
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

            # Create reference files that reference this texture
            (data_dir / "empireWoodSiding.mi").write_text(
                """TYPE MaterialInstance
PARENT defaultEmpire.mm
VAR Albedo WoodSiding3Color.dds
VAR Normalmap empire_textures/WoodSiding3Normal.dds
"""
            )
            (data_dir / "default_bundle.bundle").write_text("references WoodSiding3Color.dds")

            # Create destination directory structure
            dst_base = data_dir / "materials" / "wood_siding"
            dst_base.mkdir(parents=True, exist_ok=True)

            yield data_dir

    def test_undo_mv_files_restored(self, temp_asset_dir):
        """Test that undo restores files after mv"""
        manager = AssetManager(temp_asset_dir)
        manager.cd("empire_textures")

        # Move the asset within empire_textures
        updated_refs, undo_record = manager.mv("WoodSiding3Color", "NewName")

        # Verify files are moved
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()
        assert (temp_asset_dir / "empire_textures" / "NewName.png").exists()

        # Undo the move
        manager.undo(undo_record)

        # Verify files are back in original location
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.dds").exists()
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.tis").exists()
        assert not (temp_asset_dir / "empire_textures" / "NewName.png").exists()

    def test_undo_mv_references_restored(self, temp_asset_dir):
        """Test that undo restores references to original names"""
        manager = AssetManager(temp_asset_dir)

        # Move the asset
        updated_refs, undo_record = manager.mv("empire_textures/WoodSiding3Color", "NewName")

        # Verify references were updated
        mi_content = (temp_asset_dir / "empireWoodSiding.mi").read_text()
        assert "NewName.dds" in mi_content

        # Undo the move
        manager.undo(undo_record)

        # Verify references are restored
        mi_content = (temp_asset_dir / "empireWoodSiding.mi").read_text()
        assert "WoodSiding3Color.dds" in mi_content
        assert "NewName.dds" not in mi_content

    def test_undo_mv_with_jpg(self, temp_asset_dir):
        """Test that undo restores .jpg files"""
        manager = AssetManager(temp_asset_dir)
        manager.cd("empire_textures")

        # Create a texture with .jpg
        (temp_asset_dir / "empire_textures" / "JpegTexture.jpg").write_text("jpg data")
        (temp_asset_dir / "empire_textures" / "JpegTexture.dds").write_text("dds data")

        # Move it
        updated_refs, undo_record = manager.mv("JpegTexture", "RenamedJpeg")

        # Verify files are moved
        assert not (temp_asset_dir / "empire_textures" / "JpegTexture.jpg").exists()
        assert (temp_asset_dir / "empire_textures" / "RenamedJpeg.jpg").exists()

        # Undo
        manager.undo(undo_record)

        # Verify files are back including .jpg
        assert (temp_asset_dir / "empire_textures" / "JpegTexture.jpg").exists()
        assert (temp_asset_dir / "empire_textures" / "JpegTexture.dds").exists()
        assert not (temp_asset_dir / "empire_textures" / "RenamedJpeg.jpg").exists()

    def test_undo_mv_cross_directory(self, temp_asset_dir):
        """Test undo for mv across directories"""
        manager = AssetManager(temp_asset_dir)

        # Move to nested path
        updated_refs, undo_record = manager.mv(
            "empire_textures/WoodSiding3Color",
            "materials/wood_siding/wood_siding_color"
        )

        # Verify files are in new location
        assert (temp_asset_dir / "materials" / "wood_siding" / "wood_siding_color.png").exists()
        assert not (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()

        # Undo
        manager.undo(undo_record)

        # Verify files back in original location
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.png").exists()
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.dds").exists()
        assert (temp_asset_dir / "empire_textures" / "WoodSiding3Color.tis").exists()
        assert not (temp_asset_dir / "materials" / "wood_siding" / "wood_siding_color.png").exists()


class TestUndoCp:
    """Test undo functionality for cp command"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory with test assets"""
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "Data"
            data_dir.mkdir()

            # Create a test file
            (data_dir / "test.txt").write_text("test content")

            yield data_dir

    def test_undo_cp_file_removed(self, temp_asset_dir):
        """Test that undo removes the copied file"""
        manager = AssetManager(temp_asset_dir)

        # Copy file
        undo_record = manager.cp("test.txt", "test_copy.txt")

        # Verify copy exists
        assert (temp_asset_dir / "test_copy.txt").exists()

        # Undo
        manager.undo(undo_record)

        # Verify copy is gone, source still exists
        assert not (temp_asset_dir / "test_copy.txt").exists()
        assert (temp_asset_dir / "test.txt").exists()


class TestUndoMkdir:
    """Test undo functionality for mkdir command"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory"""
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "Data"
            data_dir.mkdir()
            yield data_dir

    def test_undo_mkdir_dir_removed(self, temp_asset_dir):
        """Test that undo removes the created directory"""
        manager = AssetManager(temp_asset_dir)

        # Create directory
        undo_record = manager.mkdir("new_dir")

        # Verify directory exists
        assert (temp_asset_dir / "new_dir").exists()

        # Undo
        manager.undo(undo_record)

        # Verify directory is gone
        assert not (temp_asset_dir / "new_dir").exists()

    def test_undo_mkdir_fails_if_not_empty(self, temp_asset_dir):
        """Test that undo fails gracefully if directory is not empty"""
        manager = AssetManager(temp_asset_dir)

        # Create directory and add a file to it
        undo_record = manager.mkdir("new_dir")
        (temp_asset_dir / "new_dir" / "file.txt").write_text("content")

        # Undo should raise OSError because rmdir fails on non-empty dir
        with pytest.raises(OSError):
            manager.undo(undo_record)

        # Directory should still exist with the file
        assert (temp_asset_dir / "new_dir" / "file.txt").exists()


class TestUndoTrash:
    """Test undo functionality for trash command"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory with test assets"""
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "Data"
            data_dir.mkdir()

            # Create a texture asset
            (data_dir / "test_texture.png").write_text("png data")
            (data_dir / "test_texture.dds").write_text("dds data")
            (data_dir / "test_texture.tis").write_text("tis data")

            yield data_dir

    def test_undo_trash_files_restored(self, temp_asset_dir):
        """Test that undo restores deleted files"""
        manager = AssetManager(temp_asset_dir)

        # Trash the asset
        undo_record = manager.trash("test_texture")

        # Verify files are gone
        assert not (temp_asset_dir / "test_texture.png").exists()
        assert not (temp_asset_dir / "test_texture.dds").exists()
        assert not (temp_asset_dir / "test_texture.tis").exists()

        # Undo
        manager.undo(undo_record)

        # Verify files are restored
        assert (temp_asset_dir / "test_texture.png").exists()
        assert (temp_asset_dir / "test_texture.dds").exists()
        assert (temp_asset_dir / "test_texture.tis").exists()

    def test_undo_trash_jpg_files(self, temp_asset_dir):
        """Test that trash and undo work with .jpg files"""
        manager = AssetManager(temp_asset_dir)

        # Create a texture with .jpg
        (temp_asset_dir / "jpg_texture.jpg").write_text("jpg data")
        (temp_asset_dir / "jpg_texture.dds").write_text("dds data")

        # Trash the asset
        undo_record = manager.trash("jpg_texture")

        # Verify files are gone
        assert not (temp_asset_dir / "jpg_texture.jpg").exists()
        assert not (temp_asset_dir / "jpg_texture.dds").exists()

        # Undo
        manager.undo(undo_record)

        # Verify .jpg is restored
        assert (temp_asset_dir / "jpg_texture.jpg").exists()
        assert (temp_asset_dir / "jpg_texture.dds").exists()

    def test_undo_trash_content_preserved(self, temp_asset_dir):
        """Test that undo preserves original file content"""
        manager = AssetManager(temp_asset_dir)

        original_content = "test png data content"
        (temp_asset_dir / "test_texture.png").write_text(original_content)

        # Trash and undo
        undo_record = manager.trash("test_texture")
        manager.undo(undo_record)

        # Verify content is preserved
        restored_content = (temp_asset_dir / "test_texture.png").read_text()
        assert restored_content == original_content


class TestUndoCli:
    """Test undo functionality at the CLI level"""

    @pytest.fixture
    def temp_asset_dir(self):
        """Create a temporary directory with test assets"""
        with tempfile.TemporaryDirectory() as tmpdir:
            data_dir = Path(tmpdir) / "Data"
            data_dir.mkdir()
            (data_dir / "test.txt").write_text("test")
            yield data_dir

    def test_cli_undo_nothing_to_undo(self, temp_asset_dir, capsys):
        """Test that undo prints 'Nothing to undo' when no prior operation"""
        cli = AssetCLI(temp_asset_dir)
        cli.do_undo("")

        captured = capsys.readouterr()
        assert "Nothing to undo" in captured.out

    def test_cli_undo_clears_record(self, temp_asset_dir, capsys):
        """Test that undo clears the record after first undo"""
        cli = AssetCLI(temp_asset_dir)

        # Perform an operation
        cli.do_cp("test.txt test_copy.txt")

        # Undo it
        cli.do_undo("")

        # Try to undo again - should print "Nothing to undo"
        cli.do_undo("")
        captured = capsys.readouterr()
        assert "Nothing to undo" in captured.out


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
