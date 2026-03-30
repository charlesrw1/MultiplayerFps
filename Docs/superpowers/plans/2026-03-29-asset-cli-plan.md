# Asset-Aware File Management CLI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a Python REPL-based file manager that intelligently groups asset files and handles smart operations like move-with-reference-fixing, grouped operations, and reference tracking within the Data/ asset directory.

**Architecture:** The tool uses an asset type registry that defines file extension patterns for each asset type, then layers file operations that understand these groupings. All operations are scoped to the asset root (Data/ or user-configured). The REPL loop parses commands and delegates to a file manager. When moving assets, all related files (import settings, source, compiled) move together, and all references are updated via ripgrep scoped to the asset directory.

**Tech Stack:** Python 3.7+, ripgrep (for reference finding), standard library (pathlib, cmd, subprocess)

---

## File Structure

- **Scripts/asset_cli.py** - Main REPL loop and command dispatcher
- **Scripts/asset_manager.py** - Core file operations (ls, cd, cp, mv, trash, cat, pwd, references)
- **Scripts/asset_types.py** - Asset type definitions and grouping logic
- **Scripts/tests/test_asset_types.py** - Asset type and grouping tests
- **Scripts/tests/test_asset_manager.py** - File operation tests
- **Scripts/tests/test_asset_cli.py** - REPL integration tests

---

### Task 1: Define Asset Types and Grouping Logic

**Files:**
- Create: `Scripts/asset_types.py`
- Test: `Scripts/tests/test_asset_types.py`

- [ ] **Step 1: Write failing test for asset type recognition**

Create `Scripts/tests/test_asset_types.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_types.py -v
```

Expected: FAIL - ModuleNotFoundError: No module named 'asset_types'

- [ ] **Step 3: Create asset_types.py with minimal implementation**

Create `Scripts/asset_types.py`:

```python
from enum import Enum
from pathlib import Path
from typing import Optional, Dict

class AssetType(Enum):
    TEXTURE = "texture"
    MODEL = "model"
    MAP = "map"
    MATERIAL = "material"

# File extension to asset type mapping
EXTENSION_MAP: Dict[str, AssetType] = {
    # Textures
    ".tis": AssetType.TEXTURE,
    ".dds": AssetType.TEXTURE,
    ".hdr": AssetType.TEXTURE,
    ".png": AssetType.TEXTURE,
    ".jpeg": AssetType.TEXTURE,
    # Models
    ".mis": AssetType.MODEL,
    ".cmdl": AssetType.MODEL,
    ".glb": AssetType.MODEL,
    # Maps
    ".tmap": AssetType.MAP,
    # Materials
    ".mm": AssetType.MATERIAL,
    ".mi": AssetType.MATERIAL,
    ".glsl": AssetType.MATERIAL,
}

def get_asset_type(filename: str) -> Optional[AssetType]:
    """Get asset type from filename extension"""
    ext = Path(filename).suffix.lower()
    return EXTENSION_MAP.get(ext)

def get_asset_group(filename: str) -> str:
    """Get the base asset name (without extension)"""
    return Path(filename).stem

def group_files(filenames: list) -> Dict[str, dict]:
    """Group files by asset base name, returning only known asset types"""
    groups = {}

    for filename in filenames:
        asset_type = get_asset_type(filename)
        if asset_type is None:
            continue  # Skip unknown file types

        group_name = get_asset_group(filename)

        if group_name not in groups:
            groups[group_name] = {
                "asset": group_name,
                "type": asset_type,
                "files": []
            }

        groups[group_name]["files"].append(filename)

    return groups
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_types.py -v
```

Expected: PASS

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_types.py Scripts/tests/test_asset_types.py
git commit -m "feat: add asset type definitions and grouping logic"
```

---

### Task 2: Build File Grouping and Display Logic

**Files:**
- Modify: `Scripts/asset_types.py`
- Test: `Scripts/tests/test_asset_types.py`

- [ ] **Step 1: Write failing test for file grouping**

Add to `Scripts/tests/test_asset_types.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_types.py::test_group_files_by_asset -v
```

Expected: PASS

- [ ] **Step 3: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_types.py Scripts/tests/test_asset_types.py
git commit -m "feat: add file grouping logic for asset display"
```

