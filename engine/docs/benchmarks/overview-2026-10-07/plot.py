#!/usr/bin/env python3
"""Render the benchmark overview from its portable, numeric-only source extract."""

import json
from pathlib import Path
from statistics import mean

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from matplotlib.colors import TwoSlopeNorm

ROOT = Path(__file__).resolve().parent
DATA = json.loads((ROOT / "chart-data.json").read_text())["datasets"]
plt.rcParams.update({
    "font.family": "DejaVu Sans", "font.size": 10,
    "axes.spines.top": False, "axes.spines.right": False,
    "axes.spines.left": False, "axes.edgecolor": "#ccd5df",
    "axes.labelcolor": "#334155", "text.color": "#15283f",
    "xtick.color": "#536579", "ytick.color": "#334155",
    "figure.facecolor": "white", "axes.facecolor": "white",
})
COLORS = ["#718096", "#d09a33", "#287ab1", "#198773", "#9964b0", "#cd6846", "#54a1aa"]


def save(fig, name, title, subtitle):
    height = fig.get_figheight()
    fig.suptitle(title, x=.04, y=.99, ha="left", fontsize=19, weight="bold")
    fig.text(.04, .99-.42/height, subtitle, ha="left", va="top", fontsize=10, color="#536579")
    header_inches = .42 + .18*len(subtitle.splitlines()) + .32
    fig.subplots_adjust(top=min(fig.subplotpars.top, .99-header_inches/height))
    fig.savefig(ROOT / (name + ".png"), dpi=170, bbox_inches="tight")
    plt.close(fig)


def bars(ax, dataset, case, modes, metric, labels, title, unit):
    rows = DATA[dataset]["rows"]
    values = [[r[metric] for r in rows if r["case"] == case and r["mode"] == m
               and r.get(metric) is not None] for m in modes]
    present = [(m, label, vals, color) for m, label, vals, color in
               zip(modes, labels, values, COLORS) if vals]
    vmax = max(max(v) for _, _, v, _ in present)
    for i, (_, label, vals, color) in enumerate(present):
        v = mean(vals)
        ax.barh(i, v, color=color, height=.62, zorder=2)
        if len(vals) > 1:
            ax.scatter(vals, np.linspace(i-.15, i+.15, len(vals)),
                       s=15, color="#172638", edgecolors="white", linewidths=.45, zorder=3)
        number = f"{v:.3f}" if metric == "ms" else f"{v:.2f}"
        ax.text(max(v, max(vals)) + vmax*.025, i, number, va="center", fontsize=10)
    ax.set_yticks(range(len(present)), [x[1] for x in present])
    ax.invert_yaxis()
    ax.tick_params(axis="y", length=0)
    ax.set_xlim(0, vmax*1.25)
    ax.xaxis.grid(True, color="#e6ebf1", linewidth=.7, zorder=0)
    ax.set_axisbelow(True)
    ax.set_xlabel(unit, fontsize=9)
    ax.set_title(title, loc="left", fontsize=11, weight="bold", pad=10)


def grid(name, dataset, cases, modes, labels, metrics, title, subtitle):
    fig, axes = plt.subplots(len(cases), len(metrics),
                             figsize=(6*len(metrics), 2.65*len(cases)+1.5), squeeze=False)
    for i, (case, label) in enumerate(cases):
        for j, (metric, unit) in enumerate(metrics):
            bars(axes[i, j], dataset, case, modes, metric, labels, label, unit)
    fig.subplots_adjust(top=.86 if len(cases) < 3 else .88, bottom=.065,
                        left=.15, right=.97, wspace=.6, hspace=.72)
    save(fig, name, title, subtitle)


