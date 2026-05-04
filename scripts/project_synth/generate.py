from __future__ import annotations

from bisect import bisect_left
from collections import defaultdict
import binascii
from dataclasses import dataclass, field
import json
from math import ceil
from pathlib import Path
import random
import shutil
import struct
import zlib

from project_synth.progress import ProgressReporter
from project_synth.profile import profile_project
from project_synth.proto import EMBEDDED_PROTO_BY_TYPE
from project_synth.proto import text_format


ROOT = Path(__file__).resolve().parents[2]
SOURCE_PROFILE_FILENAME = ".project_synth.source_profile.json"

SEED_PNG = ROOT / "editor" / "test" / "resources" / "test_project" / "graphics" / "ball.png"
SEED_TTF = ROOT / "editor" / "test" / "resources" / "test_project" / "fonts" / "vera_mo_bd.ttf"

PNG_DIMENSION_BUCKETS = (32, 48, 64, 96, 128, 192, 256, 384)
PNG_MIN_DIMENSION = 32
PNG_MAX_DIMENSION = 384
PNG_TARGET_BYTES_FLOOR = 8 * 1024
PNG_TARGET_BYTES_CEILING = 160 * 1024

COLLECTION_BRANCH_FACTOR = 8
GO_PER_COLLECTION = 6
PROXIES_PER_GO = 2
GO_ASSETS_PER_KIND = 4
GUI_ASSETS_PER_KIND = 3
ATLAS_IMAGES_PER_ATLAS = 24
TILESOURCES_PER_PARTICLEFX = 2
LUA_ATTACHMENTS_PER_SCRIPT = 6
PNG_PATTERN_VARIANTS = 16

PNG_TEMPLATE_CACHE: dict[tuple[int, int, int], bytes] = {}

BOOTSTRAP_MINIMUMS = {
    "collection": 1,
    "go": 1,
    "render": 1,
    "render_script": 1,
    "script": 1,
}

DIRECTORY_BY_EXTENSION = {
    "atlas": "atlas",
    "collection": "collections",
    "collectionproxy": "proxies",
    "font": "fonts",
    "fp": "shaders",
    "go": "game_objects",
    "gui": "guis",
    "gui_script": "scripts",
    "lua": "lua",
    "material": "materials",
    "particlefx": "particles",
    "png": "images",
    "render": "render",
    "render_script": "render",
    "script": "scripts",
    "tilesource": "tilesources",
    "vp": "shaders",
}

PREFIX_BY_EXTENSION = {
    "atlas": "atlas",
    "collection": "collection",
    "collectionproxy": "proxy",
    "font": "font",
    "fp": "fragment",
    "go": "go",
    "gui": "gui",
    "gui_script": "gui_script",
    "lua": "module",
    "material": "material",
    "particlefx": "particlefx",
    "png": "image",
    "render": "render",
    "render_script": "render_script",
    "script": "script",
    "tilesource": "tilesource",
    "vp": "vertex",
}

GUI_NODE_TYPES = ("TYPE_BOX", "TYPE_TEXT")

SAFE_ASSET_PORTAL_LIBRARY_ZIPS = (
    "https://github.com/britzl/monarch/archive/refs/tags/5.2.0.zip",
    "https://github.com/ivanquirino/defold-tiny-ecs/archive/refs/tags/1.3-3.zip",
    "https://github.com/Insality/defold-log/archive/refs/tags/6.zip",
    "https://github.com/Insality/defold-token/archive/refs/tags/5.zip",
    "https://github.com/baochungit/i18n-defold/archive/refs/tags/1.0.0.zip",
    "https://github.com/Insality/defold-tweener/archive/refs/tags/6.zip",
    "https://github.com/baochungit/defold-wrap/archive/refs/tags/1.0.0.zip",
    "https://github.com/astrochili/narrator/archive/refs/tags/1.8.zip",
    "https://github.com/paweljarosz/lua-immutable/archive/refs/tags/v1.1.zip",
    "https://github.com/Jrayp/Moku/archive/refs/tags/Alpha_v2.2.zip",
    "https://github.com/subsoap/defquest/archive/refs/tags/v1.1.0.zip",
    "https://github.com/britzl/defold-orthographic/archive/refs/tags/3.4.1.zip",
    "https://github.com/britzl/gooey/archive/refs/tags/10.5.2.zip",
    "https://github.com/Insality/druid/archive/refs/tags/1.2.1.zip",
    "https://github.com/8bitskull/dicebag/archive/refs/tags/0.3.zip",
    "https://github.com/astrochili/defold-operator/archive/refs/tags/1.5.3.zip",
    "https://github.com/indiesoftby/defold-pointer-lock/archive/refs/tags/2.0.0.zip",
    "https://github.com/britzl/defold-richtext/archive/refs/tags/5.22.1.zip",
)


@dataclass
class Node:
    extension: str
    index: int
    path: str
    dag_rank: int = 0
    outgoing_target: int = 0
    assigned_outgoing_count: int = 0
    edges: dict[str, list[str]] = field(default_factory=lambda: defaultdict(list))
    inbound_count: int = 0
    properties: dict[str, int] = field(default_factory=dict)
    target_path_depth: int = 0
    target_size_bytes: int = 0


def generate_project(
    *,
    out_project_path: Path,
    source_project_path: Path | None,
    profile_path: Path | None,
    seed: int,
    scale: float,
) -> None:
    if (source_project_path is None) == (profile_path is None):
        raise ValueError("Exactly one of source_project_path or profile_path must be supplied.")
    if scale <= 0:
        raise ValueError("Scale must be greater than zero.")

    if source_project_path is not None:
        profile = profile_project(source_project_path.resolve())
    else:
        profile = json.loads(profile_path.read_text(encoding="utf-8"))

    effective_profile = scale_profile(profile, scale)
    generation_reporter = ProgressReporter(
        estimate_generation_step_count(effective_profile),
        phase="generation",
    )
    generation_reporter.advance(detail="scaled profile")
    out_project_path = out_project_path.resolve()
    clear_output_directory(out_project_path)
    generation_reporter.advance(detail=f"prepared {out_project_path}")

    nodes_by_extension = create_nodes(effective_profile)
    assign_dag_ranks(nodes_by_extension)
    assign_topology(effective_profile, nodes_by_extension)
    assign_resource_targets(effective_profile, nodes_by_extension)
    assign_paths(nodes_by_extension)
    assign_outgoing_targets(effective_profile, nodes_by_extension)
    allocate_edges(seed, effective_profile, nodes_by_extension, generation_reporter)
    promote_main_reachability(nodes_by_extension)
    generation_reporter.advance(detail="allocated edges")
    write_project(out_project_path, effective_profile, nodes_by_extension, seed, generation_reporter)


def clear_output_directory(out_project_path: Path) -> None:
    if out_project_path.exists() and not out_project_path.is_dir():
        raise ValueError(f"Output path exists and is not a directory: {out_project_path}")

    out_project_path.mkdir(parents=True, exist_ok=True)

    for child in out_project_path.iterdir():
        if child.name == ".git":
            continue
        if child.is_dir() and not child.is_symlink():
            shutil.rmtree(child)
        else:
            child.unlink()


