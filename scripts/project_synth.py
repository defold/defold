#!/usr/bin/env python3

import importlib.util
from pathlib import Path


def main() -> int:
    script_dir = Path(__file__).resolve().parent
    cli_path = script_dir / "project_synth" / "cli.py"
    spec = importlib.util.spec_from_file_location("project_synth_cli", cli_path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to load CLI module from {cli_path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.main()


if __name__ == "__main__":
    raise SystemExit(main())
