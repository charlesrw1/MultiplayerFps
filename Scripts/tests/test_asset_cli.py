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

@pytest.fixture
def cli_with_test_data():
    """Create AssetCLI instance with test directory structure"""
    with tempfile.TemporaryDirectory() as tmpdir:
        tmpdir = Path(tmpdir)

        # Create directory structure
        (tmpdir / "materials").mkdir()
        (tmpdir / "models").mkdir()
        (tmpdir / "scripts").mkdir()
        (tmpdir / "models" / "weapons").mkdir()

        # Create test files
        (tmpdir / "models" / "my_model.mis").touch()
        (tmpdir / "models" / "my_model.glb").touch()
        (tmpdir / "models" / "my_model.cmdl").touch()

        (tmpdir / "models" / "weapons" / "sword.mis").touch()
        (tmpdir / "models" / "weapons" / "sword.glb").touch()
        (tmpdir / "models" / "weapons" / "sword.cmdl").touch()

        (tmpdir / "my_texture.tis").touch()
        (tmpdir / "my_texture.png").touch()
        (tmpdir / "my_texture.dds").touch()

        cli = AssetCLI(str(tmpdir))
        yield cli

def test_ls_completion_simple_path(cli_with_test_data):
    """Tab completion for 'ls models/' should list models directory contents (just suffix)"""
    completions = cli_with_test_data.complete_ls("", "ls models/", 3, 11)
    # When "/" is in the argument, we return just the suffix (not the full path with prefix)
    assert "my_model" in completions
    assert "weapons/" in completions
    # Should NOT return the full path with prefix duplicated
    assert "models/my_model" not in completions
    assert "models/weapons/" not in completions

def test_ls_completion_partial_match(cli_with_test_data):
    """Tab completion for 'ls models/m' should match my_model"""
    completions = cli_with_test_data.complete_ls("m", "ls models/m", 3, 11)
    assert "my_model" in completions
    assert "weapons/" not in completions
    assert "models/my_model" not in completions

def test_ls_completion_nested_path(cli_with_test_data):
    """Tab completion for nested paths like 'ls models/weapons/'"""
    completions = cli_with_test_data.complete_ls("", "ls models/weapons/", 3, 18)
    assert "sword" in completions
    assert "models/weapons/sword" not in completions

def test_ls_completion_nested_partial(cli_with_test_data):
    """Tab completion for nested partial 'ls models/weapons/s'"""
    completions = cli_with_test_data.complete_ls("s", "ls models/weapons/s", 3, 19)
    assert "sword" in completions
    assert "models/weapons/sword" not in completions

def test_ls_completion_no_duplicate_paths(cli_with_test_data):
    """Tab completion should never suggest duplicate paths like 'models/models'"""
    completions = cli_with_test_data.complete_ls("", "ls models/", 3, 11)

    # Check for any duplicated directory components
    for completion in completions:
        assert "models/models" not in completion, f"Found duplicate path: {completion}"
        # Count how many times "models" appears
        parts = completion.split("/")
        assert parts.count("models") <= 1, f"Duplicate 'models' in: {completion}"

def test_ls_completion_root_directory(cli_with_test_data):
    """Tab completion for root directory 'ls m' should show dirs and assets starting with m"""
    completions = cli_with_test_data.complete_ls("m", "ls m", 3, 4)
    assert "materials/" in completions
    assert "models/" in completions
    assert "my_texture" in completions

def test_mv_completion_with_path(cli_with_test_data):
    """Tab completion for mv with subdirectory path"""
    completions = cli_with_test_data._get_path_completions("", "mv models/my", 3, 13)
    # Returns just the suffix when "/" is in the argument
    assert "my_model" in completions
    assert "models/my_model" not in completions

def test_mv_completion_nested_path(cli_with_test_data):
    """Tab completion for mv with nested paths"""
    completions = cli_with_test_data._get_path_completions("", "mv models/weapons/s", 3, 19)
    assert "sword" in completions
    assert "models/weapons/sword" not in completions

def test_cp_completion_with_subdirectory(cli_with_test_data):
    """Tab completion for cp command with subdirectories"""
    completions = cli_with_test_data._get_path_completions("", "cp models/w", 3, 11)
    # Returns just the suffix when "/" is in the argument
    assert "weapons/" in completions
    # Should NOT have duplicated paths
    assert not any("models/models" in c for c in completions)

def test_trash_completion_shows_dirs(cli_with_test_data):
    """Tab completion for trash shows directories for navigation"""
    completions = cli_with_test_data.complete_trash("", "trash ", 6, 6)
    assert "models/" in completions
    assert "materials/" in completions

def test_trash_completion_subdir(cli_with_test_data):
    """Tab completion for trash navigates into subdirectories"""
    completions = cli_with_test_data.complete_trash("", "trash models/", 6, 13)
    assert "sword" in completions or "weapons/" in completions

def test_cat_completion_shows_dirs(cli_with_test_data):
    """Tab completion for cat shows directories for navigation"""
    completions = cli_with_test_data.complete_cat("", "cat ", 4, 4)
    assert "models/" in completions

def test_references_completion_shows_dirs(cli_with_test_data):
    """Tab completion for references shows directories for navigation"""
    completions = cli_with_test_data.complete_references("", "references ", 11, 11)
    assert "models/" in completions