def heatmap(ax, dataset, cases, modes, labels, metric, title, unit, baseline,
            limit=40, lower_better=False):
    values = []
    for case, _ in cases:
        row = []
        for mode in modes:
            vals = [r[metric] for r in DATA[dataset]["rows"]
                    if r["case"] == case and r["mode"] == mode and r.get(metric) is not None]
            row.append(mean(vals) if vals else np.nan)
        values.append(row)
    values = np.array(values)
    delta = (values / values[:, [modes.index(baseline)]] - 1)*100
    cmap = "RdYlGn_r" if lower_better else "RdYlGn"
    im = ax.imshow(delta, cmap=cmap, norm=TwoSlopeNorm(vmin=-limit, vcenter=0, vmax=limit), aspect="auto")
    for (i, j), v in np.ndenumerate(values):
        text_color = "white" if abs(delta[i, j]) > limit*.8 else "#102136"
        ax.text(j, i, f"{v:.2f}" if np.isfinite(v) else "—", ha="center", va="center",
                fontsize=11, weight="bold", color=text_color)
    ax.set_yticks(range(len(cases)), [v for _, v in cases])
    ax.set_xticks(range(len(modes)), labels)
    ax.tick_params(length=0)
    ax.set_title(title + "\nNumbers: " + unit, loc="left", pad=14, fontsize=12, weight="bold")
    ax.set_xticks(np.arange(-.5, len(modes)), minor=True)
    ax.set_yticks(np.arange(-.5, len(cases)), minor=True)
    ax.grid(which="minor", color="white", linewidth=3)
    ax.tick_params(which="minor", bottom=False, left=False)
    cb = plt.colorbar(im, ax=ax, orientation="horizontal", fraction=.07, pad=.12)
    cb.set_label("Color: % change from " + labels[modes.index(baseline)].replace("\n", " "))