def estimate_generation_step_count(profile: dict) -> int:
    base_steps = 3
    resource_count = sum(profile.get("resources", {}).get("by_extension", {}).values())
    bootstrap_files = 5
    return base_steps + bootstrap_files + resource_count


def scale_profile(profile: dict, scale: float) -> dict:
    cloned = json.loads(json.dumps(profile))

    def scale_value(value: int) -> int:
        return max(0, int(round(value * scale)))

    resource_counts = cloned.setdefault("resources", {}).setdefault("by_extension", {})
    for extension, minimum in BOOTSTRAP_MINIMUMS.items():
        resource_counts[extension] = max(scale_value(resource_counts.get(extension, 0)), minimum)
    for extension in list(resource_counts.keys()):
        if extension not in BOOTSTRAP_MINIMUMS:
            resource_counts[extension] = scale_value(resource_counts[extension])

    dependencies = cloned.setdefault("dependencies", {})
    dependencies["count"] = scale_value(dependencies.get("count", 0))

    references = cloned.setdefault("references", {})
    by_source_type = references.setdefault("by_source_type", {})
    for source_extension, target_counts in by_source_type.items():
        for target_extension in list(target_counts.keys()):
            target_counts[target_extension] = scale_value(target_counts[target_extension])

    histograms = references.setdefault("outgoing_fanout_histogram", {})
    for source_extension, histogram in histograms.items():
        for bucket in list(histogram.keys()):
            histogram[bucket] = scale_value(histogram[bucket])

    topology = cloned.setdefault("topology", {})
    for metric_name, histogram in topology.items():
        for bucket in list(histogram.keys()):
            histogram[bucket] = scale_value(histogram[bucket])

    references["total_edges"] = sum(
        count
        for target_counts in by_source_type.values()
        for count in target_counts.values()
    )
    return cloned


def create_nodes(profile: dict) -> dict[str, list[Node]]:
    nodes_by_extension: dict[str, list[Node]] = {}
    resource_counts = profile.get("resources", {}).get("by_extension", {})

    for extension, count in sorted(resource_counts.items()):
        if count <= 0:
            continue
        dir_name = DIRECTORY_BY_EXTENSION.get(extension, extension)
        prefix = PREFIX_BY_EXTENSION.get(extension, extension)
        nodes: list[Node] = []
        for index in range(count):
            if extension == "collection" and index == 0:
                path = "/main/main.collection"
            elif extension == "go" and index == 0:
                path = "/main/main.go"
            elif extension == "script" and index == 0:
                path = "/main/main.script"
            elif extension == "render" and index == 0:
                path = "/render/default.render"
            elif extension == "render_script" and index == 0:
                path = "/render/default.render_script"
            else:
                path = f"/{dir_name}/{prefix}_{index + 1:04d}.{extension}"
            nodes.append(Node(extension=extension, index=index, path=path))
        nodes_by_extension[extension] = nodes

    return nodes_by_extension


def is_bootstrap_node(node: Node) -> bool:
    return node.path in {
        "/main/main.collection",
        "/main/main.go",
        "/main/main.script",
        "/render/default.render",
        "/render/default.render_script",
    }


def parse_bucket(bucket: str) -> tuple[int, int]:
    start, end = bucket.split("-")
    return int(start), int(end)


def bucket_midpoint(bucket: str) -> int:
    start, end = parse_bucket(bucket)
    return (start + end) // 2


def expand_histogram(histogram: dict[str, int], count: int, total: int | None = None) -> list[int]:
    values: list[int] = []
    for bucket, bucket_count in sorted(histogram.items(), key=lambda item: parse_bucket(item[0])):
        values.extend([bucket_midpoint(bucket)] * max(0, bucket_count))

    if count <= 0:
        return []

    if not values:
        values = [0] * count
    elif len(values) < count:
        average = int(round(sum(values) / max(1, len(values))))
        values.extend([average] * (count - len(values)))
    elif len(values) > count:
        values = values[:count]

    if total is not None and values:
        current_total = sum(values)
        delta = total - current_total
        index = 0
        while delta != 0 and values:
            slot = index % len(values)
            if delta > 0:
                values[slot] += 1
                delta -= 1
            elif values[slot] > 0:
                values[slot] -= 1
                delta += 1
            index += 1
            if index > len(values) * max(1, abs(total - current_total) + 1):
                break

    return values


def assign_outgoing_targets(profile: dict, nodes_by_extension: dict[str, list[Node]]) -> None:
    references = profile.get("references", {})
    by_source_type = references.get("by_source_type", {})
    histograms = references.get("outgoing_fanout_histogram", {})

    for extension, nodes in nodes_by_extension.items():
        total_edges = sum(by_source_type.get(extension, {}).values())
        degrees = expand_histogram(histograms.get(extension, {}), len(nodes), total=total_edges)
        if len(degrees) < len(nodes):
            degrees.extend([0] * (len(nodes) - len(degrees)))
        for node, degree in zip(nodes, degrees):
            node.outgoing_target = max(0, degree)


def assign_metric(metric_histograms: dict, metric_name: str, count: int, total: int | None = None) -> list[int]:
    histogram = metric_histograms.get(metric_name, {})
    return expand_histogram(histogram, count, total=total)


def assign_resource_targets(profile: dict, nodes_by_extension: dict[str, list[Node]]) -> None:
    resources = profile.get("resources", {})
    size_histograms = resources.get("size_histogram_by_extension", {})
    depth_histograms = resources.get("path_depth_histogram_by_extension", {})

    for extension, nodes in nodes_by_extension.items():
        size_targets = expand_histogram(size_histograms.get(extension, {}), len(nodes))
        depth_targets = expand_histogram(depth_histograms.get(extension, {}), len(nodes))
        if len(size_targets) < len(nodes):
            size_targets.extend([0] * (len(nodes) - len(size_targets)))
        if len(depth_targets) < len(nodes):
            depth_targets.extend([1] * (len(nodes) - len(depth_targets)))

        for node, size_target, depth_target in zip(nodes, size_targets, depth_targets):
            node.target_size_bytes = max(0, size_target)
            if is_bootstrap_node(node):
                node.target_path_depth = len(Path(node.path.lstrip("/")).parts) - 1
            else:
                node.target_path_depth = max(1, depth_target)


def nested_directory_segments(index: int, depth: int) -> list[str]:
    if depth <= 0:
        return []
    return [
        f"{(index // (100 ** level)) % 100:02d}"
        for level in range(depth - 1, -1, -1)
    ]


def assign_paths(nodes_by_extension: dict[str, list[Node]]) -> None:
    for extension, nodes in nodes_by_extension.items():
        dir_name = DIRECTORY_BY_EXTENSION.get(extension, extension)
        prefix = PREFIX_BY_EXTENSION.get(extension, extension)
        for node in nodes:
            if is_bootstrap_node(node):
                continue
            nested_depth = max(0, node.target_path_depth - 1)
            nested_dirs = nested_directory_segments(node.index, nested_depth)
            dir_path = "/".join([dir_name] + nested_dirs)
            node.path = f"/{dir_path}/{prefix}_{node.index + 1:04d}.{extension}"


