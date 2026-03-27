import pytest
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))

def test_libclang_loads():
    from global_inspector import load_libclang
    cl = load_libclang()
    assert cl is not None
    idx = cl.Index.create()
    assert idx is not None
