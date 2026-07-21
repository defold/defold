from __future__ import annotations

from collections import Counter
from datetime import datetime, timezone
import os
from pathlib import Path
import re

from project_synth.progress import ProgressReporter
from project_synth.proto import EMBEDDED_PROTO_BY_TYPE
from project_synth.proto import is_resource_field
from project_synth.proto import parse_path
from project_synth.proto import parse_text_proto
from project_synth.schema import PROFILE_RELEVANT_EXTENSIONS
from project_synth.schema import PROFILE_RELEVANT_TOPOLOGY_KEYS
from project_synth.schema import SKIPPED_DIR_NAMES
from project_synth.schema import SPECIAL_FILENAMES


URL_PATTERN = re.compile(r"https?://[^\s,]+")
GO_SOURCE_PATTERN = re.compile(r"^\s*package\s+[A-Za-z_]\w*", re.MULTILINE)
LUA_REQUIRE_PATTERN = re.compile(r'(?:^|[^\w])(?:_G\.)?require\s*(?:\(\s*["\']([^"\']+)["\']\s*\)|["\']([^"\']+)["\'])')


def histogram_add(counter_map: dict[str, Counter[str]], key: str, value: int) -> None:
    counter_map.setdefault(key, Counter())[str(value)] += 1


def nested_counter_increment(counter_map: dict[str, Counter[str]], outer_key: str, inner_key: str, amount: int = 1) -> None:
    counter_map.setdefault(outer_key, Counter())[inner_key] += amount


def normalize_resource_path(value: str) -> str:
    if not value:
        return ""
    if value.startswith("/"):
        return value
    return "/" + value


def collection_node_depth_histogram(message) -> Counter[str]:
    parents = {}
    for node in message.nodes:
        if node.id:
            parents[node.id] = node.parent
    histogram = Counter()
    for node in message.nodes:
        if not node.id:
            continue
        depth = 0
        current = node.parent
        guard = set()
        while current:
            if current in guard:
                break
            guard.add(current)
            depth += 1
            current = parents.get(current, "")
        histogram[str(depth)] += 1
    return histogram


def dynamic_bucket_ranges(values: list[int]) -> list[tuple[int, int]]:
    if not values:
        return [(0, 0)]
    max_value = max(values)
    ranges = []
    exact_limit = min(8, max_value)
    for value in range(0, exact_limit + 1):
        ranges.append((value, value))
    if max_value <= 8:
        return ranges
    low = 9
    high = 16
    while low <= max_value:
        ranges.append((low, min(high, max_value)))
        low = high + 1
        high = (high * 2)
    return ranges


def bucket_label(start: int, end: int) -> str:
    return f"{start}-{end}"


def bucketize_exact_histogram(histogram: Counter[str]) -> dict:
    exact_items = sorted(((int(key), count) for key, count in histogram.items()), key=lambda item: item[0])
    ranges = dynamic_bucket_ranges([value for value, _ in exact_items])
    bucketed = Counter()
    for value, count in exact_items:
        for start, end in ranges:
            if start <= value <= end:
                bucketed[bucket_label(start, end)] += count
                break
    return dict(bucketed)


def bucketize_histogram_map(histogram_map: dict[str, Counter[str]]) -> dict[str, dict[str, int]]:
    return {
        name: bucketize_exact_histogram(histogram)
        for name, histogram in sorted(histogram_map.items())
    }


def filter_reference_map(edge_counts: dict[str, Counter[str]]) -> dict[str, dict[str, int]]:
    filtered = {}
    for source_extension, targets in sorted(edge_counts.items()):
        if source_extension not in PROFILE_RELEVANT_EXTENSIONS:
            continue
        relevant_targets = {
            target_extension: count
            for target_extension, count in sorted(targets.items())
            if target_extension in PROFILE_RELEVANT_EXTENSIONS
        }
        if relevant_targets:
            filtered[source_extension] = relevant_targets
    return filtered


