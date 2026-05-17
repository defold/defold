#!/usr/bin/env python3

import argparse
import datetime
import heapq
import html
import json
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class NinjaLogEntry:
    start: int
    end: int
    mtime: int
    output: str
    command_hash: str


def _repo_root():
    return Path(__file__).resolve().parents[2]


def _find_default_ninja_log(root):
    candidates = []
    for search_root in (root / 'engine' / 'build', root / 'engine'):
        if search_root.exists():
            candidates.extend(search_root.rglob('.ninja_log'))
    if not candidates:
        return None
    return max(candidates, key=lambda path: path.stat().st_mtime)


def _read_ninja_log(path):
    entries = []
    with path.open('r', encoding='utf-8') as f:
        for line in f:
            line = line.rstrip('\n')
            if not line or line.startswith('#'):
                continue
            parts = line.split('\t')
            if len(parts) < 5:
                continue
            try:
                start = int(parts[0])
                end = int(parts[1])
                mtime = int(parts[2])
            except ValueError:
                continue
            if end < start:
                continue
            entries.append(NinjaLogEntry(start, end, mtime, parts[3], parts[4]))
    return entries


def _latest_invocation(entries):
    if not entries:
        return []

    invocation = [entries[-1]]
    previous_end = entries[-1].end
    for entry in reversed(entries[:-1]):
        if entry.end > previous_end:
            break
        invocation.append(entry)
        previous_end = entry.end

    invocation.reverse()
    return invocation


def _classify_output(output):
    lower = output.lower()
    suffix = Path(lower).suffix
    name = Path(output).name

    if name == 'cmake.verify_globs':
        return 'cmake'
    if output == 'CMakeFiles/install.util' or lower.endswith('/cmakefiles/install.util'):
        return 'install'
    if suffix in ('.o', '.obj'):
        return 'compile'
    if suffix in ('.a', '.lib'):
        return 'archive'
    if suffix in ('.dylib', '.so', '.dll', '.exe'):
        return 'link'
    if '/src/test/' in lower and suffix == '':
        return 'test'
    if suffix in ('.cpp', '.cc', '.c', '.h', '.hpp', '.py', '.java', '.jar', '.zip', '.wav', '.luac', '.spv'):
        return 'generate'
    return 'other'


def _short_output(output, root):
    try:
        return str(Path(output).resolve().relative_to(root))
    except (OSError, ValueError):
        return output


def _assign_lanes(entries):
    sorted_entries = sorted(entries, key=lambda entry: (entry.start, entry.end, entry.output))
    lane_heap = []
    free_lanes = []
    next_lane = 0
    result = []

    for entry in sorted_entries:
        while lane_heap and lane_heap[0][0] <= entry.start:
            _, lane = heapq.heappop(lane_heap)
            heapq.heappush(free_lanes, lane)

        if free_lanes:
            lane = heapq.heappop(free_lanes)
        else:
            lane = next_lane
            next_lane += 1

        heapq.heappush(lane_heap, (entry.end, lane))
        result.append((entry, lane))

    return result, next_lane


def _summary(entries, lanes):
    if not entries:
        return {
            'task_count': 0,
            'duration_ms': 0,
            'lane_count': 0,
            'slowest': []
        }

    start = min(entry.start for entry in entries)
    end = max(entry.end for entry in entries)
    slowest = sorted(entries, key=lambda entry: entry.end - entry.start, reverse=True)[:12]
    return {
        'task_count': len(entries),
        'duration_ms': end - start,
        'lane_count': lanes,
        'slowest': [
            {
                'duration_ms': entry.end - entry.start,
                'output': entry.output
            }
            for entry in slowest
        ]
    }


def _make_payload(assigned_entries, root):
    if not assigned_entries:
        return [], 0

    entries = [entry for entry, _ in assigned_entries]
    origin = min(entry.start for entry in entries)
    tasks = []
    for entry, lane in assigned_entries:
        tasks.append({
            'start': entry.start - origin,
            'end': entry.end - origin,
            'duration': entry.end - entry.start,
            'lane': lane,
            'type': _classify_output(entry.output),
            'label': _short_output(entry.output, root),
            'output': entry.output
        })
    return tasks, max((task['end'] for task in tasks), default=0)