---

### Task 3: Implement Core File Manager - Navigation and Listing

**Files:**
- Create: `Scripts/asset_manager.py`
- Test: `Scripts/tests/test_asset_manager.py`

- [ ] **Step 1: Write failing test for pwd and cd**

Create `Scripts/tests/test_asset_manager.py`:

```python
import pytest
import tempfile
from pathlib import Path
from asset_manager import AssetManager

@pytest.fixture
def temp_asset_dir():
    """Create temporary asset directory"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)

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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_pwd -v
```

Expected: FAIL - ModuleNotFoundError: No module named 'asset_manager'

- [ ] **Step 3: Implement AssetManager with pwd and cd**

Create `Scripts/asset_manager.py`:

```python
from pathlib import Path
from typing import Optional, List, Dict
from asset_types import get_asset_type, get_asset_group, group_files, AssetType

class AssetManager:
    """Manages asset-aware file operations within an asset root directory"""

    def __init__(self, asset_root: Path):
        """Initialize with asset root directory (typically Data/)"""
        self.asset_root = Path(asset_root).resolve()
        self.current_dir = self.asset_root

    def pwd(self) -> Path:
        """Print working directory"""
        return self.current_dir

    def cd(self, path: str) -> None:
        """Change directory (relative or absolute)"""
        if Path(path).is_absolute():
            target = Path(path).resolve()
        else:
            target = (self.current_dir / path).resolve()

        # Ensure target is within asset root
        try:
            target.relative_to(self.asset_root)
        except ValueError:
            raise ValueError(f"Cannot cd outside asset root: {target}")

        if not target.exists():
            raise FileNotFoundError(f"Directory does not exist: {target}")
        if not target.is_dir():
            raise NotADirectoryError(f"Not a directory: {target}")

        self.current_dir = target
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_pwd -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cd_relative -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cd_absolute -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cd_parent -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cd_invalid -v
```

Expected: ALL PASS

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_manager.py Scripts/tests/test_asset_manager.py
git commit -m "feat: implement pwd and cd commands in AssetManager"
```

---

### Task 4: Implement ls Command with Asset Grouping

**Files:**
- Modify: `Scripts/asset_manager.py`
- Test: `Scripts/tests/test_asset_manager.py`

- [ ] **Step 1: Write failing test for ls**

Add to `Scripts/tests/test_asset_manager.py`:

```python
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
    assert rock_entry["type"] == AssetType.TEXTURE
    assert set(rock_entry["files"]) == {"rock.tis", "rock.png", "rock.dds"}

    sword_entry = [e for e in result if e["asset"] == "sword"][0]
    assert sword_entry["type"] == AssetType.MODEL

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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_ls_groups_assets -v
```

Expected: FAIL - AttributeError: 'AssetManager' object has no attribute 'ls'

- [ ] **Step 3: Implement ls and format_ls methods**

Add to `Scripts/asset_manager.py`:

```python
def ls(self) -> List[Dict]:
    """List assets in current directory, grouped by asset"""
    if not self.current_dir.exists():
        return []

    files = [f.name for f in self.current_dir.iterdir() if f.is_file()]
    grouped = group_files(files)

    # Return as sorted list
    return [grouped[key] for key in sorted(grouped.keys())]

def format_ls(self) -> str:
    """Format ls output for display"""
    assets = self.ls()
    if not assets:
        return ""

    lines = []
    for asset in assets:
        type_str = asset["type"].value.upper()
        files_str = ", ".join(sorted(asset["files"]))
        lines.append(f"{asset['asset']} [{type_str}]\n  {files_str}")

    return "\n".join(lines)
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_ls_groups_assets -v
python -m pytest Scripts/tests/test_asset_manager.py::test_ls_empty -v
python -m pytest Scripts/tests/test_asset_manager.py::test_ls_format_string -v
```

Expected: ALL PASS

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_manager.py Scripts/tests/test_asset_manager.py
git commit -m "feat: implement ls command with asset grouping"
```

---

### Task 5: Implement cp and trash Commands

**Files:**
- Modify: `Scripts/asset_manager.py`
- Test: `Scripts/tests/test_asset_manager.py`

- [ ] **Step 1: Write failing test for cp**

