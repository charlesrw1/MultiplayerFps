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
    assert (project / "Source" / "AsteroidsComponent.h").is_file()
    starter_class = (project / "Source" / "AsteroidsComponent.h").read_text()
    assert "CLASS_BODY(AsteroidsComponent, spawnable)" in starter_class
    assert "REF int starting_lives = 3;" in starter_class
    assert (project / "Content" / "Scenes" / "Main.tmap").is_file()
    assert (project / "Content" / "scripts" / "main.lua").is_file()
    assert (project / "Content" / "scripts" / "integration_test_framework.lua").is_file()
    assert (project / "Content" / "eng" / "uiDefault.mm").is_file()
    assert (project / "Content" / "eng" / "template.tmap").is_file()
    editor_template = project / "Content" / "eng" / "editor_template.tmap"
    assert editor_template.is_file()
    assert editor_template.read_bytes() == (ENGINE_ROOT / "TestFilesData" / "eng" / "editor_template.tmap").read_bytes()
    assert (project / "Content" / "eng" / "sky_simple.mm").is_file()
    assert (project / "Content" / "eng" / "sky_simple_shader.glsl").is_file()
    assert (project / "Content" / "bricksDefault.mm").is_file()
    assert (project / "Content" / "bricksDefault_shader.glsl").is_file()
    assert (project / "Content" / "woodGrunge.mi").is_file()
    assert (project / "Content" / "wornpaintedwoodsiding-albedo2.dds").is_file()
    assert (project / "Content" / "wornpaintedwoodsiding-normal-ogl.dds").is_file()
    assert (project / "Content" / "wornpaintedwoodsiding-roughness.dds").is_file()
    assert not (project / "Content" / "eng" / "default_bundle.bundle").exists()
    assert (project / "Content" / "sphere.cmdl").is_file()
    assert (project / "Content" / "cube1m.cmdl").is_file()
    assert (project / "Content" / "defaultPBR.mm").is_file()
    assert (project / "Content" / "Material.mi").is_file()
    assert (project / "Content" / "ui" / "fonts" / "inconsolata_regular.ttf").is_file()
    assert (project / "Config" / "Editor" / "imgui.ini").is_file()
    assert (project / "Config" / "Editor" / "editor.ini").is_file()
    assert (project / "Content" / "Models").is_dir()
    assert (project / "Content" / "Textures").is_dir()
    solution = (project / "Asteroids.sln").read_text()
    vcxproj = (project / "Asteroids.vcxproj").read_text()
    assert 'Asteroids.vcxproj' in solution
    assert 'Source\\**\\*.cpp' in vcxproj
    assert 'CsRemake.sln' in vcxproj
    assert 'CsTargetKind=app' in vcxproj
    assert 'LocalDebuggerCommandArguments' in vcxproj
    assert 'Build/project.ini' in vcxproj
    runtime_config = (project / "Build" / "project.ini").read_text()
    assert 'g_project_base "' + project.joinpath("Content").as_posix() + '"' in runtime_config
    assert "g_entry_level" not in runtime_config


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
    assert "starting_lives" in generated
    lua_stubs = output / "lua" / "game_lua_stubs.lua"
    assert lua_stubs.is_file()
    assert "CodegenGameComponent" in lua_stubs.read_text()