def _html_document(payload):
    data = json.dumps(payload, separators=(',', ':'))
    title = html.escape(payload['title'])
    return f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{title}</title>
<style>
:root {{
    color-scheme: light dark;
    --bg: #101418;
    --panel: #161c22;
    --text: #e8eef4;
    --muted: #8d9aa7;
    --grid: #2a333d;
    --hover: #ffffff;
}}
* {{
    box-sizing: border-box;
}}
body {{
    margin: 0;
    background: var(--bg);
    color: var(--text);
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
}}
header {{
    position: sticky;
    top: 0;
    z-index: 2;
    padding: 14px 18px;
    background: rgba(16, 20, 24, 0.94);
    border-bottom: 1px solid var(--grid);
    backdrop-filter: blur(10px);
}}
h1 {{
    margin: 0 0 8px;
    font-size: 18px;
    font-weight: 650;
}}
.meta {{
    display: flex;
    flex-wrap: wrap;
    gap: 8px 18px;
    color: var(--muted);
    font-size: 13px;
}}
.controls {{
    display: flex;
    flex-wrap: wrap;
    gap: 8px;
    align-items: center;
    margin-top: 12px;
}}
button {{
    border: 1px solid #3a4652;
    border-radius: 6px;
    background: #202832;
    color: var(--text);
    padding: 5px 10px;
    font: inherit;
    font-size: 13px;
    cursor: pointer;
}}
button:hover {{
    background: #28323d;
}}
input {{
    min-width: 220px;
    border: 1px solid #3a4652;
    border-radius: 6px;
    background: #0d1116;
    color: var(--text);
    padding: 6px 9px;
    font: inherit;
    font-size: 13px;
}}
.legend {{
    display: flex;
    flex-wrap: wrap;
    gap: 12px;
    margin-top: 10px;
    color: var(--muted);
    font-size: 12px;
}}
.legend span::before {{
    content: "";
    display: inline-block;
    width: 10px;
    height: 10px;
    margin-right: 5px;
    border-radius: 2px;
    background: var(--swatch);
}}
#chartWrap {{
    position: relative;
    width: 100%;
    overflow: hidden;
}}
#chart {{
    display: block;
    width: 100%;
    cursor: grab;
}}
#chart.dragging {{
    cursor: grabbing;
}}
#tooltip {{
    position: fixed;
    z-index: 4;
    display: none;
    max-width: min(760px, calc(100vw - 32px));
    padding: 8px 10px;
    border: 1px solid #46515e;
    border-radius: 6px;
    background: #090c10;
    color: var(--text);
    box-shadow: 0 10px 30px rgba(0, 0, 0, 0.35);
    font-size: 12px;
    pointer-events: none;
    white-space: normal;
    overflow-wrap: anywhere;
}}
#slowest {{
    padding: 12px 18px 24px;
    border-top: 1px solid var(--grid);
    color: var(--muted);
    font-size: 13px;
}}
#slowest h2 {{
    margin: 0 0 8px;
    color: var(--text);
    font-size: 15px;
}}
#slowest ol {{
    margin: 0;
    padding-left: 22px;
}}
#slowest li {{
    margin: 4px 0;
    overflow-wrap: anywhere;
}}
</style>
</head>
<body>
<header>
    <h1>{title}</h1>
    <div class="meta" id="meta"></div>
    <div class="controls">
        <button id="zoomIn">Zoom in</button>
        <button id="zoomOut">Zoom out</button>
        <button id="reset">Reset</button>
        <input id="filter" type="search" placeholder="Highlight target or path">
    </div>
    <div class="legend" id="legend"></div>
</header>
<div id="chartWrap">
    <canvas id="chart"></canvas>
    <div id="tooltip"></div>
</div>
<section id="slowest"></section>
<script>
const DATA = {data};
const COLORS = {{
    compile: "#5aa9e6",
    link: "#f2b84b",
    archive: "#61c27c",
    generate: "#b085f5",
    test: "#f26d6d",
    install: "#7bdff2",
    cmake: "#a6a6a6",
    other: "#d3d3d3"
}};