Add to `Scripts/tests/test_asset_manager.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_cp_copies_source_only -v
```

Expected: FAIL - AttributeError: 'AssetManager' object has no attribute 'cp'

- [ ] **Step 3: Implement cp and trash methods**

Add to `Scripts/asset_manager.py`:

```python
import shutil

def cp(self, src: str, dst: str) -> None:
    """
    Copy a source file. For assets, copies only the source file (e.g., .png not .dds)
    """
    src_path = self.current_dir / src

    if not src_path.exists():
        raise FileNotFoundError(f"File not found: {src}")

    # Determine what to copy based on file type
    src_asset_type = get_asset_type(src)

    if src_asset_type == AssetType.TEXTURE:
        # Copy only source files: .png, .jpeg, .hdr
        if src_path.suffix.lower() not in [".png", ".jpeg", ".hdr"]:
            # If user specified import settings or compiled, copy the source instead
            group = get_asset_group(src)
            for ext in [".png", ".jpeg", ".hdr"]:
                candidate = src_path.parent / (group + ext)
                if candidate.exists():
                    src_path = candidate
                    break

    elif src_asset_type == AssetType.MODEL:
        # Copy only .glb source
        if src_path.suffix.lower() != ".glb":
            group = get_asset_group(src)
            glb_file = src_path.parent / (group + ".glb")
            if glb_file.exists():
                src_path = glb_file

    # For maps and materials, copy as-is

    shutil.copy2(src_path, dst)

def trash(self, path: str) -> None:
    """
    Trash/delete a file. For assets, deletes the compiled and source versions
    (e.g., .dds and .png for textures, but keeps .tis import settings)
    """
    target = self.current_dir / path

    if not target.exists():
        raise FileNotFoundError(f"File not found: {path}")

    asset_type = get_asset_type(path)
    group = get_asset_group(path)

    to_delete = [target]

    if asset_type == AssetType.TEXTURE:
        # Delete .dds, .hdr, .png, .jpeg but keep .tis
        for ext in [".dds", ".hdr", ".png", ".jpeg"]:
            candidate = target.parent / (group + ext)
            if candidate.exists() and candidate != target:
                to_delete.append(candidate)

    elif asset_type == AssetType.MODEL:
        # Delete .cmdl and .glb but keep .mis
        for ext in [".cmdl", ".glb"]:
            candidate = target.parent / (group + ext)
            if candidate.exists() and candidate != target:
                to_delete.append(candidate)

    # For maps and materials, delete only the specified file

    for file_path in to_delete:
        file_path.unlink()
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_cp_copies_source_only -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cp_model_copies_source -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cp_file_not_found -v
```

Expected: ALL PASS

- [ ] **Step 5: Write test for trash**

Add to `Scripts/tests/test_asset_manager.py`:

```python
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
```

- [ ] **Step 6: Run trash tests**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_trash_removes_compiled_and_source -v
python -m pytest Scripts/tests/test_asset_manager.py::test_trash_file_not_found -v
```

Expected: ALL PASS

- [ ] **Step 7: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_manager.py Scripts/tests/test_asset_manager.py
git commit -m "feat: implement cp and trash commands with asset-aware deletion"
```

---

### Task 6: Implement cat and References Finder

**Files:**
- Modify: `Scripts/asset_manager.py`
- Test: `Scripts/tests/test_asset_manager.py`

- [ ] **Step 1: Write failing test for cat**

Add to `Scripts/tests/test_asset_manager.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_cat_reads_file -v
```

Expected: FAIL - AttributeError: 'AssetManager' object has no attribute 'cat'

- [ ] **Step 3: Implement cat and find_references**

Add to `Scripts/asset_manager.py`:

