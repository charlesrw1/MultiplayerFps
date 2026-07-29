#!/usr/bin/env python3
"""Static site generator for docs/ -> docs_site/<theme>/.

Parses [[wikilink]] / [[wikilink#Header]] / [[wikilink#Header|text]] syntax used
across docs/*.md, builds a nav tree from the docs/ folder structure (each
directory may carry an index.toml for {title, order, hidden}; each page may
carry a `+++ ... +++` TOML frontmatter block for {title, order, hidden}), and
renders themed HTML pages with pygments code highlighting.
"""
import argparse
import re
import shutil
import tomllib
from pathlib import Path

import markdown
from markdown.extensions.toc import TocExtension

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
OUT_ROOT = ROOT / "docs_site"
THEMES_DIR = Path(__file__).resolve().parent / "docs_site_themes"

WIKILINK_RE = re.compile(r"\[\[([^\]]+)\]\]")
FRONTMATTER_RE = re.compile(r"\A\+\+\+\r?\n(.*?\r?\n)\+\+\+\r?\n?", re.S)

TOPLEVEL_KEY = ""  # virtual section for .md files directly under docs/
SKIP_DIRS = {"images"}


def slugify(text: str) -> str:
    s = re.sub(r"[^\w\s-]", "", text.lower()).strip()
    return re.sub(r"[\s_]+", "-", s)


def prettify(name: str) -> str:
    return name.replace("_", " ").replace("-", " ").title()


def load_toml(path: Path) -> dict:
    if not path.exists():
        return {}
    with path.open("rb") as f:
        return tomllib.load(f)


def extract_frontmatter(md_text: str) -> tuple[dict, str]:
    m = FRONTMATTER_RE.match(md_text)
    if not m:
        return {}, md_text
    try:
        meta = tomllib.loads(m.group(1))
    except tomllib.TOMLDecodeError:
        meta = {}
    return meta, md_text[m.end():]


def discover_docs() -> dict[str, list[Path]]:
    """basename (no ext) -> list of matching rel Paths, for wikilink basename fallback resolution."""
    by_basename: dict[str, list[Path]] = {}
    for p in DOCS.rglob("*.md"):
        if p.name == "index.md":
            continue
        rel = p.relative_to(DOCS).with_suffix("")
        by_basename.setdefault(p.stem, []).append(rel)
    return by_basename


def resolve_wikilink(raw: str, current_rel: Path, by_basename: dict) -> tuple[str | None, str]:
    """Returns (href, display_text) for a [[...]] body."""
    body = raw
    text_override = None
    if "|" in body:
        body, text_override = body.split("|", 1)
    header = None
    if "#" in body:
        file_part, header = body.split("#", 1)
    else:
        file_part = body

    if not file_part:
        # same-file header link
        href = f"#{slugify(header)}" if header else "#"
        display = text_override or (header or "")
        return href, display

    # Non-.md extension => source file reference, not a doc page
    if "." in Path(file_part).name and not file_part.endswith(".md"):
        display = text_override or file_part
        return None, display  # caller renders as plain code text

    candidate_rel = Path(file_part.removesuffix(".md"))
    target_rel = None
    if (DOCS / candidate_rel.with_suffix(".md")).exists():
        target_rel = candidate_rel
    else:
        matches = by_basename.get(candidate_rel.name)
        if matches and len(matches) == 1:
            target_rel = matches[0]

    display = text_override or header or file_part.rsplit("/", 1)[-1]

    if target_rel is None:
        return None, display

    href = "/" + str(target_rel).replace("\\", "/") + ".html"
    if header:
        href += f"#{slugify(header)}"
    return href, display


def preprocess_wikilinks(text: str, current_rel: Path, by_basename: dict) -> str:
    def repl(m: re.Match) -> str:
        href, display = resolve_wikilink(m.group(1), current_rel, by_basename)
        if href is None:
            return f"`{display}`"
        return f"[{display}]({href})"

    # Skip fenced code blocks
    parts = re.split(r"(```.*?```)", text, flags=re.S)
    for i, part in enumerate(parts):
        if i % 2 == 0:
            parts[i] = WIKILINK_RE.sub(repl, part)
    return "".join(parts)