const canvas = document.getElementById("chart");
const ctx = canvas.getContext("2d");
const tooltip = document.getElementById("tooltip");
const filterInput = document.getElementById("filter");

const layout = {{
    left: 64,
    right: 18,
    top: 34,
    rowHeight: 22,
    barHeight: 15,
    bottom: 22
}};

let viewStart = 0;
let viewSpan = Math.max(DATA.duration_ms, 1);
let hoverTask = null;
let drag = null;
let filterText = "";

function formatDuration(ms) {{
    if (ms < 1000) return `${{ms.toFixed(0)}} ms`;
    if (ms < 60000) return `${{(ms / 1000).toFixed(2)}} s`;
    const minutes = Math.floor(ms / 60000);
    const seconds = ((ms % 60000) / 1000).toFixed(1).padStart(4, "0");
    return `${{minutes}}:${{seconds}}`;
}}

function escapeHtml(value) {{
    return String(value).replace(/[&<>"']/g, (ch) => ({{
        "&": "&amp;",
        "<": "&lt;",
        ">": "&gt;",
        '"': "&quot;",
        "'": "&#39;"
    }}[ch]));
}}

function niceStep(span) {{
    const target = span / 10;
    const power = Math.pow(10, Math.floor(Math.log10(Math.max(target, 1))));
    for (const factor of [1, 2, 5, 10]) {{
        const step = factor * power;
        if (step >= target) return step;
    }}
    return power * 10;
}}

function clampView() {{
    const total = Math.max(DATA.duration_ms, 1);
    const minSpan = Math.max(total / 2000, 1);
    viewSpan = Math.min(Math.max(viewSpan, minSpan), total);
    viewStart = Math.min(Math.max(viewStart, 0), Math.max(total - viewSpan, 0));
}}

function resize() {{
    const dpr = window.devicePixelRatio || 1;
    const cssWidth = Math.max(document.documentElement.clientWidth, 320);
    const cssHeight = layout.top + Math.max(DATA.lane_count, 1) * layout.rowHeight + layout.bottom;
    canvas.style.height = `${{cssHeight}}px`;
    canvas.width = Math.floor(cssWidth * dpr);
    canvas.height = Math.floor(cssHeight * dpr);
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    draw();
}}

function plotWidth() {{
    return Math.max(canvas.clientWidth - layout.left - layout.right, 1);
}}

function timeToX(time) {{
    return layout.left + ((time - viewStart) / viewSpan) * plotWidth();
}}

function xToTime(x) {{
    return viewStart + ((x - layout.left) / plotWidth()) * viewSpan;
}}

function zoomAt(x, factor) {{
    const before = xToTime(x);
    viewSpan *= factor;
    clampView();
    const after = xToTime(x);
    viewStart += before - after;
    clampView();
    draw();
}}

function panBy(deltaMs) {{
    viewStart += deltaMs;
    clampView();
    draw();
}}

function drawGrid(width, height) {{
    ctx.fillStyle = "#101418";
    ctx.fillRect(0, 0, width, height);

    ctx.strokeStyle = "#2a333d";
    ctx.fillStyle = "#8d9aa7";
    ctx.font = "12px -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif";
    ctx.textBaseline = "top";

    const step = niceStep(viewSpan);
    const first = Math.ceil(viewStart / step) * step;
    for (let t = first; t <= viewStart + viewSpan; t += step) {{
        const x = Math.round(timeToX(t)) + 0.5;
        ctx.beginPath();
        ctx.moveTo(x, layout.top - 20);
        ctx.lineTo(x, height - layout.bottom);
        ctx.stroke();
        ctx.fillText(formatDuration(t), x + 4, 8);
    }}

    ctx.strokeStyle = "#25303a";
    for (let lane = 0; lane < DATA.lane_count; lane += 1) {{
        const y = layout.top + lane * layout.rowHeight + layout.rowHeight;
        ctx.beginPath();
        ctx.moveTo(layout.left, y + 0.5);
        ctx.lineTo(width - layout.right, y + 0.5);
        ctx.stroke();
    }}

    ctx.fillStyle = "#8d9aa7";
    ctx.textAlign = "right";
    for (let lane = 0; lane < DATA.lane_count; lane += 1) {{
        const y = layout.top + lane * layout.rowHeight + 3;
        ctx.fillText(`${{lane + 1}}`, layout.left - 10, y);
    }}
    ctx.textAlign = "left";
}}

