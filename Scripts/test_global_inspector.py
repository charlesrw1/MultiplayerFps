import pytest
import sys
import os
from pathlib import Path
sys.path.insert(0, os.path.dirname(__file__))

def test_libclang_loads():
    from global_inspector import load_libclang
    cl = load_libclang()
    assert cl is not None
    idx = cl.Index.create()
    assert idx is not None


def test_find_source_files_basic(tmp_path):
    from global_inspector import find_source_files
    (tmp_path / "foo.cpp").write_text("int x;")
    (tmp_path / "bar.h").write_text("int y;")
    sub = tmp_path / "External"
    sub.mkdir()
    (sub / "third.cpp").write_text("int z;")

    files = find_source_files(str(tmp_path), exclude_dirs=["External"], include_headers=False)
    names = [Path(f).name for f in files]
    assert "foo.cpp" in names
    assert "bar.h" not in names        # headers excluded by default
    assert "third.cpp" not in names    # External excluded


def test_find_source_files_include_headers(tmp_path):
    from global_inspector import find_source_files
    (tmp_path / "foo.cpp").write_text("int x;")
    (tmp_path / "bar.h").write_text("int y;")

    files = find_source_files(str(tmp_path), exclude_dirs=[], include_headers=True)
    names = [Path(f).name for f in files]
    assert "foo.cpp" in names
    assert "bar.h" in names


def test_find_source_files_nested_exclude(tmp_path):
    from global_inspector import find_source_files
    src = tmp_path / "Source"
    src.mkdir()
    (src / "engine.cpp").write_text("int x;")
    ext = src / "External"
    ext.mkdir()
    (ext / "imgui.cpp").write_text("int y;")

    files = find_source_files(str(tmp_path), exclude_dirs=["External"], include_headers=False)
    names = [Path(f).name for f in files]
    assert "engine.cpp" in names
    assert "imgui.cpp" not in names   # nested External excluded


def _make_inspector(cl=None):
    from global_inspector import Inspector, load_libclang
    if cl is None:
        cl = load_libclang()
    return Inspector(cl, exclude_dirs=["External"])


def test_collects_non_static_global(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text("int g_value = 42;\n")
    insp.collect_file_globals(f)
    by_name = {v.name: v for v in insp.globals.values()}
    assert "g_value" in by_name
    assert by_name["g_value"].is_static == False
    assert by_name["g_value"].type_str == "int"


def test_collects_static_global(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text("static float s_time = 0.0f;\n")
    insp.collect_file_globals(f)
    by_name = {v.name: v for v in insp.globals.values()}
    assert "s_time" in by_name
    assert by_name["s_time"].is_static == True


def test_skips_local_variable(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text("void fn() { int local = 1; }\n")
    insp.collect_file_globals(f)
    by_name = {v.name: v for v in insp.globals.values()}
    assert "local" not in by_name


def test_detects_direct_global_reference(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "int g_value = 0;\n"
        "void setter() { g_value = 5; }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    assert "setter" in insp.functions
    fn = insp.functions["setter"]
    assert "g_value" in fn.direct_globals


def test_no_false_positive_for_local(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "void fn() { int local = 1; local = 2; }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    fn = insp.functions.get("fn")
    assert fn is not None
    assert len(fn.direct_globals) == 0


def test_detects_call_edge(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "void helper() {}\n"
        "void caller() { helper(); }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    caller = insp.functions.get("caller")
    assert caller is not None
    assert "helper" in caller.calls


def test_transitive_global_through_call(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "int g_state = 0;\n"
        "void helper() { g_state = 1; }\n"
        "void top() { helper(); }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    report = insp.compute_file_reports()
    fp = str(f)
    assert fp in report
    # top() doesn't touch g_state directly but helper() does
    assert "g_state" not in report[fp]["functions"]["top"]["direct_globals"]
    assert "g_state" in report[fp]["transitive_globals"]


def test_no_duplicate_in_transitive(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "int g_x = 0;\n"
        "void a() { g_x = 1; }\n"
        "void b() { g_x = 2; }\n"
        "void top() { a(); b(); }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    report = insp.compute_file_reports()
    fp = str(f)
    assert report[fp]["transitive_globals"].count("g_x") == 1


def test_cycle_in_call_graph(tmp_path):
    insp = _make_inspector()
    f = tmp_path / "foo.cpp"
    f.write_text(
        "int g_x = 0;\n"
        "void a();\n"
        "void b() { g_x = 1; a(); }\n"
        "void a() { b(); }\n"
    )
    insp.collect_file_globals(f)
    insp.analyze_file_functions(f)
    # Should not hang or crash on cycle
    report = insp.compute_file_reports()
    fp = str(f)
    assert "g_x" in report[fp]["transitive_globals"]