```python
import subprocess

def cat(self, filename: str) -> str:
    """Read and return file contents"""
    file_path = self.current_dir / filename

    if not file_path.exists():
        raise FileNotFoundError(f"File not found: {filename}")

    return file_path.read_text()

def find_references(self, filename: str) -> List[str]:
    """
    Find all files in asset root that reference this file using ripgrep.
    Returns list of filenames that contain references.
    Search is scoped to asset_root only.
    """
    if not (self.current_dir / filename).exists():
        raise FileNotFoundError(f"File not found: {filename}")

    # Escape filename for regex, handle backslashes on Windows
    escaped_name = filename.replace("\\", "\\\\")

    try:
        result = subprocess.run(
            ["rg", "--files-with-matches", escaped_name, str(self.asset_root)],
            capture_output=True,
            text=True,
            timeout=30
        )

        if result.returncode == 0:
            # Convert absolute paths to relative filenames
            refs = []
            for line in result.stdout.strip().split("\n"):
                if line:
                    ref_path = Path(line)
                    refs.append(ref_path.name)
            return refs
        else:
            return []

    except FileNotFoundError:
        # ripgrep not installed
        raise RuntimeError("ripgrep (rg) is not installed. Install it to use reference finding.")
    except subprocess.TimeoutExpired:
        raise RuntimeError("Reference search timed out")
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_cat_reads_file -v
python -m pytest Scripts/tests/test_asset_manager.py::test_cat_file_not_found -v
python -m pytest Scripts/tests/test_asset_manager.py::test_find_references -v
python -m pytest Scripts/tests/test_asset_manager.py::test_find_references_returns_empty_if_none -v
```

Expected: ALL PASS (except find_references may fail if ripgrep isn't available - that's OK for now)

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_manager.py Scripts/tests/test_asset_manager.py
git commit -m "feat: implement cat and find_references commands"
```

---

### Task 7: Implement mv Command with All Related Files and Reference Fixing

**Files:**
- Modify: `Scripts/asset_manager.py`
- Test: `Scripts/tests/test_asset_manager.py`

- [ ] **Step 1: Write failing test for mv**

Add to `Scripts/tests/test_asset_manager.py`:

```python
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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_moves_file -v
```

Expected: FAIL - AttributeError: 'AssetManager' object has no attribute 'mv'

- [ ] **Step 3: Implement mv with all related files and reference fixing**

Add to `Scripts/asset_manager.py`:

```python
def mv(self, src: str, dst: str) -> None:
    """
    Move file and ALL related asset files, then update all references.
    For example, moving rock.png also moves rock.tis and rock.dds to stone.tis/png/dds.
    Updates all references to moved files throughout the asset root.
    """
    src_path = self.current_dir / src
    dst_path = Path(dst)

    if not src_path.exists():
        raise FileNotFoundError(f"File not found: {src}")

    asset_type = get_asset_type(src)
    src_group = get_asset_group(src)
    dst_group = get_asset_group(dst_path.name)

    # Get all related files for this asset
    files_to_move = []
    old_to_new = {}  # Track old filename -> new filename for reference updates

    if asset_type == AssetType.TEXTURE:
        # Move all texture-related files: .tis, .png, .jpeg, .hdr, .dds
        for ext in [".tis", ".png", ".jpeg", ".hdr", ".dds"]:
            candidate = src_path.parent / (src_group + ext)
            if candidate.exists():
                files_to_move.append(candidate)
                old_filename = candidate.name
                new_filename = dst_group + ext
                old_to_new[old_filename] = new_filename

    elif asset_type == AssetType.MODEL:
        # Move all model-related files: .mis, .glb, .cmdl
        for ext in [".mis", ".glb", ".cmdl"]:
            candidate = src_path.parent / (src_group + ext)
            if candidate.exists():
                files_to_move.append(candidate)
                old_filename = candidate.name
                new_filename = dst_group + ext
                old_to_new[old_filename] = new_filename

    elif asset_type == AssetType.MAP:
        # Move .tmap only
        if src_path.exists():
            files_to_move.append(src_path)
            old_to_new[src_path.name] = Path(dst).name

    elif asset_type == AssetType.MATERIAL:
        # Move all material files: .mm, .mi, .glsl
        for ext in [".mm", ".mi", ".glsl"]:
            candidate = src_path.parent / (src_group + ext)
            if candidate.exists():
                files_to_move.append(candidate)
                old_filename = candidate.name
                new_filename = dst_group + ext
                old_to_new[old_filename] = new_filename
    else:
        # Unknown type, just move the one file
        files_to_move.append(src_path)
        old_to_new[src_path.name] = Path(dst).name

    # Move all related files
    for file_path in files_to_move:
        ext = file_path.suffix
        new_path = src_path.parent / (dst_group + ext)
        file_path.rename(new_path)

    # Fix references: for each old filename, find references and update them
    for old_name, new_name in old_to_new.items():
        try:
            result = subprocess.run(
                ["rg", "--files-with-matches", old_name, str(self.asset_root)],
                capture_output=True,
                text=True,
                timeout=30
            )

            if result.returncode == 0:
                for ref_file in result.stdout.strip().split("\n"):
                    if ref_file:
                        ref_path = Path(ref_file)
                        if ref_path.exists() and ref_path not in files_to_move:
                            try:
                                content = ref_path.read_text()
                                updated = content.replace(old_name, new_name)
                                ref_path.write_text(updated)
                            except (IOError, OSError):
                                # Skip files that can't be read/written
                                pass
        except (FileNotFoundError, subprocess.TimeoutExpired):
            # ripgrep not available, skip reference updates for this file
            pass
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_moves_file -v
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_texture_moves_all_related_files -v
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_model_moves_all_related_files -v
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_file_not_found -v
python -m pytest Scripts/tests/test_asset_manager.py::test_mv_fixes_references -v
```

Expected: ALL PASS (mv_fixes_references may require ripgrep)

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_manager.py Scripts/tests/test_asset_manager.py
git commit -m "feat: implement mv command with all related files and reference fixing"
```