def main():
    fig, axes = plt.subplots(2, 3, figsize=(17, 9))
    early = [
        ("native_capture", "50k capture", ["Before", "Binding reuse"], "ms", "50k sprites: capture optimization", "Capture span, ms ↓"),
        ("native_idle", "30k", ["Direct", "Inline", "Threaded"], "ups", "30k sprites: idle-session repeat", "Updates/s ↑"),
        ("native_mixed", "Mixed crowd 20k", ["Direct", "Inline", "Threaded"], "ups", "20k sprites + GUI + particles", "Updates/s ↑"),
        ("native_mixed", "Mixed balanced 5k", ["Direct", "Inline", "Threaded"], "ups", "5k mixed + simulation work", "Updates/s ↑"),
        ("native_timer", "60 Hz", ["Direct/default", "Thread/default", "Direct/staged", "Thread/staged"], "p99", "Separate timer evaluation: 60 Hz", "Update p99, ms ↓"),
        ("native_timer", "120 Hz", ["Direct/default", "Thread/default", "Direct/staged", "Thread/staged"], "p99", "Separate timer evaluation: 120 Hz", "Update p99, ms ↓"),
    ]
    for ax, (ds, case, modes, metric, title, unit) in zip(axes.flat, early):
        bars(ax, ds, case, modes, metric, modes, title, unit)
    fig.subplots_adjust(top=.85, bottom=.08, left=.13, right=.97, wspace=.65, hspace=.7)
    save(fig, "native-early", "Early native experiments · macOS / Metal",
         "Sep 30–Oct 1 · separate collections per panel · published medians · direct = same-build original path\nTimer trials do not establish an energy saving; 60 Hz interrupt wakeups increased from 203/s to 504/s.")

    cases = [(c, l) for c, l in [
        ("crowd-10000", "Moving sprites 10k"), ("crowd-20000", "Moving sprites 20k"),
        ("crowd-30000", "Moving sprites 30k"), ("mixed-crowd-20k", "Mixed crowd 20k"),
        ("gui-heavy", "GUI heavy"), ("particle-heavy", "Particle heavy"),
        ("render-prep-heavy", "Preparation heavy"), ("fill-negative", "Fill negative control"),
        ("mixed-fill-cap60", "Mixed fill / cap 60"), ("mixed-fill-cap120", "Mixed fill / cap 120")]]
    modes = ["7", "0", "2", "4", "6"]
    labels = ["Vanilla", "PoC\ndirect", "Component\nthread", "Graphics\nthread", "Render-layer\nthread"]
    fig, axes = plt.subplots(1, 2, figsize=(18, 9.5))
    heatmap(axes[0], "native", cases, modes, labels, "ups", "Update throughput", "updates/s ↑", "7", 35)
    heatmap(axes[1], "native", cases, modes, labels, "rss", "Sampled resident process memory", "MiB ↓", "7", 35, True)
    fig.subplots_adjust(top=.84, bottom=.06, left=.13, right=.98, wspace=.4)
    save(fig, "native-architectures", "Three native threading boundaries · all with a vanilla control",
         "Oct 2 · medians of 3 or 6 trials · POSSIBLE WINDOW OCCLUSION: descriptive, not controlled acceptance\nMixed-balanced is omitted because of split pacing regimes; inline controls and full trial ranges remain in the source report.")

    grid("native-replay", "native_game", [("Offline Underwatermelon", "Offline Underwatermelon")],
         ["Direct", "Component thread", "Graphics thread", "Render thread"],
         ["PoC direct", "Component", "Graphics", "Render layer"],
         [("ups", "Uncapped updates/s ↑"), ("rss", "Uncapped peak RSS, MiB ↓"), ("p99", "Separate 60 Hz p99, ms ↓")],
         "Native game replay · no clear throughput winner",
         "Oct 2 · 1,200 measured ticks per run · six repeats · medians · no vanilla timestep-hook control · occlusion caveat")

    fig, axes = plt.subplots(1, 2, figsize=(13, 5))
    bars(axes[0], "bunny_initial", "30k", ["Direct", "Inline", "Threaded"], "ups", ["Direct", "Inline", "Threaded"], "Headless hardware Chrome / 720 × 720", "Updates/s ↑")
    bars(axes[1], "bunny_visible", "30k", ["Direct", "Threaded"], "ups", ["Direct", "Threaded"], "Visible Chrome / 1440 × 1440", "Updates/s ↑")
    fig.subplots_adjust(top=.73, bottom=.15, left=.11, right=.96, wspace=.4)
    save(fig, "web-first-bunny", "Why the first Bunnymark collection stayed at 60",
         "Oct 3 · same Release bundle within each comparison · mean of three runs · dots = individual visible runs\nHeaded/headless mode and resolution changed together; the screenshot's unidentified baseline was not reproduced.")

    fig, axes = plt.subplots(1, 2, figsize=(18, 8))
    cases = [(c, l) for c, l in [("bunny10k", "Bunnies 10k"), ("bunny20k", "Bunnies 20k"),
             ("bunny30k", "Bunnies 30k"), ("render50k", "Static sprites 50k"),
             ("simulation", "Simulation heavy"), ("balanced", "Balanced"), ("fill", "Fill heavy")]]
    heatmap(axes[0], "dispatch", cases, ["direct", "inline", "threaded", "scheduled"],
            ["Direct", "Inline", "rAF", "Completion"], "ups", "Initial scheduler comparison", "updates/s ↑", "direct", 45)
    heatmap(axes[1], "cache", [x for x in cases if x[0] not in ("bunny20k", "simulation")],
            ["direct", "threaded", "scheduled", "cached_raf", "cached_completion", "cached_retry"],
            ["Direct", "Old\nrAF", "Old\ncompletion", "Cache\nrAF", "Cache\ncompletion", "Cache\nretry"],
            "ups", "Window-query cache follow-up", "updates/s ↑", "direct", 45)
    fig.subplots_adjust(top=.82, bottom=.06, left=.1, right=.98, wspace=.35)
    save(fig, "web-schedulers", "Desktop web · scheduler and window ownership experiments",
         "Oct 3 · instrumented collections · mean of three 15-second runs per cell · each panel has its own direct control\nInitial collection used focus emulation; follow-up checked actual focus. This is historical diagnostic evidence.")

    grid("web-overlap", "overlap", [("bunny30k", "30k bunnies"), ("render50k", "50k static sprites"),
         ("balanced", "Balanced simulation + sprites"), ("geometry200", "400 meshes + 1,000 models"),
         ("geometry1000", "2,000 meshes + 5,000 models")],
         ["direct", "serialized", "barrier", "overlap"], ["Direct", "Serialized", "Prep barrier", "Full overlap"],
         [("ups", "Updates/s ↑"), ("live", "Peak live allocation, MiB ↓")],
         "Broad components · simulation and preparation overlap",
         "Oct 4 · rAF scheduler · three 15-second runs per mode · mean bars + run dots · 5 MiB worker stack")

    grid("web-candidate", "candidate", [("bunny30k", "30k bunnies"),
         ("geometry1000", "2,000 meshes + 5,000 models"), ("gameplay_long", "Offline Underwatermelon")],
         ["direct", "overlap_completion"], ["Direct", "Candidate"],
         [("ups", "Updates/s ↑"), ("p99", "Update p99, ms ↓"), ("live", "Peak live allocation, MiB ↓")],
         "Longer desktop evaluation · original broad-component candidate",
         "Oct 4 · three ~120-second runs per mode · means + individual dots · historical candidate, before sprite placement / smaller stack")

    grid("web-placement", "placement", [("bunny30k", "30k bunnies"),
         ("geometry1000", "2,000 meshes + 5,000 models"), ("gameplay_long", "Offline Underwatermelon")],
         ["direct", "current", "deferred", "paced", "combined"],
         ["Direct", "Prior threaded", "Geometry on main", "Paced only", "Both"],
         [("ups", "Updates/s ↑"), ("p99", "Update p99, ms ↓"), ("live", "Peak live allocation, MiB ↓")],
         "Desktop web · moving sprite geometry to browser main",
         "Oct 4 · three runs per mode · means + individual dots · geometry-only/paced-only not collected for model/mesh control")

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    for ax, case, title in [(axes[0, 0], "bunny30k", "30k bunnies: snapshot + stack changes"),
                            (axes[0, 1], "render50k", "50k sprites: snapshot + stack changes")]:
        bars(ax, "compact", case, ["before_direct", "before_threaded", "after_threaded", "after_stack2", "after_linear_stack2"],
             "live", ["Old direct", "Old threaded / 5 MiB", "Compact / 5 MiB", "Compact / 2 MiB", "+ linear WASM growth"], title, "Peak live allocation, MiB ↓")
    bars(axes[1, 0], "owned", "geometry1000", ["direct", "copied", "owned"], "live",
         ["Direct", "Copied uploads", "Owned model uploads"], "7,000 geometry components: upload ownership", "Peak live allocation, MiB ↓")
    bars(axes[1, 1], "stack", "gameplay_long", ["direct", "deferred_5m", "deferred_2m"], "live",
         ["Direct", "Threaded / 5 MiB", "Threaded / 2 MiB"], "Offline Underwatermelon: smaller worker stack", "Peak live allocation, MiB ↓")
    fig.subplots_adjust(top=.84, bottom=.07, left=.19, right=.97, wspace=.65, hspace=.6)
    save(fig, "web-memory", "Memory optimization results · live allocation, not WASM capacity",
         "Oct 3–4 · separate matched experiments per panel · mean of three per-run peaks + individual dots\nDo not join these into one trend: project limits, paths and engine revisions differ.")

    fig, axes = plt.subplots(2, 3, figsize=(18, 8.5))
    for i, ds, title in [(0, "space_desktop", "Desktop Chrome / M1 Pro"), (1, "space_pixel", "Pixel 4a Chrome / repeated warm-device runs")]:
        for j, (metric, unit) in enumerate([("ups", "Updates/s ↑"), ("p99", "Update p99, ms ↓"), ("live", "Peak live allocation, MiB ↓")]):
            bars(axes[i, j], ds, "space_game", ["vanilla", "poc_direct", "threaded", "threaded_ready"], metric,
                 ["Vanilla (pthread)", "PoC direct", "Threaded", "Threaded + ready"], title, unit)
    fig.subplots_adjust(top=.83, bottom=.08, left=.13, right=.97, wspace=.65, hspace=.65)
    save(fig, "space-game", "Space game · real-project comparison on two physical targets",
         "Oct 4 / Oct 6 · three fixed-tick replays per mode · means + individual dots · private game details excluded\nVanilla shares pthread build flags; ordinary non-pthread shipping build was not measured. Do not pool across devices.")

    grid("pixel-latest", "pixel", [("bunny20k", "20k moving sprites"), ("bunny30k", "30k moving sprites"),
         ("fill", "Fill / four passes"), ("geometry100", "200 meshes + 500 models"),
         ("geometry1000", "2,000 meshes + 5,000 models")],
         ["poc_direct", "threaded", "threaded_ready", "threaded_budget"],
         ["PoC direct", "Threaded", "Ready", "Ready + 32 ms budget"],
         [("ups", "Updates/s ↑"), ("p99", "Update p99, ms ↓"), ("live", "Peak live allocation, MiB ↓")],
         "Pixel 4a · latest quiet synthetic comparison",
         "Oct 6 · identical Release bytes within each workload · three 30-second runs per mode · means + individual dots\nPeriodic console output disabled · normal thermal management retained · small geometry is paced at 60 Hz")

    # Export the plotted source aggregates for auditing without reading chart pixels.
    aggregates = {}
    for name, ds in DATA.items():
        result = []
        for case, mode in sorted({(r["case"], r["mode"]) for r in ds["rows"]}):
            rows = [r for r in ds["rows"] if (r["case"], r["mode"]) == (case, mode)]
            entry = {"case": case, "mode": mode}
            for field in ("ups", "p99", "rss", "live", "wasm", "ms"):
                vals = [r[field] for r in rows if r.get(field) is not None]
                if vals:
                    entry[field] = {"mean_of_source_values": mean(vals), "min": min(vals), "max": max(vals), "source_values": len(vals)}
            result.append(entry)
        aggregates[name] = {"source": ds["source"], "aggregation": ds["aggregation"], "values": result}
    (ROOT / "chart-aggregates.json").write_text(json.dumps(aggregates, indent=2) + "\n")


if __name__ == "__main__":
    main()
