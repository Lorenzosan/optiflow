"""Regression checks for generated backend source-documentation coverage."""

from __future__ import annotations

import ast
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
BACKEND_APP = REPOSITORY_ROOT / "backend" / "app"
PRODUCTION_MODULES = tuple(sorted(BACKEND_APP.glob("*.py")))


def test_doxyfile_includes_python_backend_sources() -> None:
    doxyfile = (REPOSITORY_ROOT / "Doxyfile").read_text(encoding="utf-8")

    assert "backend/README.md" in doxyfile
    assert "backend/app" in doxyfile
    assert "*.py" in doxyfile
    assert "PYTHON_DOCSTRING       = NO" in doxyfile


def test_production_backend_modules_and_top_level_symbols_are_documented() -> None:
    assert PRODUCTION_MODULES

    missing: list[str] = []
    for path in PRODUCTION_MODULES:
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        module_docstring = ast.get_docstring(tree)
        if (
            not module_docstring
            or "@file" not in module_docstring
            or "@brief" not in module_docstring
        ):
            missing.append(f"{path.relative_to(REPOSITORY_ROOT)}: module")

        for node in tree.body:
            if not isinstance(node, (ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            if not ast.get_docstring(node):
                missing.append(f"{path.relative_to(REPOSITORY_ROOT)}: {node.name}")

    assert missing == []
