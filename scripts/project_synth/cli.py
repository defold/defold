from __future__ import annotations

import argparse
import json
from pathlib import Path

from project_synth.generate import generate_project
from project_synth.profile import profile_project


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Profile a Defold project into aggregate-only topology data.")
    subparsers = parser.add_subparsers(dest="command", required=True)

    profile_parser = subparsers.add_parser("profile", help="Profile a source Defold project.")
    profile_parser.add_argument("--project-path", required=True, help="Path to the source Defold project.")
    profile_parser.add_argument("--out", help="Optional output JSON path. If omitted, the profile is printed to stdout.")

    generate_parser = subparsers.add_parser("generate", help="Generate a synthetic Defold project from a project or profile.")
    generate_parser.add_argument("--out-project", required=True, help="Directory where the synthetic project will be created.")
    source_group = generate_parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--project-path", help="Path to a source Defold project that should be profiled first.")
    source_group.add_argument("--profile", help="Path to a profile JSON file.")
    generate_parser.add_argument("--seed", type=int, default=0, help="Deterministic random seed.")
    generate_parser.add_argument("--scale", type=float, default=1.0, help="Scale factor applied to profile counts.")
    return parser


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()

    if args.command == "profile":
        project_path = Path(args.project_path)
        payload = profile_project(project_path)
        serialized = json.dumps(payload, indent=2, sort_keys=True) + "\n"
        if args.out:
            out_path = Path(args.out)
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_text(serialized, encoding="utf-8")
        else:
            print(serialized, end="")
        return 0

    if args.command == "generate":
        source_project_path = Path(args.project_path) if args.project_path else None
        profile_path = Path(args.profile) if args.profile else None
        generate_project(
            out_project_path=Path(args.out_project),
            source_project_path=source_project_path,
            profile_path=profile_path,
            seed=args.seed,
            scale=args.scale,
        )
        return 0

    parser.error(f"Unknown command: {args.command}")
    return 2
