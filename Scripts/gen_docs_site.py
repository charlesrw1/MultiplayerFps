#!/usr/bin/env python3
"""Static site generator for docs/ -> docs_site/<theme>/.

Parses [[wikilink]] / [[wikilink#Header]] / [[wikilink#Header|text]] syntax used
across docs/*.md, builds a nav tree from docs/README.md's structure, and renders
themed HTML pages with pygments code highlighting.
"""
import argparse
import re
import shutil
from pathlib import Path

import markdown
from markdown.extensions.toc import TocExtension

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
OUT_ROOT = ROOT / "docs_site"
THEMES_DIR = Path(__file__).resolve().parent / "docs_site_themes"

WIKILINK_RE = re.compile(r"\[\[([^\]]+)\]\]")


def slugify(text: str) -> str:
    s = re.sub(r"[^\w\s-]", "", text.lower()).strip()
    return re.sub(r"[\s_]+", "-", s)


def discover_docs() -> dict[str, list[Path]]:
    """basename (no ext) -> list of matching rel Paths, for wikilink basename fallback resolution."""
    by_basename: dict[str, list[Path]] = {}
    for p in DOCS.rglob("*.md"):
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


def parse_readme_nav() -> list[dict]:
    """Parse docs/README.md '## Section' + '- [[link]] — desc' into a nav tree."""
    text = (DOCS / "README.md").read_text(encoding="utf-8")
    sections: list[dict] = []
    current = None
    for line in text.splitlines():
        h2 = re.match(r"^##\s+(.*)", line)
        item = re.match(r"^-\s+\[\[([^\]]+)\]\]\s*(?:—|--|-)?\s*(.*)", line)
        if h2:
            current = {"title": h2.group(1).strip(), "items": []}
            sections.append(current)
        elif item and current is not None:
            link_body = item.group(1)
            desc = item.group(2).strip()
            file_part = link_body.split("#")[0].split("|")[0]
            current["items"].append({
                "rel": file_part,
                "desc": desc,
            })
    return sections


def render_markdown(md_text: str, rel: Path, by_basename: dict) -> tuple[str, str]:
    md_text = preprocess_wikilinks(md_text, rel, by_basename)
    md = markdown.Markdown(extensions=[
        "fenced_code", "tables", "sane_lists", "attr_list",
        "codehilite",
        TocExtension(anchorlink=False, permalink=False),
    ], extension_configs={"codehilite": {"css_class": "codehilite", "guess_lang": False}})
    html = md.convert(md_text)
    title_match = re.search(r"<h1[^>]*>(.*?)</h1>", html)
    title = re.sub(r"<[^>]+>", "", title_match.group(1)) if title_match else rel.name
    return html, title


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
    nav = parse_readme_nav()
    nav_rels = {item["rel"] for sec in nav for item in sec["items"]}

    md_files = sorted(DOCS.rglob("*.md"))
    all_rels = []
    for p in md_files:
        rel = p.relative_to(DOCS).with_suffix("")
        all_rels.append(rel)

    def nav_html(current_rel: Path) -> str:
        out = []
        for sec in nav:
            out.append(f'<div class="nav-section"><h3>{sec["title"]}</h3><ul>')
            for it in sec["items"]:
                rel = it["rel"]
                href = "/" + rel + ".html"
                active = " class=\"active\"" if rel == str(current_rel).replace("\\", "/") else ""
                out.append(f'<li><a href="{href}"{active}>{Path(rel).name}</a></li>')
            out.append("</ul></div>")
        # unlisted docs (not referenced from README)
        unlisted = [r for r in all_rels if str(r).replace("\\", "/") not in nav_rels]
        if unlisted:
            out.append('<div class="nav-section"><h3>Other</h3><ul>')
            for rel in unlisted:
                href = "/" + str(rel).replace("\\", "/") + ".html"
                active = " class=\"active\"" if rel == current_rel else ""
                out.append(f'<li><a href="{href}"{active}>{rel.name}</a></li>')
            out.append("</ul></div>")
        return "".join(out)

    pages = []
    for p in md_files:
        rel = p.relative_to(DOCS).with_suffix("")
        md_text = p.read_text(encoding="utf-8")
        body_html, title = render_markdown(md_text, rel, by_basename)
        pages.append((rel, title, body_html))

    def breadcrumb_html(rel: Path, title: str) -> str:
        crumbs = ['<a href="/index.html">Docs Home</a>']
        parts = rel.parts[:-1]
        acc = []
        for part in parts:
            acc.append(part)
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

    # index.html = README rendered, plus redirect-friendly copy
    readme_rel = Path("README")
    readme_html, _ = render_markdown((DOCS / "README.md").read_text(encoding="utf-8"), readme_rel, by_basename)
    index_html = render_page("Documentation", nav_html(readme_rel), readme_html, readme_rel)
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
