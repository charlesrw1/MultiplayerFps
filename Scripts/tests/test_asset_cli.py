import pytest
import tempfile
from pathlib import Path
from asset_cli import AssetCLI

@pytest.fixture
def temp_asset_dir():
    """Create temporary asset directory"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)

def test_pwd_command(temp_asset_dir, capsys):
    """pwd command prints current directory"""
    cli = AssetCLI(temp_asset_dir)
    cli.onecmd("pwd")
    captured = capsys.readouterr()

    assert str(temp_asset_dir) in captured.out

def test_cd_command(temp_asset_dir):
    """cd command changes directory"""
    subdir = temp_asset_dir / "textures"
    subdir.mkdir()

    cli = AssetCLI(temp_asset_dir)
    cli.onecmd("cd textures")
    assert cli.manager.pwd() == subdir.resolve()

def test_ls_command(temp_asset_dir, capsys):
    """ls command lists assets"""
    (temp_asset_dir / "rock.tis").touch()
    (temp_asset_dir / "rock.png").touch()

    cli = AssetCLI(temp_asset_dir)
    cli.onecmd("ls")
    captured = capsys.readouterr()

    assert "rock" in captured.out

def test_help_command(capsys):
    """help command shows available commands"""
    with tempfile.TemporaryDirectory() as tmpdir:
        cli = AssetCLI(Path(tmpdir))
        cli.onecmd("help")
        captured = capsys.readouterr()

        assert "ls" in captured.out.lower()
        assert "cd" in captured.out.lower()