def filter_histogram_map(histogram_map: dict[str, Counter[str]]) -> dict[str, Counter[str]]:
    return {
        key: histogram
        for key, histogram in sorted(histogram_map.items())
        if key in PROFILE_RELEVANT_EXTENSIONS
    }


def resource_extension(path: Path) -> str:
    suffix = path.suffix.lstrip(".")
    if suffix == "go":
        text = path.read_text(encoding="utf-8", errors="ignore")[:512]
        if GO_SOURCE_PATTERN.search(text):
            return ""
    if suffix:
        return suffix
    if path.name in SPECIAL_FILENAMES:
        return path.name
    return ""


def resource_extension_from_string(path: str) -> str:
    path_obj = Path(path)
    suffix = path_obj.suffix.lstrip(".")
    if suffix:
        return suffix
    if path_obj.name in SPECIAL_FILENAMES:
        return path_obj.name
    return ""


def count_dependencies(game_project_path: Path) -> int:
    if not game_project_path.is_file():
        return 0
    return len(URL_PATTERN.findall(game_project_path.read_text(encoding="utf-8", errors="ignore")))


def strip_lua_comments(text: str) -> str:
    text = re.sub(r"--\[\[.*?\]\]", "", text, flags=re.DOTALL)
    text = re.sub(r"--[^\n]*", "", text)
    return text


def extract_lua_requires(text: str) -> list[str]:
    stripped = strip_lua_comments(text)
    modules = []
    for match in LUA_REQUIRE_PATTERN.finditer(stripped):
        module_name = match.group(1) or match.group(2)
        if module_name:
            modules.append(module_name)
    return modules


def lua_module_to_resource_path(module_name: str) -> str:
    normalized = module_name.replace(".", "/")
    if normalized.startswith("/"):
        return normalized + ("" if normalized.endswith(".lua") else ".lua")
    return "/" + normalized + ("" if normalized.endswith(".lua") else ".lua")


def iter_resource_strings(message) -> list[str]:
    from google.protobuf.descriptor import FieldDescriptor

    resources = []
    descriptor = message.DESCRIPTOR
    for field in descriptor.fields:
        value = getattr(message, field.name)
        if field.type == FieldDescriptor.TYPE_MESSAGE:
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for item in value:
                    resources.extend(iter_resource_strings(item))
            else:
                if message.HasField(field.name):
                    resources.extend(iter_resource_strings(value))
        elif is_resource_field(field):
            if field.label == FieldDescriptor.LABEL_REPEATED:
                for item in value:
                    normalized = normalize_resource_path(item)
                    if normalized:
                        resources.append(normalized)
            else:
                normalized = normalize_resource_path(value)
                if normalized:
                    resources.append(normalized)
    return resources


def embedded_message_resources(message, source_extension: str) -> tuple[list[str], dict[str, int]]:
    resources = []
    topology = {}
    extra_samples = []
    if source_extension == "go":
        topology["component_count"] = len(message.components) + len(message.embedded_components)
        for embedded in message.embedded_components:
            embedded_cls = EMBEDDED_PROTO_BY_TYPE.get(embedded.type)
            if embedded_cls is None or not embedded.data:
                continue
            try:
                embedded_message = parse_text_proto(embedded.data, embedded_cls)
            except Exception:
                continue
            resources.extend(iter_resource_strings(embedded_message))
    elif source_extension == "collection":
        topology["instance_count"] = len(message.instances) + len(message.embedded_instances)
        topology["collection_instance_count"] = len(message.collection_instances)
        for embedded in message.embedded_instances:
            if not embedded.data:
                continue
            try:
                embedded_message = parse_text_proto(embedded.data, EMBEDDED_PROTO_BY_TYPE["go"])
            except Exception:
                continue
            embedded_resources, embedded_topology, _ = embedded_message_resources(embedded_message, "go")
            extra_samples.append(
                {
                    "source_extension": "go",
                    "resource_paths": embedded_resources,
                    "topology": embedded_topology,
                }
            )
    elif source_extension == "gui":
        topology["node_count"] = len(message.nodes)
        topology["parented_node_count"] = sum(1 for node in message.nodes if node.parent)
        topology["template_node_count"] = sum(1 for node in message.nodes if getattr(node, "template", ""))
        topology["max_node_depth"] = max((int(depth) for depth in collection_node_depth_histogram(message).keys()), default=0)
        topology["node_depth_histogram"] = collection_node_depth_histogram(message)
    elif source_extension == "render":
        topology["material_slot_count"] = len(message.materials)
    elif source_extension == "particlefx":
        topology["emitter_count"] = len(message.emitters)
    return resources, topology, extra_samples