function drawTasks() {{
    const visibleStart = viewStart;
    const visibleEnd = viewStart + viewSpan;
    const query = filterText.toLowerCase();
    const hovered = hoverTask;

    for (const task of DATA.tasks) {{
        if (task.end < visibleStart || task.start > visibleEnd) continue;

        const x = timeToX(task.start);
        const endX = timeToX(task.end);
        const drawX = Math.max(x, layout.left);
        const drawW = Math.max(Math.min(endX, canvas.clientWidth - layout.right) - drawX, 1.5);
        const y = layout.top + task.lane * layout.rowHeight + 4;
        const isMatch = query && task.label.toLowerCase().includes(query);
        const isHover = hovered === task;

        ctx.globalAlpha = query && !isMatch ? 0.22 : 1;
        ctx.fillStyle = COLORS[task.type] || COLORS.other;
        ctx.fillRect(drawX, y, drawW, layout.barHeight);

        if (isMatch || isHover) {{
            ctx.strokeStyle = "#ffffff";
            ctx.lineWidth = isHover ? 2 : 1;
            ctx.strokeRect(drawX, y, drawW, layout.barHeight);
        }}

        if (drawW > 90) {{
            ctx.save();
            ctx.beginPath();
            ctx.rect(drawX, y, drawW, layout.barHeight);
            ctx.clip();
            ctx.fillStyle = "#081018";
            ctx.font = "11px -apple-system, BlinkMacSystemFont, Segoe UI, sans-serif";
            ctx.textBaseline = "middle";
            ctx.fillText(task.label, drawX + 4, y + layout.barHeight / 2 + 0.5);
            ctx.restore();
        }}
    }}
    ctx.globalAlpha = 1;
}}

function draw() {{
    const width = canvas.clientWidth;
    const height = canvas.clientHeight;
    drawGrid(width, height);
    drawTasks();
}}

function taskAt(x, y) {{
    const time = xToTime(x);
    const lane = Math.floor((y - layout.top) / layout.rowHeight);
    if (lane < 0 || lane >= DATA.lane_count) return null;
    for (const task of DATA.tasks) {{
        if (task.lane === lane && task.start <= time && task.end >= time) {{
            return task;
        }}
    }}
    return null;
}}

function showTooltip(task, event) {{
    if (!task) {{
        tooltip.style.display = "none";
        return;
    }}
    tooltip.innerHTML = `<b>${{formatDuration(task.duration)}}</b> ` +
        `<span style="color:#8d9aa7">${{escapeHtml(task.type)}}</span><br>` +
        `${{escapeHtml(task.label)}}<br>` +
        `<span style="color:#8d9aa7">start ${{formatDuration(task.start)}}, end ${{formatDuration(task.end)}}, lane ${{task.lane + 1}}</span>`;
    tooltip.style.display = "block";
    tooltip.style.left = `${{Math.min(event.clientX + 14, window.innerWidth - tooltip.offsetWidth - 14)}}px`;
    tooltip.style.top = `${{Math.min(event.clientY + 14, window.innerHeight - tooltip.offsetHeight - 14)}}px`;
}}

function updateStaticContent() {{
    document.getElementById("meta").innerHTML = [
        `Source: <code>${{escapeHtml(DATA.source)}}</code>`,
        `Mode: ${{escapeHtml(DATA.mode)}}`,
        `Tasks: ${{DATA.task_count}}`,
        `Duration: ${{formatDuration(DATA.duration_ms)}}`,
        `Inferred lanes: ${{DATA.lane_count}}`,
        `Generated: ${{escapeHtml(DATA.generated_at)}}`
    ].map((item) => `<span>${{item}}</span>`).join("");

    document.getElementById("legend").innerHTML = Object.keys(COLORS).map((key) =>
        `<span style="--swatch:${{COLORS[key]}}">${{key}}</span>`
    ).join("");

    document.getElementById("slowest").innerHTML =
        `<h2>Slowest tasks</h2><ol>` +
        DATA.slowest.map((task) =>
            `<li><b>${{formatDuration(task.duration_ms)}}</b> ${{escapeHtml(task.output)}}</li>`
        ).join("") +
        `</ol>`;
}}