def assign_dag_ranks(nodes_by_extension: dict[str, list[Node]]) -> None:
    interleaved_offsets = {
        "collection": 2,
        "go": 1,
        "collectionproxy": 0,
    }
    interleaved_span = max(
        (len(nodes_by_extension.get(extension, [])) for extension in interleaved_offsets),
        default=0,
    )
    for extension, offset in interleaved_offsets.items():
        for node in nodes_by_extension.get(extension, []):
            node.dag_rank = 1_000_000 + ((interleaved_span - node.index) * 10) + offset

    base_rank_by_extension = {
        "render": 900_000,
        "gui": 800_000,
        "particlefx": 700_000,
        "script": 650_000,
        "gui_script": 640_000,
        "render_script": 630_000,
        "material": 620_000,
        "atlas": 610_000,
        "tilesource": 600_000,
        "font": 590_000,
        "png": 580_000,
        "lua": 570_000,
        "vp": 560_000,
        "fp": 550_000,
    }
    for extension, nodes in nodes_by_extension.items():
        if extension in interleaved_offsets:
            continue
        base_rank = base_rank_by_extension.get(extension, 100_000)
        for node in nodes:
            node.dag_rank = base_rank - node.index


def assign_topology(profile: dict, nodes_by_extension: dict[str, list[Node]]) -> None:
    topology = profile.get("topology", {})

    collection_nodes = nodes_by_extension.get("collection", [])
    for node, instance_count, subcollection_count in zip(
        collection_nodes,
        assign_metric(topology, "collection_instance_count", len(collection_nodes)),
        assign_metric(topology, "collection_subcollection_count", len(collection_nodes)),
    ):
        node.properties["instance_count"] = max(0, instance_count)
        node.properties["subcollection_count"] = max(0, subcollection_count)

    go_nodes = nodes_by_extension.get("go", [])
    for node, component_count in zip(
        go_nodes,
        assign_metric(topology, "go_component_count", len(go_nodes)),
    ):
        node.properties["component_count"] = max(0, component_count)

    gui_nodes = nodes_by_extension.get("gui", [])
    node_counts = assign_metric(topology, "gui_node_count", len(gui_nodes))
    parented_counts = assign_metric(topology, "gui_parented_node_count", len(gui_nodes))
    template_counts = assign_metric(topology, "gui_template_node_count", len(gui_nodes))
    max_depths = assign_metric(topology, "gui_max_node_depth", len(gui_nodes))
    for node, node_count, parented_count, template_count, max_depth in zip(
        gui_nodes,
        node_counts,
        parented_counts,
        template_counts,
        max_depths,
    ):
        node.properties["node_count"] = max(0, node_count)
        node.properties["parented_node_count"] = max(0, min(parented_count, node_count))
        node.properties["template_node_count"] = max(0, min(template_count, node_count))
        node.properties["max_node_depth"] = max(0, max_depth)

    particle_nodes = nodes_by_extension.get("particlefx", [])
    for node, emitter_count in zip(
        particle_nodes,
        assign_metric(topology, "particlefx_emitter_count", len(particle_nodes)),
    ):
        node.properties["emitter_count"] = max(1, emitter_count)

    render_nodes = nodes_by_extension.get("render", [])
    for node, material_slot_count in zip(
        render_nodes,
        assign_metric(topology, "render_material_slot_count", len(render_nodes)),
    ):
        node.properties["material_slot_count"] = max(1, material_slot_count)


def weighted_target_choice(rng: random.Random, nodes: list[Node]) -> Node:
    choice = weighted_target_choice_from_prefix(rng, nodes, len(nodes))
    if choice is None:
        raise ValueError("weighted_target_choice requires at least one node")
    return choice


def weighted_target_choice_from_prefix(rng: random.Random, nodes: list[Node], eligible_count: int) -> Node | None:
    if eligible_count <= 0:
        return None
    if eligible_count == 1:
        choice = nodes[0]
        choice.inbound_count += 1
        return choice
    hot_count = max(1, min(eligible_count, ceil(eligible_count ** 0.5)))
    if rng.random() < 0.8:
        choice = nodes[rng.randrange(hot_count)]
    else:
        choice = nodes[rng.randrange(eligible_count)]
    choice.inbound_count += 1
    return choice


def eligible_target_count(sorted_target_ranks: list[int], source_rank: int) -> int:
    return bisect_left(sorted_target_ranks, source_rank)


def build_target_pool_cache(
    source_nodes: list[Node],
    target_nodes: list[Node],
) -> tuple[list[Node], dict[int, int]]:
    sorted_target_nodes = sorted(target_nodes, key=lambda node: node.dag_rank)
    sorted_target_ranks = [node.dag_rank for node in sorted_target_nodes]
    eligible_count_by_source_index = {
        source_node.index: eligible_target_count(sorted_target_ranks, source_node.dag_rank)
        for source_node in source_nodes
    }
    return sorted_target_nodes, eligible_count_by_source_index


def choose_target_from_pool(
    rng: random.Random,
    target_nodes: list[Node],
    eligible_count: int,
) -> Node | None:
    return weighted_target_choice_from_prefix(rng, target_nodes, eligible_count)