class NavFile:
    def __init__(self, rel: Path, title: str, order: float, hidden: bool):
        self.rel = rel
        self.title = title
        self.order = order
        self.hidden = hidden


class NavFolder:
    def __init__(self, key: str, title: str, order: float, hidden: bool):
        self.key = key  # top-level dir name, "" for the virtual root section
        self.title = title
        self.order = order
        self.hidden = hidden
        self.files: list[NavFile] = []

    def sorted_files(self) -> list[NavFile]:
        return sorted(self.files, key=lambda f: (f.order, f.title.lower()))


def build_nav_tree(pages_meta: dict[Path, dict]) -> list[NavFolder]:
    """pages_meta: rel path -> {"title":, "order":, "hidden":} for every page."""
    root_cfg = load_toml(DOCS / "index.toml")
    sections: dict[str, NavFolder] = {
        TOPLEVEL_KEY: NavFolder(
            TOPLEVEL_KEY,
            root_cfg.get("title", "Top-level"),
            root_cfg.get("order", -100),
            False,
        )
    }

    for d in sorted(p for p in DOCS.iterdir() if p.is_dir() and p.name not in SKIP_DIRS):
        cfg = load_toml(d / "index.toml")
        sections[d.name] = NavFolder(
            d.name,
            cfg.get("title", prettify(d.name)),
            cfg.get("order", 0),
            cfg.get("hidden", False),
        )

    for rel, meta in pages_meta.items():
        key = rel.parts[0] if len(rel.parts) > 1 else TOPLEVEL_KEY
        sec = sections.setdefault(key, NavFolder(key, prettify(key), 0, False))
        sec.files.append(NavFile(rel, meta["title"], meta["order"], meta["hidden"]))

    return sorted(
        (s for s in sections.values() if s.files),
        key=lambda s: (s.order, s.title.lower()),
    )


def render_markdown(md_text: str, rel: Path, by_basename: dict) -> tuple[str, str, list]:
    md_text = preprocess_wikilinks(md_text, rel, by_basename)
    md = markdown.Markdown(extensions=[
        "fenced_code", "tables", "sane_lists", "attr_list",
        "codehilite",
        TocExtension(anchorlink=False, permalink=False),
    ], extension_configs={"codehilite": {"css_class": "codehilite", "guess_lang": False}})
    html = md.convert(md_text)
    title_match = re.search(r"<h1[^>]*>(.*?)</h1>", html)
    title = re.sub(r"<[^>]+>", "", title_match.group(1)) if title_match else rel.name
    return html, title, md.toc_tokens


def render_toc_box(toc_tokens: list) -> str:
    """MediaWiki-style 'Contents' box, collapsible, listing h2/h3 headings."""
    # python-markdown's toc nests everything under the page's own h1; that's
    # already shown as the page title, so descend past it.
    while len(toc_tokens) == 1 and toc_tokens[0]["level"] == 1:
        toc_tokens = toc_tokens[0]["children"]
    top = [t for t in toc_tokens if t["level"] <= 3]
    if len(top) < 2:
        return ""

    def render_items(tokens: list) -> str:
        out = ['<ol>']
        for t in tokens:
            out.append(f'<li><a href="#{t["id"]}">{t["name"]}</a>')
            children = [c for c in t.get("children", []) if c["level"] <= 3]
            if children:
                out.append(render_items(children))
            out.append("</li>")
        out.append("</ol>")
        return "".join(out)

    return (
        '<details class="page-toc" open>'
        '<summary>Contents</summary>'
        f'{render_items(top)}'
        '</details>'
    )