def apply_resource_paths(source_extension: str, resource_paths: list[str], edge_counts: dict[str, Counter[str]], outgoing_histogram: dict[str, Counter[str]]) -> None:
    if not resource_paths:
        return
    histogram_add(outgoing_histogram, source_extension, len(resource_paths))
    for resource_path in resource_paths:
        target_extension = resource_extension_from_string(resource_path)
        if not target_extension:
            continue
        nested_counter_increment(edge_counts, source_extension, target_extension)


def apply_topology(source_extension: str, topology: dict[str, int], topology_histograms: dict[str, Counter[str]]) -> None:
    if source_extension == "collection":
        histogram_add(topology_histograms, "collection_instance_count", topology.get("instance_count", 0))
        histogram_add(topology_histograms, "collection_subcollection_count", topology.get("collection_instance_count", 0))
    elif source_extension == "go":
        histogram_add(topology_histograms, "go_component_count", topology.get("component_count", 0))
    elif source_extension == "gui":
        histogram_add(topology_histograms, "gui_node_count", topology.get("node_count", 0))
        histogram_add(topology_histograms, "gui_parented_node_count", topology.get("parented_node_count", 0))
        histogram_add(topology_histograms, "gui_template_node_count", topology.get("template_node_count", 0))
        histogram_add(topology_histograms, "gui_max_node_depth", topology.get("max_node_depth", 0))
        for depth, count in topology.get("node_depth_histogram", Counter()).items():
            topology_histograms.setdefault("gui_node_depth", Counter())[depth] += count
    elif source_extension in {"lua", "script", "gui_script", "render_script"}:
        histogram_add(topology_histograms, "lua_require_count", topology.get("lua_require_count", 0))
        histogram_add(topology_histograms, "lua_require_resolved_count", topology.get("lua_require_resolved_count", 0))
    elif source_extension == "particlefx":
        histogram_add(topology_histograms, "particlefx_emitter_count", topology.get("emitter_count", 0))
    elif source_extension == "render":
        histogram_add(topology_histograms, "render_material_slot_count", topology.get("material_slot_count", 0))


def iter_project_files(project_path: Path):
    for current_root, dir_names, file_names in os.walk(project_path):
        dir_names[:] = [
            dir_name
            for dir_name in dir_names
            if dir_name not in SKIPPED_DIR_NAMES and not dir_name.startswith(".")
        ]
        current_root_path = Path(current_root)
        for file_name in file_names:
            yield current_root_path / file_name


def project_resource_path(project_path: Path, file_path: Path) -> str:
    return "/" + file_path.relative_to(project_path).as_posix()


def project_resource_depth(project_path: Path, file_path: Path) -> int:
    return len(file_path.relative_to(project_path).parts) - 1


