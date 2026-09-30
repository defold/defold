import glob
import os
import sys
from pathlib import Path


def _find_defold_home():
    env_home = os.environ.get("DEFOLD_HOME")
    if env_home:
        return Path(env_home).resolve()

    path = Path(__file__).resolve()
    for parent in path.parents:
        if (parent / "engine" / "script" / "src" / "script" / "sys_ddf.proto").exists():
            return parent
    return None


def _existing_paths(paths):
    existing = []
    seen = set()
    for path in paths:
        path = Path(path)
        if path.exists():
            path = str(path)
            if path not in seen:
                existing.append(path)
                seen.add(path)
    return existing


def _glob_existing(pattern):
    return [Path(path) for path in sorted(glob.glob(str(pattern))) if Path(path).exists()]


def add_engine_test_proto_paths():
    defold_home = _find_defold_home()
    dynamo_home = Path(os.environ.get("DYNAMO_HOME", "../../tmp/dynamo_home")).resolve()

    paths = [
        dynamo_home / "lib" / "python",
        dynamo_home / "lib" / "python" / "script",
        dynamo_home / "lib" / "python" / "engine",
        dynamo_home / "lib" / "python" / "gameobject",
    ]

    if defold_home:
        paths.extend([
            defold_home / "engine" / "script" / "build" / "src" / "script",
            defold_home / "engine" / "engine" / "build" / "proto" / "engine",
            defold_home / "engine" / "gameobject" / "build" / "proto" / "gameobject",
        ])
        paths.extend(_glob_existing(defold_home / "engine" / "ddf" / "build" / "*" / "python"))
        paths.extend(_glob_existing(defold_home / "engine" / "script" / "build" / "*" / "python"))
        paths.extend(_glob_existing(defold_home / "engine" / "engine" / "build" / "*" / "proto"))
        paths.extend(_glob_existing(defold_home / "engine" / "gameobject" / "build" / "*" / "proto"))
        paths.extend(_glob_existing(defold_home / "engine" / "ddf" / "build" / "*" / "python" / "ddf"))
        paths.extend(_glob_existing(defold_home / "engine" / "script" / "build" / "*" / "python" / "script"))
        paths.extend(_glob_existing(defold_home / "engine" / "engine" / "build" / "*" / "proto" / "engine"))
        paths.extend(_glob_existing(defold_home / "engine" / "gameobject" / "build" / "*" / "proto" / "gameobject"))

    sys.path = _existing_paths(paths) + sys.path