def build(theme: str):
    theme_dir = THEMES_DIR / theme
    if not theme_dir.exists():
        raise SystemExit(f"unknown theme '{theme}', looked in {theme_dir}")
    layout = (theme_dir / "layout.html").read_text(encoding="utf-8")
    css = (theme_dir / "style.css").read_text(encoding="utf-8")

    out_dir = OUT_ROOT / theme
    if out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    (out_dir / "style.css").write_text(css, encoding="utf-8")

    from pygments.formatters import HtmlFormatter
    pygments_style = (theme_dir / "pygments_style.txt").read_text(encoding="utf-8").strip() \
        if (theme_dir / "pygments_style.txt").exists() else "default"
    (out_dir / "pygments.css").write_text(
        HtmlFormatter(style=pygments_style).get_style_defs(".codehilite"), encoding="utf-8")

    if (DOCS / "images").exists():
        shutil.copytree(DOCS / "images", out_dir / "images")

    by_basename = discover_docs()

    md_files = sorted(p for p in DOCS.rglob("*.md") if p.name != "README.md")

    pages = []  # (rel, title, body_html, meta)
    pages_meta: dict[Path, dict] = {}
    for p in md_files:
        rel = p.relative_to(DOCS).with_suffix("")
        raw = p.read_text(encoding="utf-8")
        meta, md_text = extract_frontmatter(raw)
        body_html, h1_title, toc_tokens = render_markdown(md_text, rel, by_basename)
        title = meta.get("title") or h1_title
        order = meta.get("order", 0)
        hidden = meta.get("hidden", False)
        pages_meta[rel] = {"title": title, "order": order, "hidden": hidden}
        toc_html = render_toc_box(toc_tokens)
        if toc_html:
            body_html = re.sub(r"(</h1>)", r"\1" + toc_html.replace("\\", "\\\\"), body_html, count=1)
        pages.append((rel, title, body_html))

    nav = build_nav_tree(pages_meta)

    def nav_html(current_rel: Path) -> str:
        current_key = current_rel.parts[0] if len(current_rel.parts) > 1 else TOPLEVEL_KEY
        out = ['<div class="nav-tree">']
        for sec in nav:
            if sec.hidden:
                continue
            is_open = " open" if sec.key == current_key else ""
            out.append(f'<details class="nav-folder"{is_open}><summary>{sec.title}</summary><ul>')
            for f in sec.sorted_files():
                if f.hidden:
                    continue
                href = "/" + str(f.rel).replace("\\", "/") + ".html"
                active = " class=\"active\"" if f.rel == current_rel else ""
                out.append(f'<li><a href="{href}"{active}>{f.title}</a></li>')
            out.append("</ul></details>")
        out.append("</div>")
        return "".join(out)

    def breadcrumb_html(rel: Path, title: str) -> str:
        crumbs = ['<a href="/index.html">Docs Home</a>']
        parts = rel.parts[:-1]
        for part in parts:
            crumbs.append(f'<span class="sep">&gt;</span> <span class="crumb-dir">{part.replace("_", " ").title()}</span>')
        crumbs.append(f'<span class="sep">&gt;</span> <span class="crumb-here">{title}</span>')
        return " ".join(crumbs)

    def render_page(title: str, nav_str: str, content: str, rel: Path) -> str:
        return (layout
                .replace("__TITLE__", title)
                .replace("__NAV__", nav_str)
                .replace("__CONTENT__", content)
                .replace("__BREADCRUMB__", breadcrumb_html(rel, title)))

    for rel, title, body_html in pages:
        page_html = render_page(title, nav_html(rel), body_html, rel)
        out_file = out_dir / (str(rel) + ".html")
        out_file.parent.mkdir(parents=True, exist_ok=True)
        out_file.write_text(page_html, encoding="utf-8")

    # index.html = curated landing page (docs/README.md content), full nav tree in sidebar
    readme_rel = Path("README")
    readme_raw = (DOCS / "README.md").read_text(encoding="utf-8")
    _, readme_md = extract_frontmatter(readme_raw)
    readme_html, _, _ = render_markdown(readme_md, readme_rel, by_basename)
    index_html = render_page("Documentation", nav_html(Path()), readme_html, readme_rel)
    (out_dir / "index.html").write_text(index_html, encoding="utf-8")

    print(f"[{theme}] built {len(pages)} pages -> {out_dir}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--theme", action="append", help="theme name(s); default = all discovered themes")
    args = ap.parse_args()

    themes = args.theme or [d.name for d in THEMES_DIR.iterdir() if d.is_dir()]
    for t in themes:
        build(t)


if __name__ == "__main__":
    main()