def profile_project(project_path: Path) -> dict:
    project_path = project_path.resolve()
    extension_counts: Counter[str] = Counter()
    parsed_counts: Counter[str] = Counter()
    parse_failures: Counter[str] = Counter()
    edge_counts: dict[str, Counter[str]] = {}
    outgoing_histogram: dict[str, Counter[str]] = {}
    topology_histograms: dict[str, Counter[str]] = {}
    file_size_histograms: dict[str, Counter[str]] = {}
    path_depth_histograms: dict[str, Counter[str]] = {}

    all_files = list(iter_project_files(project_path))
    total_scan_files = len(all_files)
    lua_resource_paths = {
        project_resource_path(project_path, file_path)
        for file_path in all_files
        if resource_extension(file_path) in {"lua", "script", "gui_script", "render_script"}
    }
    reporter = ProgressReporter(total_scan_files, phase="parsing")
    parsed_file_count = 0

    for file_path in all_files:
        file_size = file_path.stat().st_size
        extension = resource_extension(file_path)
        parsed = False
        if extension:
            extension_counts[extension] += 1
            histogram_add(file_size_histograms, extension, file_size)
            histogram_add(path_depth_histograms, extension, project_resource_depth(project_path, file_path))

        resource_paths = []
        topology = {}
        if file_path.suffix == ".lua" or file_path.suffix in {".script", ".gui_script", ".render_script"}:
            text = file_path.read_text(encoding="utf-8", errors="ignore")
            modules = extract_lua_requires(text)
            resource_paths = [lua_module_to_resource_path(module_name) for module_name in modules]
            topology["lua_require_count"] = len(modules)
            topology["lua_require_resolved_count"] = sum(1 for resource_path in resource_paths if resource_path in lua_resource_paths)
            parsed = True
            parsed_counts[extension or file_path.suffix.lstrip(".")] += 1
        else:
            message = None
            try:
                message = parse_path(file_path)
            except Exception:
                message = None
                if extension:
                    parse_failures[extension] += 1
            if message is not None:
                parsed = True
                parsed_counts[extension] += 1
                resource_paths = iter_resource_strings(message)
                embedded_resources, topology, extra_samples = embedded_message_resources(message, extension)
                resource_paths.extend(embedded_resources)
                for extra_sample in extra_samples:
                    apply_resource_paths(
                        extra_sample["source_extension"],
                        extra_sample["resource_paths"],
                        edge_counts,
                        outgoing_histogram,
                    )
                    apply_topology(
                        extra_sample["source_extension"],
                        extra_sample["topology"],
                        topology_histograms,
                    )

        if extension:
            apply_resource_paths(extension, resource_paths, edge_counts, outgoing_histogram)
            apply_topology(extension, topology, topology_histograms)

        if parsed:
            parsed_file_count += 1
        reporter.advance(detail=f"parsed={parsed_file_count}")

    filtered_resource_counts = {
        extension: count
        for extension, count in sorted(extension_counts.items())
        if extension in PROFILE_RELEVANT_EXTENSIONS
    }
    filtered_size_histograms = filter_histogram_map(file_size_histograms)
    filtered_path_depth_histograms = filter_histogram_map(path_depth_histograms)
    filtered_edge_counts = filter_reference_map(edge_counts)
    filtered_outgoing_histogram = filter_histogram_map(outgoing_histogram)
    filtered_topology_histograms = {
        key: histogram
        for key, histogram in sorted(topology_histograms.items())
        if key in PROFILE_RELEVANT_TOPOLOGY_KEYS
    }
    filtered_total_edges = sum(
        count
        for targets in filtered_edge_counts.values()
        for count in targets.values()
    )

    return {
        "source_summary": {
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        },
        "dependencies": {
            "count": count_dependencies(project_path / "game.project"),
        },
        "resources": {
            "by_extension": filtered_resource_counts,
            "path_depth_histogram_by_extension": bucketize_histogram_map(filtered_path_depth_histograms),
            "size_histogram_by_extension": bucketize_histogram_map(filtered_size_histograms),
        },
        "references": {
            "total_edges": filtered_total_edges,
            "by_source_type": filtered_edge_counts,
            "outgoing_fanout_histogram": bucketize_histogram_map(filtered_outgoing_histogram),
        },
        "topology": bucketize_histogram_map(filtered_topology_histograms),
    }