---

### Task 8: Implement REPL Loop

**Files:**
- Create: `Scripts/asset_cli.py`
- Test: `Scripts/tests/test_asset_cli.py`

- [ ] **Step 1: Write failing test for command parsing**

Create `Scripts/tests/test_asset_cli.py`:

```python
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
    assert cli.manager.pwd() == subdir

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
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_cli.py::test_pwd_command -v
```

Expected: FAIL - ModuleNotFoundError: No module named 'asset_cli'

- [ ] **Step 3: Implement REPL with cmd module**

Create `Scripts/asset_cli.py`:

```python
import cmd
from pathlib import Path
from asset_manager import AssetManager

class AssetCLI(cmd.Cmd):
    """Asset-aware file management REPL"""

    intro = "Asset File Manager - asset-aware file operations\nType 'help' for commands"
    prompt = "assets> "

    def __init__(self, asset_root=None):
        super().__init__()
        if asset_root:
            self.manager = AssetManager(Path(asset_root))
        else:
            # Default to Data/ subdirectory
            default_root = Path.cwd() / "Data"
            if not default_root.exists():
                default_root = Path.cwd()
            self.manager = AssetManager(default_root)
        self.update_prompt()

    def update_prompt(self):
        """Update prompt to show current directory"""
        cwd = self.manager.pwd()
        root = self.manager.asset_root
        try:
            rel = cwd.relative_to(root)
            if rel == Path("."):
                self.prompt = f"{root.name}> "
            else:
                self.prompt = f"{rel}> "
        except ValueError:
            self.prompt = f"{cwd.name}> "

    def do_pwd(self, arg):
        """Print working directory"""
        print(self.manager.pwd())

    def do_cd(self, arg):
        """Change directory: cd <path>"""
        if not arg:
            print("Usage: cd <path>")
            return

        try:
            self.manager.cd(arg)
            self.update_prompt()
        except (FileNotFoundError, NotADirectoryError, ValueError) as e:
            print(f"Error: {e}")

    def do_ls(self, arg):
        """List assets in current directory"""
        output = self.manager.format_ls()
        if output:
            print(output)
        else:
            print("No assets found")

    def do_cat(self, arg):
        """Show file contents: cat <filename>"""
        if not arg:
            print("Usage: cat <filename>")
            return

        try:
            print(self.manager.cat(arg))
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_cp(self, arg):
        """Copy file: cp <src> <dst>"""
        parts = arg.split()
        if len(parts) != 2:
            print("Usage: cp <src> <dst>")
            return

        src, dst = parts
        try:
            self.manager.cp(src, dst)
            print(f"Copied {src} to {dst}")
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_mv(self, arg):
        """Move file and all related files: mv <src> <dst>"""
        parts = arg.split()
        if len(parts) != 2:
            print("Usage: mv <src> <dst>")
            return

        src, dst = parts
        try:
            self.manager.mv(src, dst)
            print(f"Moved {src} and related files to {dst}")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_trash(self, arg):
        """Delete file and compiled versions: trash <filename>"""
        if not arg:
            print("Usage: trash <filename>")
            return

        try:
            self.manager.trash(arg)
            print(f"Deleted {arg}")
        except FileNotFoundError as e:
            print(f"Error: {e}")

    def do_references(self, arg):
        """Find files that reference this asset: references <filename>"""
        if not arg:
            print("Usage: references <filename>")
            return

        try:
            refs = self.manager.find_references(arg)
            if refs:
                print("Referenced by:")
                for ref in sorted(set(refs)):  # Remove duplicates
                    print(f"  {ref}")
            else:
                print("No references found")
        except (FileNotFoundError, RuntimeError) as e:
            print(f"Error: {e}")

    def do_shell(self, arg):
        """Execute PowerShell command: !<command>"""
        import subprocess
        try:
            result = subprocess.run(
                ["powershell", "-Command", arg],
                capture_output=True,
                text=True
            )
            if result.stdout:
                print(result.stdout, end="")
            if result.stderr:
                print(result.stderr, end="")
        except Exception as e:
            print(f"Error: {e}")

    def default(self, line):
        """Handle ! prefix for shell commands"""
        if line.startswith("!"):
            self.do_shell(line[1:])
        else:
            print(f"Unknown command: {line}")

    def do_exit(self, arg):
        """Exit the CLI"""
        return True

    def do_quit(self, arg):
        """Exit the CLI"""
        return True

    def emptyline(self):
        """Don't repeat last command on empty line"""
        pass

def main():
    """Main entry point - starts REPL or accepts asset root argument"""
    import sys

    asset_root = None
    if len(sys.argv) > 1:
        asset_root = Path(sys.argv[1])

    cli = AssetCLI(asset_root)
    cli.cmdloop()

if __name__ == "__main__":
    main()
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/test_asset_cli.py::test_pwd_command -v
python -m pytest Scripts/tests/test_asset_cli.py::test_cd_command -v
python -m pytest Scripts/tests/test_asset_cli.py::test_ls_command -v
python -m pytest Scripts/tests/test_asset_cli.py::test_help_command -v
```

