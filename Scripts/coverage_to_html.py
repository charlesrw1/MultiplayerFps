#!/usr/bin/env python
"""Convert coverage_summary.md into a pretty interactive HTML report.

Usage:
  py Scripts/coverage_to_html.py [input.md] [output.html]

Defaults: TestFiles/coverage/coverage_summary.md -> TestFiles/coverage/coverage_summary.html
"""
from __future__ import annotations

import html
import json
import re
import sys
from pathlib import Path

HEADER_RE = re.compile(r"^\|\s*file\s*\|", re.IGNORECASE)
SEP_RE = re.compile(r"^\|\s*-+\s*\|")
ROW_RE = re.compile(r"^\|(.+)\|\s*$")
PCT_RE = re.compile(r"(-|\d+)\s*%?")


def parse_md(text: str):
    """Return (meta_lines, rows) where rows is list of {file, game, editor}."""
    meta = []
    rows = []
    in_table = False
    for line in text.splitlines():
        if SEP_RE.match(line):
            in_table = True
            continue
        if HEADER_RE.match(line):
            in_table = False
            continue
        if in_table and line.strip().startswith("|"):
            m = ROW_RE.match(line)
            if not m:
                continue
            cells = [c.strip() for c in m.group(1).split("|")]
            if len(cells) < 3:
                continue
            file_path = cells[0]
            game = parse_pct(cells[1])
            editor = parse_pct(cells[2])
            rows.append({"file": file_path, "game": game, "editor": editor})
        elif not in_table and line.strip() and not line.startswith("|"):
            meta.append(line)
    return meta, rows


def parse_pct(s: str):
    s = s.strip().replace("%", "").strip()
    if s == "-" or s == "":
        return None
    try:
        return int(s)
    except ValueError:
        try:
            return int(float(s))
        except ValueError:
            return None


def trim_path(p: str) -> str:
    """Strip the long shared prefix so the visible file column stays readable."""
    marker = "\\Source\\"
    idx = p.find(marker)
    if idx >= 0:
        return p[idx + len(marker):]
    marker = "/Source/"
    idx = p.find(marker)
    if idx >= 0:
        return p[idx + len(marker):]
    return p


def module_of(short_path: str) -> str:
    norm = short_path.replace("\\", "/")
    parts = norm.split("/")
    return parts[0] if len(parts) > 1 else "(root)"


def best(row):
    g, e = row["game"], row["editor"]
    vals = [v for v in (g, e) if v is not None]
    return max(vals) if vals else 0


def build_html(meta, rows):
    # Stats
    n = len(rows)
    bests = [best(r) for r in rows]
    avg_best = sum(bests) / n if n else 0
    zero = sum(1 for b in bests if b == 0)
    full = sum(1 for b in bests if b >= 100)
    mid_low = sum(1 for b in bests if 1 <= b < 50)
    mid_high = sum(1 for b in bests if 50 <= b < 100)

    # Modules
    mod_stats: dict[str, list[int]] = {}
    for r in rows:
        m = module_of(trim_path(r["file"]))
        mod_stats.setdefault(m, []).append(best(r))
    mods = []
    for m, vals in mod_stats.items():
        mods.append(
            {
                "name": m,
                "count": len(vals),
                "avg": round(sum(vals) / len(vals), 1),
                "zero": sum(1 for v in vals if v == 0),
            }
        )
    mods.sort(key=lambda x: x["avg"])

    # Compact rows for JS
    js_rows = []
    for r in rows:
        short = trim_path(r["file"])
        js_rows.append(
            {
                "f": short,
                "m": module_of(short),
                "g": r["game"],
                "e": r["editor"],
            }
        )

    data_json = json.dumps(js_rows)
    mods_json = json.dumps(mods)
    meta_html = "<br>".join(html.escape(m) for m in meta if m.strip())

    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Coverage Report</title>