canvas.addEventListener("wheel", (event) => {{
    event.preventDefault();
    if (Math.abs(event.deltaX) > Math.abs(event.deltaY)) {{
        panBy((event.deltaX / plotWidth()) * viewSpan);
    }} else {{
        const factor = Math.exp(event.deltaY * 0.0015);
        zoomAt(event.offsetX, factor);
    }}
}}, {{ passive: false }});

canvas.addEventListener("mousedown", (event) => {{
    drag = {{ x: event.clientX, start: viewStart }};
    canvas.classList.add("dragging");
}});

window.addEventListener("mouseup", () => {{
    drag = null;
    canvas.classList.remove("dragging");
}});

window.addEventListener("mousemove", (event) => {{
    const rect = canvas.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;

    if (drag) {{
        const dx = event.clientX - drag.x;
        viewStart = drag.start - (dx / plotWidth()) * viewSpan;
        clampView();
        draw();
        return;
    }}

    const task = taskAt(x, y);
    if (task !== hoverTask) {{
        hoverTask = task;
        draw();
    }}
    showTooltip(task, event);
}});

canvas.addEventListener("mouseleave", () => {{
    hoverTask = null;
    tooltip.style.display = "none";
    draw();
}});

document.getElementById("zoomIn").addEventListener("click", () => zoomAt(canvas.clientWidth / 2, 0.6));
document.getElementById("zoomOut").addEventListener("click", () => zoomAt(canvas.clientWidth / 2, 1.7));
document.getElementById("reset").addEventListener("click", () => {{
    viewStart = 0;
    viewSpan = Math.max(DATA.duration_ms, 1);
    draw();
}});
filterInput.addEventListener("input", () => {{
    filterText = filterInput.value.trim();
    draw();
}});

window.addEventListener("resize", resize);
updateStaticContent();
resize();
</script>
</body>
</html>
"""


def _write_html(entries, args, ninja_log, root):
    assigned, lane_count = _assign_lanes(entries)
    tasks, duration_ms = _make_payload(assigned, root)
    summary = _summary(entries, lane_count)
    payload = {
        'title': args.title,
        'source': str(ninja_log),
        'mode': 'latest invocation' if args.latest else 'all entries',
        'generated_at': datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
        'task_count': summary['task_count'],
        'duration_ms': duration_ms,
        'lane_count': summary['lane_count'],
        'slowest': [
            {
                'duration_ms': item['duration_ms'],
                'output': _short_output(item['output'], root)
            }
            for item in summary['slowest']
        ],
        'tasks': tasks
    }

    args.output.write_text(_html_document(payload), encoding='utf-8')


def main():
    root = _repo_root()
    parser = argparse.ArgumentParser(description='Render a Ninja build timeline as a self-contained HTML file.')
    parser.add_argument('ninja_log', nargs='?', type=Path, help='Path to .ninja_log. Defaults to the newest engine/**/.ninja_log.')
    parser.add_argument('-o', '--output', type=Path, default=Path('build-tasks.html'), help='Output HTML file. Defaults to build-tasks.html.')
    parser.add_argument('--latest', action='store_true', help='Render only the latest Ninja invocation instead of all .ninja_log entries.')
    parser.add_argument('--title', default='Ninja Build Timeline', help='HTML title.')
    args = parser.parse_args()

    ninja_log = args.ninja_log or _find_default_ninja_log(root)
    if ninja_log is None:
        print('Could not find a .ninja_log under engine/build or engine/*/build.', file=sys.stderr)
        return 1
    ninja_log = ninja_log.resolve()

    entries = _read_ninja_log(ninja_log)
    if not entries:
        print(f'No Ninja log entries found in {ninja_log}', file=sys.stderr)
        return 1

    selected_entries = _latest_invocation(entries) if args.latest else entries
    _write_html(selected_entries, args, ninja_log, root)
    print(f'Wrote {args.output} with {len(selected_entries)} tasks from {ninja_log}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
