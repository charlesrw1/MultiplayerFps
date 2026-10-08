import json
import subprocess
from pathlib import Path


ENGINE_ROOT = Path(__file__).resolve().parents[1]
CSCLI = ENGINE_ROOT / "x64" / "Debug" / "cscli.exe"


def test_new_external_project(tmp_path):
    project = tmp_path / "Asteroids"
    result = subprocess.run(
        [str(CSCLI), "new", "Asteroids", "--path", str(project)],
        cwd=ENGINE_ROOT,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    manifest = json.loads((project / "game.json").read_text())
    assert manifest["name"] == "Asteroids"
    assert Path(manifest["engine"]).resolve() == ENGINE_ROOT
    assert manifest["startupScene"] == "Scenes/Main.tmap"
    assert (project / "Source" / "AsteroidsComponent.h").is_file()
    assert (project / "Content" / "Scenes" / "Main.tmap").is_file()
    assert (project / "Content" / "scripts" / "main.lua").is_file()


def test_external_project_codegen(tmp_path):
    project = tmp_path / "CodegenGame"
    subprocess.run(
        [str(CSCLI), "new", "CodegenGame", "--path", str(project)],
        cwd=ENGINE_ROOT,
        check=True,
    )
    output = project / "Build" / "generated"
    result = subprocess.run(
        [
            "py",
            str(ENGINE_ROOT / "Scripts" / "external_game_codegen.py"),
            "--engine-root",
            str(ENGINE_ROOT),
            "--game-root",
            str(project),
            "--output",
            str(output),
        ],
        cwd=ENGINE_ROOT,
        capture_output=True,
        text=True,
    )

    assert result.returncode == 0, result.stderr
    generated = (output / "ExternalGame.gen.cpp").read_text()
    assert "CodegenGameComponent::StaticType" in generated
    assert (output / "lua" / "game_lua_stubs.lua").is_file()