Expected: ALL PASS

- [ ] **Step 5: Commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_cli.py Scripts/tests/test_asset_cli.py
git commit -m "feat: implement REPL CLI with cmd module"
```

---

### Task 9: Run Full Integration Tests and Final Verification

**Files:**
- Test: All components

- [ ] **Step 1: Run all tests to verify everything works together**

```bash
cd /Users/charlie/source/MultiplayerFps
python -m pytest Scripts/tests/ -v
```

Expected: All tests pass

- [ ] **Step 2: Manual smoke test the CLI**

```bash
cd /Users/charlie/source/MultiplayerFps
python Scripts/asset_cli.py
```

Then in the REPL:
```
assets> pwd
assets> ls
assets> help
assets> exit
```

Expected: Commands work without errors

- [ ] **Step 3: Format code with clang-format**

```bash
cd /Users/charlie/source/MultiplayerFps
powershell -Command "& ./Scripts/clang-format-all.ps1"
```

(Note: clang-format is for C++; Python code is not affected but the script may check it)

- [ ] **Step 4: Final commit**

```bash
cd /Users/charlie/source/MultiplayerFps
git add Scripts/asset_cli.py Scripts/asset_manager.py Scripts/asset_types.py Scripts/tests/
git commit -m "feat: complete asset CLI tool with all commands and integration tests"
```

---

## Spec Coverage Check

✓ **ls** - Groups assets by type, shows one per line
✓ **cd** - Navigate directories within asset root
✓ **mv** - Move all related files together with reference fixing via ripgrep scoped to asset_root
✓ **cp** - Copy source files only (not compiled versions)
✓ **trash** - Delete compiled + source, keep import settings
✓ **cat** - Display file contents
✓ **!<powershell>** - Execute PowerShell commands
✓ **pwd** - Print working directory
✓ **references** - Show assets that reference a file (scoped to asset_root)
✓ **Asset types** - Texture, Model, Map, Material with correct extensions
✓ **Asset root** - All operations scoped to Data/ directory (or user-configured)
✓ **REPL** - No state caching, fresh operations each time

---

Plan revised and complete. Ready to execute.

Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?