def allocate_edges(
    seed: int,
    profile: dict,
    nodes_by_extension: dict[str, list[Node]],
    reporter: ProgressReporter | None = None,
) -> None:
    rng = random.Random(seed)
    references = profile.get("references", {}).get("by_source_type", {})

    for source_extension, target_counts in sorted(references.items()):
        source_nodes = nodes_by_extension.get(source_extension, [])
        if not source_nodes:
            continue

        total_edges = sum(target_counts.values())
        if total_edges > sum(node.outgoing_target for node in source_nodes):
            deficits = total_edges - sum(node.outgoing_target for node in source_nodes)
            for index in range(deficits):
                source_nodes[index % len(source_nodes)].outgoing_target += 1

        source_order = sorted(source_nodes, key=lambda node: (-node.outgoing_target, node.path))
        source_index = 0
        target_extensions = list(sorted(target_counts.items()))
        for target_index, (target_extension, count) in enumerate(target_extensions, start=1):
            target_nodes = nodes_by_extension.get(target_extension, [])
            if not target_nodes:
                continue
            sorted_target_nodes, eligible_count_by_source_index = build_target_pool_cache(source_nodes, target_nodes)
            progress_interval = max(1, min(1000, count // 100))
            for edge_index in range(count):
                if reporter is not None and edge_index % progress_interval == 0:
                    reporter.status(
                        f"allocating {source_extension}->{target_extension} {edge_index}/{count}"
                    )

                attempts = 0
                source_node = source_order[source_index % len(source_order)]
                while source_node.outgoing_target <= source_node.assigned_outgoing_count:
                    source_index += 1
                    attempts += 1
                    source_node = source_order[source_index % len(source_order)]
                    if attempts > len(source_order):
                        source_node.outgoing_target += 1
                        break
                target_node = choose_target_from_pool(
                    rng,
                    sorted_target_nodes,
                    eligible_count_by_source_index[source_node.index],
                )
                if target_node is None:
                    continue
                source_node.edges[target_extension].append(target_node.path)
                source_node.assigned_outgoing_count += 1
                source_index += 1

            if reporter is not None:
                reporter.status(
                    f"allocated {source_extension}->{target_extension} {target_index}/{len(target_extensions)}",
                    force=True,
                )


def add_unique_edge(source_node: Node, target_extension: str, target_node: Node) -> None:
    if source_node.path == target_node.path:
        return
    if source_node.dag_rank <= target_node.dag_rank:
        return
    edges = source_node.edges[target_extension]
    if target_node.path not in edges:
        edges.append(target_node.path)


def connect_targets_round_robin(
    source_nodes: list[Node],
    target_extension: str,
    target_nodes: list[Node],
    *,
    targets_per_source: int,
    skip_bootstrap_target: bool = False,
    skip_target_paths: set[str] | None = None,
) -> None:
    if not source_nodes or not target_nodes or targets_per_source <= 0:
        return
    source_count = len(source_nodes)
    for target_index, target_node in enumerate(target_nodes):
        if skip_bootstrap_target and is_bootstrap_node(target_node):
            continue
        if skip_target_paths is not None and target_node.path in skip_target_paths:
            continue
        start = (target_index // targets_per_source) % source_count
        for offset in range(source_count):
            source_node = source_nodes[(start + offset) % source_count]
            if source_node.dag_rank > target_node.dag_rank:
                add_unique_edge(source_node, target_extension, target_node)
                break


def connect_collection_tree(collection_nodes: list[Node]) -> None:
    for target_index, target_node in enumerate(collection_nodes[1:], start=1):
        parent_index = (target_index - 1) // COLLECTION_BRANCH_FACTOR
        add_unique_edge(collection_nodes[parent_index], "collection", target_node)


def connect_go_tree(collection_nodes: list[Node], go_nodes: list[Node]) -> None:
    if not collection_nodes:
        return
    for target_index, target_node in enumerate(go_nodes[1:], start=1):
        parent_index = min(len(collection_nodes) - 1, target_index // GO_PER_COLLECTION)
        add_unique_edge(collection_nodes[parent_index], "go", target_node)


def connect_lua_tree(script_roots: list[Node], lua_nodes: list[Node]) -> None:
    if not script_roots or not lua_nodes:
        return
    for target_index, target_node in enumerate(lua_nodes):
        root_index = min(len(script_roots) - 1, target_index // LUA_ATTACHMENTS_PER_SCRIPT)
        source_node = script_roots[root_index]
        if source_node.dag_rank <= target_node.dag_rank:
            for fallback in script_roots:
                if fallback.dag_rank > target_node.dag_rank:
                    source_node = fallback
                    break
        add_unique_edge(source_node, "lua", target_node)

    for target_index, target_node in enumerate(lua_nodes[1:], start=1):
        parent_index = (target_index - 1) // COLLECTION_BRANCH_FACTOR
        source_node = lua_nodes[parent_index]
        add_unique_edge(source_node, "lua", target_node)


def sanitize_collectionproxy_targets(nodes_by_extension: dict[str, list[Node]]) -> None:
    collection_nodes = nodes_by_extension.get("collection", [])
    proxy_nodes = nodes_by_extension.get("collectionproxy", [])
    fallback_targets = [node.path for node in collection_nodes if not is_bootstrap_node(node)]
    if not fallback_targets:
        return
    fallback_index = 0
    for proxy_node in proxy_nodes:
        collection_edges = proxy_node.edges.get("collection", [])
        if not collection_edges:
            proxy_node.edges["collection"] = [fallback_targets[fallback_index % len(fallback_targets)]]
            fallback_index += 1
            continue
        rewritten_edges = []
        changed = False
        for target_path in collection_edges:
            if target_path == "/main/main.collection":
                rewritten_edges.append(fallback_targets[fallback_index % len(fallback_targets)])
                fallback_index += 1
                changed = True
            else:
                rewritten_edges.append(target_path)
        if changed:
            deduped = []
            seen = set()
            for target_path in rewritten_edges:
                if target_path not in seen:
                    seen.add(target_path)
                    deduped.append(target_path)
            proxy_node.edges["collection"] = deduped


def promote_main_reachability(nodes_by_extension: dict[str, list[Node]]) -> None:
    collection_nodes = nodes_by_extension.get("collection", [])
    go_nodes = nodes_by_extension.get("go", [])
    proxy_nodes = nodes_by_extension.get("collectionproxy", [])
    script_nodes = nodes_by_extension.get("script", [])
    gui_nodes = nodes_by_extension.get("gui", [])
    gui_script_nodes = nodes_by_extension.get("gui_script", [])
    atlas_nodes = nodes_by_extension.get("atlas", [])
    png_nodes = nodes_by_extension.get("png", [])
    font_nodes = nodes_by_extension.get("font", [])
    material_nodes = nodes_by_extension.get("material", [])
    particle_nodes = nodes_by_extension.get("particlefx", [])
    tilesource_nodes = nodes_by_extension.get("tilesource", [])
    lua_nodes = nodes_by_extension.get("lua", [])
    fp_nodes = nodes_by_extension.get("fp", [])
    vp_nodes = nodes_by_extension.get("vp", [])
    render_nodes = nodes_by_extension.get("render", [])
    render_script_nodes = nodes_by_extension.get("render_script", [])
    problematic_proxy_paths = {
        node.path
        for node in proxy_nodes
        if "/main/main.collection" in node.edges.get("collection", [])
    }

    connect_collection_tree(collection_nodes)
    connect_go_tree(collection_nodes, go_nodes)
    connect_targets_round_robin(
        go_nodes,
        "collectionproxy",
        proxy_nodes,
        targets_per_source=PROXIES_PER_GO,
        skip_bootstrap_target=True,
        skip_target_paths=problematic_proxy_paths,
    )
    connect_targets_round_robin(go_nodes, "script", script_nodes, targets_per_source=GO_ASSETS_PER_KIND, skip_bootstrap_target=True)
    connect_targets_round_robin(go_nodes, "gui", gui_nodes, targets_per_source=GO_ASSETS_PER_KIND)
    connect_targets_round_robin(go_nodes, "atlas", atlas_nodes, targets_per_source=GO_ASSETS_PER_KIND)
    connect_targets_round_robin(go_nodes, "font", font_nodes, targets_per_source=GO_ASSETS_PER_KIND)
    connect_targets_round_robin(go_nodes, "material", material_nodes, targets_per_source=GO_ASSETS_PER_KIND)

    connect_targets_round_robin(gui_nodes, "gui_script", gui_script_nodes, targets_per_source=GUI_ASSETS_PER_KIND)
    connect_targets_round_robin(gui_nodes, "particlefx", particle_nodes, targets_per_source=GUI_ASSETS_PER_KIND)
    connect_targets_round_robin(gui_nodes, "atlas", atlas_nodes, targets_per_source=GUI_ASSETS_PER_KIND)
    connect_targets_round_robin(gui_nodes, "font", font_nodes, targets_per_source=GUI_ASSETS_PER_KIND)

    connect_targets_round_robin(atlas_nodes, "png", png_nodes, targets_per_source=ATLAS_IMAGES_PER_ATLAS)
    connect_targets_round_robin(tilesource_nodes, "png", png_nodes, targets_per_source=1)
    connect_targets_round_robin(particle_nodes, "tilesource", tilesource_nodes, targets_per_source=TILESOURCES_PER_PARTICLEFX)
    connect_targets_round_robin(particle_nodes, "atlas", atlas_nodes, targets_per_source=GUI_ASSETS_PER_KIND)
    connect_targets_round_robin(particle_nodes, "material", material_nodes, targets_per_source=GUI_ASSETS_PER_KIND)

    connect_targets_round_robin(material_nodes, "fp", fp_nodes, targets_per_source=1)
    connect_targets_round_robin(material_nodes, "vp", vp_nodes, targets_per_source=1)
    connect_targets_round_robin(render_nodes, "material", material_nodes, targets_per_source=GO_ASSETS_PER_KIND)
    connect_targets_round_robin(render_nodes, "render_script", render_script_nodes, targets_per_source=1)

    script_roots = script_nodes + gui_script_nodes + render_script_nodes
    connect_lua_tree(script_roots, lua_nodes)
    sanitize_collectionproxy_targets(nodes_by_extension)


def write_project(
    out_project_path: Path,
    profile: dict,
    nodes_by_extension: dict[str, list[Node]],
    seed: int,
    reporter: ProgressReporter,
) -> None:
    write_source_profile(out_project_path, profile)
    reporter.advance(detail="wrote source profile")
    copy_seed_assets(out_project_path)
    reporter.advance(detail="copied seed assets")
    write_game_project(out_project_path, profile)
    reporter.advance(detail="wrote game.project")
    write_shared_editor_settings(out_project_path)
    reporter.advance(detail="wrote project.shared_editor_settings")
    write_input_files(out_project_path)
    reporter.advance(detail="wrote input files")
    write_gitignore(out_project_path)
    reporter.advance(detail="wrote .gitignore")

    all_nodes = [node for nodes in nodes_by_extension.values() for node in nodes]
    for node in sorted(all_nodes, key=lambda node: node.path):
        write_node(out_project_path, node, nodes_by_extension, seed)
        reporter.advance(detail=node.path)


def write_source_profile(out_project_path: Path, profile: dict) -> None:
    payload = json.dumps(profile, indent=2, sort_keys=True) + "\n"
    target = out_project_path / SOURCE_PROFILE_FILENAME
    target.write_text(payload, encoding="utf-8")


def copy_seed_assets(out_project_path: Path) -> None:
    copy_file(SEED_TTF, out_project_path / "fonts" / "source_seed.ttf")


def copy_file(source: Path, target: Path) -> None:
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)


def write_text_resource(out_project_path: Path, resource_path: str, content: str) -> None:
    target = out_project_path / resource_path.lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(content, encoding="utf-8")


def nearest_png_dimension(target_dimension: int) -> int:
    return min(PNG_DIMENSION_BUCKETS, key=lambda candidate: abs(candidate - target_dimension))


def choose_png_dimensions(node: Node) -> tuple[int, int]:
    target_size = min(PNG_TARGET_BYTES_CEILING, max(PNG_TARGET_BYTES_FLOOR, node.target_size_bytes))
    target_dimension = int((target_size / 3) ** 0.5)
    width = nearest_png_dimension(max(PNG_MIN_DIMENSION, min(PNG_MAX_DIMENSION, target_dimension)))
    height_seed = max(PNG_MIN_DIMENSION, min(PNG_MAX_DIMENSION, int(round(width * (0.75 + ((node.index % 5) * 0.125))))))
    height = nearest_png_dimension(height_seed)
    return width, height


def png_chunk(chunk_type: bytes, data: bytes) -> bytes:
    crc = binascii.crc32(chunk_type)
    crc = binascii.crc32(data, crc) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + chunk_type + data + struct.pack(">I", crc)


def make_png_rows(width: int, height: int, variant: int) -> bytes:
    rows = bytearray()
    base_seed = (variant * 37) & 0xFF
    x_values = bytes(((x * 11 + base_seed) & 0xFF) for x in range(width))
    stripe_values = bytes((((x // 8) * 29 + variant * 17) & 0xFF) for x in range(width))
    checker_even = bytes(255 if ((x // 16) + variant) % 2 == 0 else 32 for x in range(width))
    checker_odd = bytes(32 if ((x // 16) + variant) % 2 == 0 else 255 for x in range(width))
    for y in range(height):
        rows.append(0)
        row_shift = (y * 7 + variant * 13) & 0xFF
        checker_values = checker_even if ((y // 16) + variant) % 2 == 0 else checker_odd
        for base, stripe, checker in zip(x_values, stripe_values, checker_values):
            rows.extend(((base + row_shift) & 0xFF, (stripe + (y * 19)) & 0xFF, checker, 255))
    return bytes(rows)


def png_template_bytes(width: int, height: int, variant: int) -> bytes:
    cache_key = (width, height, variant)
    cached = PNG_TEMPLATE_CACHE.get(cache_key)
    if cached is not None:
        return cached
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    template = b"".join(
        (
            b"\x89PNG\r\n\x1a\n",
            png_chunk(b"IHDR", header),
            png_chunk(b"IDAT", zlib.compress(make_png_rows(width, height, variant), level=1)),
            png_chunk(b"IEND", b""),
        )
    )
    PNG_TEMPLATE_CACHE[cache_key] = template
    return template


def synthetic_png_bytes(node: Node) -> bytes:
    width, height = choose_png_dimensions(node)
    variant = node.index % PNG_PATTERN_VARIANTS
    template = png_template_bytes(width, height, variant)
    marker = png_chunk(b"tEXt", f"SyntheticNode\x00{node.index}".encode("latin-1"))
    return template[:-12] + marker + template[-12:]


def ensure_png(out_project_path: Path, node: Node) -> None:
    target = out_project_path / node.path.lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(synthetic_png_bytes(node))


def pad_text_content(content: str, target_size_bytes: int) -> str:
    if target_size_bytes <= 0:
        return content
    current_size = len(content.encode("utf-8"))
    if current_size >= target_size_bytes:
        return content
    return content + (" " * (target_size_bytes - current_size))


def write_game_project(out_project_path: Path, profile: dict) -> None:
    dependency_count = profile.get("dependencies", {}).get("count", 0)
    dependency_urls = [
        SAFE_ASSET_PORTAL_LIBRARY_ZIPS[index % len(SAFE_ASSET_PORTAL_LIBRARY_ZIPS)]
        for index in range(max(0, dependency_count))
    ]
    dependencies = ",".join(
        dependency_urls
    )
    content = (
        "[project]\n"
        "title = Synthetic Project\n"
        f"dependencies = {dependencies}\n\n"
        "[bootstrap]\n"
        "main_collection = /main/main.collectionc\n"
        "render = /render/default.renderc\n\n"
        "[input]\n"
        "game_binding = /input/game.input_bindingc\n"
        "gamepads = /input/default.gamepadsc\n\n"
        "[display]\n"
        "width = 960\n"
        "height = 640\n\n"
        "[resource]\n"
        "uri = build/default\n"
        "max_resources = 200000\n\n"
        "[collection]\n"
        "max_count = 4096\n\n"
        "max_instances = 65534\n\n"
        "[collection_proxy]\n"
        "max_count = 4096\n\n"
        "[factory]\n"
        "max_count = 4096\n\n"
        "[sprite]\n"
        "max_count = 16384\n\n"
        "[gui]\n"
        "max_count = 2048\n"
        "max_particlefx_count = 4096\n"
        "max_animation_count = 8192\n"
        "max_particle_count = 8192\n\n"
        "[label]\n"
        "max_count = 8192\n\n"
        "[particlefx]\n"
        "max_count = 4096\n\n"
        "[sound]\n"
        "max_count = 4096\n\n"
        "[spine]\n"
        "max_count = 2048\n\n"
        "[model]\n"
        "max_count = 2048\n"
    )
    write_text_resource(out_project_path, "/game.project", content)


def write_shared_editor_settings(out_project_path: Path) -> None:
    write_text_resource(
        out_project_path,
        "/project.shared_editor_settings",
        "[performance]\ncache_capacity = 200000\n",
    )


def write_input_files(out_project_path: Path) -> None:
    write_text_resource(
        out_project_path,
        "/input/game.input_binding",
        "key_trigger {\n  input: KEY_SPACE\n  action: \"ok\"\n}\n",
    )
    write_text_resource(out_project_path, "/input/default.gamepads", "")


def write_gitignore(out_project_path: Path) -> None:
    write_text_resource(
        out_project_path,
        "/.gitignore",
        "/.internal\n/.editor_settings\n/build\n",
    )


def write_node(out_project_path: Path, node: Node, nodes_by_extension: dict[str, list[Node]], seed: int) -> None:
    if node.extension == "png":
        ensure_png(out_project_path, node)
        return
    if node.extension == "vp":
        write_text_resource(out_project_path, node.path, pad_text_content(default_vertex_program(), node.target_size_bytes))
        return
    if node.extension == "fp":
        write_text_resource(out_project_path, node.path, pad_text_content(default_fragment_program(), node.target_size_bytes))
        return
    if node.extension == "material":
        write_text_resource(out_project_path, node.path, pad_text_content(render_material(node), node.target_size_bytes))
        return
    if node.extension == "font":
        write_text_resource(out_project_path, node.path, pad_text_content(render_font(node), node.target_size_bytes))
        return
    if node.extension in {"lua", "script", "gui_script", "render_script"}:
        write_text_resource(out_project_path, node.path, pad_text_content(render_lua(node), node.target_size_bytes))
        return
    if node.extension == "atlas":
        write_text_resource(out_project_path, node.path, pad_text_content(render_atlas(node), node.target_size_bytes))
        return
    if node.extension == "tilesource":
        write_text_resource(out_project_path, node.path, pad_text_content(render_tilesource(node), node.target_size_bytes))
        return
    if node.extension == "particlefx":
        write_text_resource(out_project_path, node.path, pad_text_content(render_particlefx(node), node.target_size_bytes))
        return
    if node.extension == "gui":
        write_text_resource(out_project_path, node.path, pad_text_content(render_gui(node, nodes_by_extension, seed), node.target_size_bytes))
        return
    if node.extension == "go":
        write_text_resource(out_project_path, node.path, pad_text_content(render_go(node), node.target_size_bytes))
        return
    if node.extension == "collectionproxy":
        write_text_resource(out_project_path, node.path, pad_text_content(render_collectionproxy(node, nodes_by_extension), node.target_size_bytes))
        return
    if node.extension == "collection":
        write_text_resource(out_project_path, node.path, pad_text_content(render_collection(node, nodes_by_extension), node.target_size_bytes))
        return
    if node.extension == "render":
        write_text_resource(out_project_path, node.path, pad_text_content(render_render(node), node.target_size_bytes))
        return
    write_text_resource(out_project_path, node.path, pad_text_content("", node.target_size_bytes))


def message_to_text(message) -> str:
    return text_format.MessageToString(message, as_utf8=True)


def make_embedded_sprite(tile_set: str, material: str) -> str:
    message = EMBEDDED_PROTO_BY_TYPE["sprite"]()
    message.tile_set = tile_set
    message.default_animation = "anim"
    message.material = material
    if hasattr(message, "blend_mode"):
        message.blend_mode = message.BLEND_MODE_ALPHA
    return message_to_text(message)


def make_embedded_label(font: str) -> str:
    message = EMBEDDED_PROTO_BY_TYPE["label"]()
    message.size.x = 128.0
    message.size.y = 32.0
    message.size.z = 0.0
    message.size.w = 0.0
    message.scale.x = 1.0
    message.scale.y = 1.0
    message.scale.z = 1.0
    message.scale.w = 0.0
    message.color.x = 1.0
    message.color.y = 1.0
    message.color.z = 1.0
    message.color.w = 1.0
    message.outline.x = 0.0
    message.outline.y = 0.0
    message.outline.z = 0.0
    message.outline.w = 1.0
    message.shadow.x = 0.0
    message.shadow.y = 0.0
    message.shadow.z = 0.0
    message.shadow.w = 1.0
    message.leading = 1.0
    message.tracking = 0.0
    if hasattr(message, "pivot"):
        message.pivot = message.PIVOT_CENTER
    if hasattr(message, "blend_mode"):
        message.blend_mode = message.BLEND_MODE_ALPHA
    message.line_break = False
    message.text = "synthetic"
    message.font = font
    message.material = "/builtins/fonts/label.material"
    return message_to_text(message)


def make_embedded_collectionproxy(collection: str) -> str:
    message = EMBEDDED_PROTO_BY_TYPE["collectionproxy"]()
    message.collection = collection
    message.exclude = False
    return message_to_text(message)


def make_embedded_factory(prototype: str) -> str:
    message = EMBEDDED_PROTO_BY_TYPE["factory"]()
    message.prototype = prototype
    message.load_dynamically = True
    if hasattr(message, "dynamic_prototype"):
        message.dynamic_prototype = True
    return message_to_text(message)


def first_edge(node: Node, extension: str, fallback: str) -> str:
    return node.edges.get(extension, [fallback])[0]


def unique_edges(node: Node, extension: str) -> list[str]:
    seen = set()
    result = []
    for path in node.edges.get(extension, []):
        if path not in seen:
            seen.add(path)
            result.append(path)
    return result


def default_vertex_program() -> str:
    return (
        "#version 140\n\n"
        "in highp vec4 position;\n"
        "in mediump vec2 texcoord0;\n\n"
        "out mediump vec2 var_texcoord0;\n\n"
        "uniform vs_uniforms\n"
        "{\n"
        "    highp mat4 view_proj;\n"
        "};\n\n"
        "void main()\n"
        "{\n"
        "    gl_Position = view_proj * vec4(position.xyz, 1.0);\n"
        "    var_texcoord0 = texcoord0;\n"
        "}\n"
    )


def default_fragment_program() -> str:
    return (
        "#version 140\n\n"
        "in mediump vec2 var_texcoord0;\n\n"
        "out vec4 out_fragColor;\n\n"
        "uniform mediump sampler2D texture_sampler;\n"
        "uniform fs_uniforms\n"
        "{\n"
        "    mediump vec4 tint;\n"
        "};\n\n"
        "void main()\n"
        "{\n"
        "    mediump vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);\n"
        "    out_fragColor = texture(texture_sampler, var_texcoord0.xy) * tint_pm;\n"
        "}\n"
    )


def render_material(node: Node) -> str:
    vp = first_edge(node, "vp", "/builtins/materials/sprite.vp")
    fp = first_edge(node, "fp", "/builtins/materials/sprite.fp")
    return (
        'name: "SyntheticMaterial"\n'
        'tags: "tile"\n'
        "vertex_space: VERTEX_SPACE_WORLD\n"
        f'vertex_program: "{vp}"\n'
        f'fragment_program: "{fp}"\n'
        'vertex_constants {\n  name: "view_proj"\n  type: CONSTANT_TYPE_VIEWPROJ\n}\n'
        'fragment_constants {\n  name: "tint"\n  type: CONSTANT_TYPE_USER\n  value {\n    x: 1.0\n    y: 1.0\n    z: 1.0\n    w: 1.0\n  }\n}\n'
        'samplers {\n  name: "texture_sampler"\n  wrap_u: WRAP_MODE_CLAMP_TO_EDGE\n  wrap_v: WRAP_MODE_CLAMP_TO_EDGE\n  filter_min: FILTER_MODE_MIN_DEFAULT\n  filter_mag: FILTER_MODE_MAG_DEFAULT\n  max_anisotropy: 1.0\n}\n'
        "max_page_count: 0\n"
    )


def render_font(node: Node) -> str:
    return (
        'font: "/fonts/source_seed.ttf"\n'
        'material: "/builtins/fonts/font-df.material"\n'
        "size: 20\n"
        "antialias: 1\n"
        "alpha: 1.0\n"
        "outline_alpha: 0.0\n"
        "outline_width: 0.0\n"
        "shadow_alpha: 0.0\n"
        "shadow_blur: 0\n"
        "shadow_x: 0.0\n"
        "shadow_y: 0.0\n"
    )


def render_lua(node: Node) -> str:
    requires = []
    for module_path in unique_edges(node, "lua"):
        module_name = module_path.lstrip("/").removesuffix(".lua").replace("/", ".")
        requires.append(f"require('{module_name}')")
    body = "\n".join(requires)

    if node.extension == "render_script":
        return (
            body + ("\n\n" if body else "")
            + "function init(self)\n"
            + "    self.tile_pred = render.predicate({\"tile\"})\n"
            + "    self.gui_pred = render.predicate({\"gui\"})\n"
            + "    self.text_pred = render.predicate({\"text\"})\n"
            + "end\n\n"
            + "function update(self)\n"
            + "    render.clear({[render.BUFFER_COLOR_BIT] = vmath.vector4(0, 0, 0, 1), [render.BUFFER_DEPTH_BIT] = 1})\n"
            + "    render.set_viewport(0, 0, render.get_window_width(), render.get_window_height())\n"
            + "    render.set_view(vmath.matrix4())\n"
            + "    render.set_projection(vmath.matrix4_orthographic(0, render.get_window_width(), 0, render.get_window_height(), -1, 1))\n"
            + "    render.set_depth_mask(false)\n"
            + "    render.disable_state(graphics.STATE_DEPTH_TEST)\n"
            + "    render.enable_state(graphics.STATE_BLEND)\n"
            + "    render.set_blend_func(graphics.BLEND_FACTOR_SRC_ALPHA, graphics.BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)\n"
            + "    render.disable_state(graphics.STATE_CULL_FACE)\n"
            + "    render.draw(self.tile_pred)\n"
            + "    render.set_projection(vmath.matrix4_orthographic(0, render.get_window_width(), render.get_window_height(), 0, -1, 1))\n"
            + "    render.draw(self.gui_pred)\n"
            + "    render.draw(self.text_pred)\n"
            + "end\n\n"
            + "function on_message(self, message_id, message)\n"
            + "end\n"
        )
    if node.extension == "gui_script":
        return (
            body + ("\n\n" if body else "")
            + "function init(self)\n"
            + "end\n"
        )
    return (
        body + ("\n\n" if body else "")
        + "function init(self)\n"
        + "end\n"
    )


def render_atlas(node: Node) -> str:
    pngs = unique_edges(node, "png")
    if not pngs:
        pngs = ["/images/image_0001.png"]
    lines = []
    for png in pngs:
        lines.append("images {")
        lines.append(f'  image: "{png}"')
        lines.append("}")
    lines.append("animations {")
    lines.append('  id: "anim"')
    for png in pngs[: min(8, len(pngs))]:
        lines.append("  images {")
        lines.append(f'    image: "{png}"')
        lines.append("  }")
    lines.append("}")
    return "\n".join(lines) + "\n"


def render_tilesource(node: Node) -> str:
    png = first_edge(node, "png", "/images/image_0001.png")
    return (
        f'image: "{png}"\n'
        "tile_width: 16\n"
        "tile_height: 16\n"
        "tile_margin: 0\n"
        "tile_spacing: 0\n"
        f'collision: "{png}"\n'
        'material_tag: "tile"\n'
    )


def render_particlefx(node: Node) -> str:
    emitter_count = max(1, node.properties.get("emitter_count", 1))
    tile_targets = unique_edges(node, "atlas") + unique_edges(node, "tilesource")
    if not tile_targets:
        tile_targets = ["/images/image_0001.png"]
    material_targets = unique_edges(node, "material") or ["/builtins/materials/sprite.material"]
    chunks = []
    for index in range(emitter_count):
        tile_target = tile_targets[index % len(tile_targets)]
        material_target = material_targets[index % len(material_targets)]
        chunks.extend(
            [
                "emitters {",
                f'  id: "emitter_{index + 1}"',
                "  mode: PLAY_MODE_ONCE",
                "  duration: 1.0",
                "  space: EMISSION_SPACE_WORLD",
                f'  tile_source: "{tile_target}"',
                '  animation: "anim"',
                f'  material: "{material_target}"',
                "  blend_mode: BLEND_MODE_ALPHA",
                "  max_particle_count: 16",
                "  type: EMITTER_TYPE_CIRCLE",
                "  properties {",
                "    key: EMITTER_KEY_SPAWN_RATE",
                "    points { x: 0.0 y: 4.0 t_x: 1.0 t_y: 0.0 }",
                "  }",
                "}",
            ]
        )
    return "\n".join(chunks) + "\n"


def gui_node_block(node_id: str, *, parent: str | None, template: str | None, font_name: str, texture_name: str, material: str | None, node_type: str) -> list[str]:
    lines = [
        "nodes {",
        "  position { x: 0.0 y: 0.0 z: 0.0 w: 1.0 }",
        "  rotation { x: 0.0 y: 0.0 z: 0.0 w: 1.0 }",
        "  scale { x: 1.0 y: 1.0 z: 1.0 w: 1.0 }",
        "  size { x: 64.0 y: 32.0 z: 0.0 w: 1.0 }",
        "  color { x: 1.0 y: 1.0 z: 1.0 w: 1.0 }",
        f"  type: {node_type}",
        "  blend_mode: BLEND_MODE_ALPHA",
        f'  id: "{node_id}"',
        "  xanchor: XANCHOR_NONE",
        "  yanchor: YANCHOR_NONE",
        "  pivot: PIVOT_CENTER",
        '  layer: ""',
        "  inherit_alpha: true",
        "  alpha: 1.0",
        "  template_node_child: false",
    ]
    if node_type == "TYPE_TEXT":
        lines.append('  text: "synthetic"')
        lines.append(f'  font: "{font_name}"')
    else:
        lines.append(f'  texture: "{texture_name}/anim"')
    if parent is not None:
        lines.append(f'  parent: "{parent}"')
    if template is not None:
        lines.append(f'  template: "{template}"')
    if material is not None:
        lines.append(f'  material: "{material}"')
    lines.append("}")
    return lines


def render_gui(node: Node, nodes_by_extension: dict[str, list[Node]], seed: int) -> str:
    rng = random.Random(seed + node.index)
    script = first_edge(node, "gui_script", "")
    fonts = unique_edges(node, "font")
    textures = unique_edges(node, "atlas") + unique_edges(node, "png")
    templates = unique_edges(node, "gui")
    materials: list[str] = []
    node_count = max(0, node.properties.get("node_count", 0))
    parented_count = min(node_count, node.properties.get("parented_node_count", 0))
    template_count = min(node_count, node.properties.get("template_node_count", 0))
    max_depth = max(0, node.properties.get("max_node_depth", 0))
    if not fonts and not textures and not templates:
        node_count = 0
        parented_count = 0
        template_count = 0

    lines = [f'script: "{script}"', f"max_nodes: {max(512, node_count)}"]
    for index, font in enumerate(fonts):
        lines.append("fonts {")
        lines.append(f'  name: "font_{index + 1}"')
        lines.append(f'  font: "{font}"')
        lines.append("}")
    for index, texture in enumerate(textures):
        lines.append("textures {")
        lines.append(f'  name: "texture_{index + 1}"')
        lines.append(f'  texture: "{texture}"')
        lines.append("}")
    lines.append("background_color { x: 0.0 y: 0.0 z: 0.0 w: 1.0 }")

    parent_candidates = []
    for index in range(node_count):
        node_id = f"node_{index + 1}"
        parent = None
        if index < parented_count and parent_candidates:
            depth_limit = max(1, max_depth)
            eligible = parent_candidates[: max(1, min(len(parent_candidates), depth_limit))]
            parent = rng.choice(eligible)
        template = templates[index % len(templates)] if index < template_count and templates else None
        font_name = f"font_{(index % len(fonts)) + 1}" if fonts else ""
        texture_name = f"texture_{(index % len(textures)) + 1}" if textures else ""
        material = None
        if fonts and textures:
            node_type = GUI_NODE_TYPES[index % len(GUI_NODE_TYPES)]
        elif fonts:
            node_type = "TYPE_TEXT"
        elif textures:
            node_type = "TYPE_BOX"
        else:
            node_type = "TYPE_TEXT"
        lines.extend(gui_node_block(node_id, parent=parent, template=template, font_name=font_name, texture_name=texture_name, material=material, node_type=node_type))
        parent_candidates.append(node_id)

    lines.append("adjust_reference: ADJUST_REFERENCE_PARENT")
    return "\n".join(lines) + "\n"


def render_go(node: Node) -> str:
    lines: list[str] = []

    for index, script_path in enumerate(unique_edges(node, "script")):
        lines.extend(
            [
                "components {",
                f'  id: "script_{index + 1}"',
                f'  component: "{script_path}"',
                "}",
            ]
        )

    for index, gui_path in enumerate(unique_edges(node, "gui")):
        lines.extend(
            [
                "components {",
                f'  id: "gui_{index + 1}"',
                f'  component: "{gui_path}"',
                "}",
            ]
        )

    for index, proxy_path in enumerate(unique_edges(node, "collectionproxy")):
        lines.extend(
            [
                "components {",
                f'  id: "proxy_{index + 1}"',
                f'  component: "{proxy_path}"',
                "}",
            ]
        )

    atlas_targets = unique_edges(node, "atlas")
    material_targets = unique_edges(node, "material") or ["/builtins/materials/sprite.material"]
    for index, atlas_path in enumerate(atlas_targets):
        material_path = material_targets[index % len(material_targets)]
        lines.extend(
            [
                "embedded_components {",
                f'  id: "sprite_{index + 1}"',
                '  type: "sprite"',
                f'  data: {json.dumps(make_embedded_sprite(atlas_path, material_path))}',
                "}",
            ]
        )

    for index, font_path in enumerate(unique_edges(node, "font")):
        lines.extend(
            [
                "embedded_components {",
                f'  id: "label_{index + 1}"',
                '  type: "label"',
                f'  data: {json.dumps(make_embedded_label(font_path))}',
                "}",
            ]
        )

    for index, collection_path in enumerate(unique_edges(node, "collection")):
        lines.extend(
            [
                "embedded_components {",
                f'  id: "collection_proxy_{index + 1}"',
                '  type: "collectionproxy"',
                f'  data: {json.dumps(make_embedded_collectionproxy(collection_path))}',
                "}",
            ]
        )

    for index, go_path in enumerate(unique_edges(node, "go")):
        lines.extend(
            [
                "embedded_components {",
                f'  id: "factory_{index + 1}"',
                '  type: "factory"',
                f'  data: {json.dumps(make_embedded_factory(go_path))}',
                "}",
            ]
        )

    if not lines and node.path == "/main/main.go":
        lines.extend(
            [
                "components {",
                '  id: "script"',
                '  component: "/main/main.script"',
                "}",
            ]
        )
    return "\n".join(lines) + "\n"


def render_collectionproxy(node: Node, nodes_by_extension: dict[str, list[Node]]) -> str:
    fallback = nodes_by_extension.get("collection", [Node("collection", 0, "/main/main.collection")])[0].path
    collection_path = first_edge(node, "collection", fallback)
    return f'collection: "{collection_path}"\nexclude: false\n'


def render_collection(node: Node, nodes_by_extension: dict[str, list[Node]]) -> str:
    lines = [f'name: "{Path(node.path).stem}"']
    go_targets = unique_edges(node, "go")
    collection_targets = [path for path in unique_edges(node, "collection") if path != node.path]

    if node.path == "/main/main.collection":
        if "/main/main.go" not in go_targets:
            go_targets = ["/main/main.go"] + go_targets

    instance_count = len(go_targets)
    subcollection_count = len(collection_targets)

    for index in range(instance_count):
        prototype = go_targets[index % len(go_targets)]
        lines.extend(
            [
                "instances {",
                f'  id: "go_{index + 1}"',
                f'  prototype: "{prototype}"',
                "  position { x: 0.0 y: 0.0 z: 0.0 }",
                "  rotation { x: 0.0 y: 0.0 z: 0.0 w: 1.0 }",
                "}",
            ]
        )

    for index in range(subcollection_count):
        collection = collection_targets[index % len(collection_targets)] if collection_targets else node.path
        lines.extend(
            [
                "collection_instances {",
                f'  id: "collection_{index + 1}"',
                f'  collection: "{collection}"',
                "  position { x: 0.0 y: 0.0 z: 0.0 }",
                "  rotation { x: 0.0 y: 0.0 z: 0.0 w: 1.0 }",
                "}",
            ]
        )
    return "\n".join(lines) + "\n"


def render_render(node: Node) -> str:
    script = first_edge(node, "render_script", "/render/default.render_script")
    materials = unique_edges(node, "material")
    if not materials:
        materials = ["/builtins/materials/sprite.material"]
    lines = [f'script: "{script}"']
    slot_count = max(1, node.properties.get("material_slot_count", len(materials)))
    for index in range(slot_count):
        material = materials[index % len(materials)]
        lines.append(f'materials {{ name: "slot_{index + 1}" material: "{material}" }}')
    return "\n".join(lines) + "\n"
