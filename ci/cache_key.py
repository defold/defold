#!/usr/bin/env python3
"""Key the Actions download cache on the dependencies in its path allowlist."""

import ast
import hashlib
import json
from pathlib import Path
import re


CACHE_NAMESPACE = "dcache-dependencies-v3"
SDK_VERSIONS = (
    "VERSION_EDITOR_JDK",
    "VERSION_WINDOWS_SDK",
    "VISUAL_STUDIO_VERSION",
    "VERSION_WINDOWS_MSVC",
)
EDITOR_VERSIONS = {
    "sass": ("editor/tasks/leiningen/sass.clj",
             r'\(get-dart-sass-executable-path\s+"([^"\s]+)"'),
    "icu4j": ("editor/project.clj", r':icu4j\s+\{\s*:version\s+"([^"\s]+)"'),
    "lua-language-server": ("editor/project.clj",
                            r':lua-language-server\s+\{\s*:version\s+"([^"\s]+)"'),
    "template-empty": ("editor/resources/welcome/welcome.edn",
                       r':zip-url\s+"https://github.com/defold/template-empty/archive/([^"\s]+)\.zip"'),
}


def dependency_versions(read_text):
    # Parse declarations without importing sdk.py or executing build/editor code.
    sdk = ast.parse(read_text("build_tools/sdk.py"))
    versions = {}
    for name in SDK_VERSIONS:
        values = [ast.literal_eval(node.value) for node in sdk.body
                  if isinstance(node, ast.Assign)
                  and any(isinstance(target, ast.Name) and target.id == name
                          for target in node.targets)]
        if len(values) != 1 or not isinstance(values[0], str) or not values[0]:
            raise ValueError(f"Expected one string declaration for {name} in build_tools/sdk.py")
        versions[name] = values[0]

    for name, (path, pattern) in EDITOR_VERSIONS.items():
        # Remove line comments while preserving quoted strings, including URLs.
        source = re.sub(r'"(?:\\.|[^"\\])*"|;[^\n]*',
                        lambda match: "" if match[0].startswith(";") else match[0],
                        read_text(path))
        values = re.findall(pattern, source)
        if len(values) != 1:
            raise ValueError(f"Expected one version declaration for {name} in {path}")
        versions[name] = values[0]
    return versions


def cache_key(read_text):
    versions = json.dumps(dependency_versions(read_text), sort_keys=True, separators=(",", ":"))
    return f"{CACHE_NAMESPACE}-{hashlib.sha256(versions.encode('utf-8')).hexdigest()}"


if __name__ == "__main__":
    root = Path(__file__).resolve().parent.parent
    print(f"key={cache_key(lambda path: (root / path).read_text(encoding='utf-8'))}")