<style>
  :root {{
    --bg: #0f1419;
    --panel: #181f2a;
    --panel2: #1f2735;
    --border: #2a3445;
    --text: #d8e1ee;
    --muted: #8a98ad;
    --accent: #5eb1ff;
    --red: #ef4444;
    --orange: #f97316;
    --yellow: #eab308;
    --lime: #84cc16;
    --green: #22c55e;
  }}
  * {{ box-sizing: border-box; }}
  body {{
    margin: 0;
    font: 13px/1.5 -apple-system, "Segoe UI", Roboto, sans-serif;
    background: var(--bg);
    color: var(--text);
  }}
  header {{
    padding: 18px 24px;
    border-bottom: 1px solid var(--border);
    background: var(--panel);
  }}
  h1 {{ margin: 0 0 6px; font-size: 20px; font-weight: 600; }}
  .meta {{ color: var(--muted); font-size: 12px; }}
  .container {{ padding: 16px 24px 40px; max-width: 1400px; margin: 0 auto; }}
  .stats {{
    display: grid;
    grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
    gap: 10px;
    margin-bottom: 18px;
  }}
  .stat {{
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 12px 14px;
  }}
  .stat .v {{ font-size: 22px; font-weight: 600; }}
  .stat .l {{ font-size: 11px; color: var(--muted); text-transform: uppercase; letter-spacing: .04em; }}
  .stat.red .v {{ color: var(--red); }}
  .stat.green .v {{ color: var(--green); }}
  .stat.accent .v {{ color: var(--accent); }}

  .modules {{
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 8px;
    padding: 12px 14px;
    margin-bottom: 18px;
  }}
  .modules h2 {{ margin: 0 0 10px; font-size: 13px; color: var(--muted); text-transform: uppercase; letter-spacing: .04em; font-weight: 600; }}
  .mod-grid {{
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
    gap: 6px;
  }}
  .mod {{
    background: var(--panel2);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 6px 9px;
    cursor: pointer;
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 8px;
    font-size: 12px;
  }}
  .mod:hover {{ border-color: var(--accent); }}
  .mod.active {{ border-color: var(--accent); background: #20324a; }}
  .mod .n {{ overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }}
  .mod .p {{ color: var(--muted); font-variant-numeric: tabular-nums; font-size: 11px; }}

  .toolbar {{
    display: flex;
    gap: 10px;
    margin-bottom: 10px;
    flex-wrap: wrap;
    align-items: center;
  }}
  .toolbar input[type=search] {{
    flex: 1;
    min-width: 240px;
    background: var(--panel);
    border: 1px solid var(--border);
    color: var(--text);
    padding: 8px 12px;
    border-radius: 6px;
    font: inherit;
  }}
  .toolbar input[type=search]:focus {{ outline: none; border-color: var(--accent); }}
  .toolbar label {{ color: var(--muted); display: flex; align-items: center; gap: 5px; font-size: 12px; }}
  .toolbar select {{
    background: var(--panel);
    border: 1px solid var(--border);
    color: var(--text);
    padding: 6px 8px;
    border-radius: 6px;
  }}
  .count {{ color: var(--muted); font-size: 12px; margin-left: auto; }}

  table {{
    width: 100%;
    border-collapse: collapse;
    background: var(--panel);
    border: 1px solid var(--border);
    border-radius: 8px;
    overflow: hidden;
  }}
  thead th {{
    background: var(--panel2);
    text-align: left;
    padding: 8px 12px;
    font-size: 11px;
    text-transform: uppercase;
    letter-spacing: .04em;
    color: var(--muted);
    cursor: pointer;
    user-select: none;
    border-bottom: 1px solid var(--border);
    white-space: nowrap;
  }}
  thead th:hover {{ color: var(--accent); }}
  thead th .arr {{ display: inline-block; width: 10px; color: var(--accent); }}
  tbody td {{
    padding: 4px 12px;
    border-bottom: 1px solid #1c2433;
    vertical-align: middle;
  }}
  tbody tr:hover {{ background: #1a2230; }}
  td.file {{ font-family: ui-monospace, Consolas, monospace; font-size: 12px; }}
  td.mod {{ color: var(--muted); font-size: 11px; white-space: nowrap; }}
  td.pct {{
    width: 240px;
  }}
  .bar {{
    position: relative;
    height: 18px;
    background: #0c1118;
    border-radius: 4px;
    overflow: hidden;
  }}
  .bar > div {{
    position: absolute;
    inset: 0 auto 0 0;
    border-radius: 4px;
  }}
  .bar > span {{
    position: relative;
    z-index: 1;
    display: block;
    text-align: center;
    line-height: 18px;
    font-variant-numeric: tabular-nums;
    font-size: 11px;
    text-shadow: 0 1px 2px rgba(0,0,0,.6);
  }}
  .bar.missing {{ background: repeating-linear-gradient(45deg, #1a2230, #1a2230 4px, #161e2b 4px, #161e2b 8px); }}
  .bar.missing > span {{ color: var(--muted); }}

  .diff {{
    color: var(--muted);
    font-variant-numeric: tabular-nums;
    font-size: 11px;
    width: 60px;
    text-align: right;
  }}
  .diff.big {{ color: var(--orange); font-weight: 600; }}

  footer {{ color: var(--muted); font-size: 11px; margin-top: 18px; text-align: center; }}
</style>
</head>
<body>
<header>
  <h1>Integration test coverage</h1>
  <div class="meta">{meta_html}</div>
</header>
<div class="container">
  <div class="stats">
    <div class="stat accent"><div class="v">{n}</div><div class="l">files</div></div>
    <div class="stat accent"><div class="v">{avg_best:.1f}%</div><div class="l">avg (best of g/e)</div></div>
    <div class="stat red"><div class="v">{zero}</div><div class="l">at 0%</div></div>
    <div class="stat"><div class="v">{mid_low}</div><div class="l">1-49%</div></div>
    <div class="stat"><div class="v">{mid_high}</div><div class="l">50-99%</div></div>
    <div class="stat green"><div class="v">{full}</div><div class="l">at 100%</div></div>
  </div>

  <div class="modules">
    <h2>Modules (click to filter)</h2>
    <div id="mods" class="mod-grid"></div>
  </div>

  <div class="toolbar">
    <input id="q" type="search" placeholder="Filter by path...">
    <label>Show
      <select id="band">
        <option value="all">all</option>
        <option value="zero">0% only</option>
        <option value="low">1-49%</option>
        <option value="mid">50-99%</option>
        <option value="full">100%</option>
        <option value="partial">not 100%</option>
        <option value="diff">game vs editor diff &gt;= 20</option>
      </select>
    </label>
    <span id="count" class="count"></span>
  </div>

  <table id="t">
    <thead>
      <tr>
        <th data-k="m">Module <span class="arr"></span></th>
        <th data-k="f">File <span class="arr"></span></th>
        <th data-k="g">Game <span class="arr"></span></th>
        <th data-k="e">Editor <span class="arr"></span></th>
        <th data-k="d">Δ <span class="arr"></span></th>
      </tr>
    </thead>
    <tbody></tbody>
  </table>

  <footer>Coverage = best per-file (game/editor). Striped bars = file not seen by that mode.</footer>
</div>

<script>
const DATA = {data_json};
const MODS = {mods_json};

function pctColor(v) {{
  if (v === null || v === undefined) return '#33415c';
  if (v >= 80) return 'var(--green)';
  if (v >= 60) return 'var(--lime)';
  if (v >= 40) return 'var(--yellow)';
  if (v >= 20) return 'var(--orange)';
  return 'var(--red)';
}}

function bar(v) {{
  if (v === null || v === undefined) {{
    return '<div class="bar missing"><span>—</span></div>';
  }}
  return `<div class="bar"><div style="width:${{v}}%;background:${{pctColor(v)}};opacity:.75"></div><span>${{v}}%</span></div>`;
}}

function diffOf(r) {{
  if (r.g === null || r.e === null) return null;
  return r.g - r.e;
}}

const state = {{ q: '', band: 'all', module: null, sortKey: 'best', sortDir: 1 }};

function score(r) {{
  const a = r.g === null ? -1 : r.g;
  const b = r.e === null ? -1 : r.e;
  return Math.max(a, b);
}}

function pass(r) {{
  if (state.module && r.m !== state.module) return false;
  if (state.q && !r.f.toLowerCase().includes(state.q)) return false;
  const s = score(r);
  switch (state.band) {{
    case 'zero': return s === 0;
    case 'low': return s >= 1 && s < 50;
    case 'mid': return s >= 50 && s < 100;
    case 'full': return s >= 100;
    case 'partial': return s < 100;
    case 'diff': {{
      const d = diffOf(r);
      return d !== null && Math.abs(d) >= 20;
    }}
  }}
  return true;
}}

function sortVal(r, k) {{
  if (k === 'best') return score(r);
  if (k === 'd') {{
    const d = diffOf(r);
    return d === null ? -999 : d;
  }}
  const v = r[k];
  if (typeof v === 'string') return v;
  return v === null ? -1 : v;
}}

function render() {{
  const rows = DATA.filter(pass);
  rows.sort((a, b) => {{
    const va = sortVal(a, state.sortKey);
    const vb = sortVal(b, state.sortKey);
    if (va < vb) return -state.sortDir;
    if (va > vb) return state.sortDir;
    return a.f.localeCompare(b.f);
  }});
  const tbody = document.querySelector('#t tbody');
  const frag = document.createDocumentFragment();
  for (const r of rows) {{
    const tr = document.createElement('tr');
    const d = diffOf(r);
    const dCls = (d !== null && Math.abs(d) >= 20) ? 'diff big' : 'diff';
    const dTxt = d === null ? '—' : (d > 0 ? '+' + d : d);
    tr.innerHTML = `<td class="mod">${{r.m}}</td><td class="file">${{r.f}}</td><td class="pct">${{bar(r.g)}}</td><td class="pct">${{bar(r.e)}}</td><td class="${{dCls}}">${{dTxt}}</td>`;
    frag.appendChild(tr);
  }}
  tbody.replaceChildren(frag);
  document.getElementById('count').textContent = `${{rows.length}} / ${{DATA.length}} files`;
  document.querySelectorAll('thead th .arr').forEach(s => s.textContent = '');
  const active = document.querySelector(`thead th[data-k="${{state.sortKey}}"] .arr`);
  if (active) active.textContent = state.sortDir > 0 ? '▲' : '▼';
}}

function renderMods() {{
  const root = document.getElementById('mods');
  const all = {{ name: '(all)', count: DATA.length, avg: (DATA.reduce((a,r)=>a+score(r),0)/DATA.length).toFixed(1), zero: 0 }};
  const items = [all, ...MODS];
  root.innerHTML = '';
  for (const m of items) {{
    const el = document.createElement('div');
    el.className = 'mod' + ((state.module === null && m === all) || state.module === m.name ? ' active' : '');
    el.innerHTML = `<span class="n" title="${{m.name}}">${{m.name}}</span><span class="p">${{m.avg}}% · ${{m.count}}</span>`;
    el.onclick = () => {{
      state.module = (m === all) ? null : m.name;
      renderMods();
      render();
    }};
    root.appendChild(el);
  }}
}}

document.getElementById('q').addEventListener('input', e => {{
  state.q = e.target.value.toLowerCase();
  render();
}});
document.getElementById('band').addEventListener('change', e => {{
  state.band = e.target.value;
  render();
}});
document.querySelectorAll('thead th').forEach(th => {{
  th.addEventListener('click', () => {{
    const k = th.dataset.k;
    if (state.sortKey === k) state.sortDir = -state.sortDir;
    else {{ state.sortKey = k; state.sortDir = (k === 'f' || k === 'm') ? 1 : -1; }}
    render();
  }});
}});

renderMods();
render();
</script>
</body>
</html>
"""


def main(argv):
    default_in = Path("TestFiles/coverage/coverage_summary.md")
    in_path = Path(argv[1]) if len(argv) > 1 else default_in
    out_path = Path(argv[2]) if len(argv) > 2 else in_path.with_suffix(".html")
    if not in_path.exists():
        print(f"error: {in_path} not found", file=sys.stderr)
        return 1
    meta, rows = parse_md(in_path.read_text(encoding="utf-8"))
    if not rows:
        print("error: no table rows parsed", file=sys.stderr)
        return 1
    html_str = build_html(meta, rows)
    out_path.write_text(html_str, encoding="utf-8")
    print(f"wrote {out_path} ({len(rows)} rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
