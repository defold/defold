#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

# Embeds sourcesContent into a wasm source map, like emscripten's
# `-gsource-map=inline` would, but tolerant of source files that are not
# valid UTF-8. emscripten's tools/wasm-sourcemap.py --sources (still in
# latest upstream) reads every source with strict UTF-8 decoding and only
# catches IOError/OSError, so a single stray Windows-1252 byte in e.g. a
# third-party header (Bullet's btScalar.h) aborts the whole link with a
# UnicodeDecodeError. We instead link with plain -gsource-map (which never
# reads source files) and run this script post-link, decoding each source
# with errors='replace'.
#
# Source map format: https://tc39.es/ecma426/ (sources/sourcesContent are
# parallel arrays; relative sources resolve against the map location).

import argparse
import json
import os
import sys

def resolve_source(src, source_root, search_dirs):
    candidates = []
    joined = os.path.join(source_root, src) if source_root and not os.path.isabs(src) else src
    if os.path.isabs(joined):
        candidates.append(joined)
    else:
        for d in search_dirs:
            candidates.append(os.path.join(d, joined))
    for candidate in candidates:
        if os.path.isfile(candidate):
            return candidate
    return None

def embed_sources(map_file, search_dirs):
    with open(map_file, 'r', encoding='utf-8') as f:
        source_map = json.load(f)

    sources = source_map.get('sources', [])
    source_root = source_map.get('sourceRoot', '')
    contents = []
    missing = 0
    for src in sources:
        path = resolve_source(src, source_root, search_dirs)
        if path is None:
            contents.append(None)
            missing += 1
            continue
        with open(path, 'rb') as f:
            contents.append(f.read().decode('utf-8', errors='replace'))

    source_map['sourcesContent'] = contents
    with open(map_file, 'w', encoding='utf-8') as f:
        json.dump(source_map, f, separators=(',', ':'))

    embedded = len(sources) - missing
    print('%s: embedded %d/%d sources%s' % (os.path.basename(map_file), embedded, len(sources),
          ' (%d not found on disk)' % missing if missing else ''))

def main():
    parser = argparse.ArgumentParser(description='Embed sourcesContent into a wasm source map')
    parser.add_argument('map_file', help='the .wasm.map file to update in place')
    parser.add_argument('--search-dir', action='append', default=[], dest='search_dirs',
                        help='extra directory to resolve relative source paths against (repeatable)')
    args = parser.parse_args()

    if not os.path.isfile(args.map_file):
        print('embed_wasm_sourcemap_sources.py: no such file, skipping: %s' % args.map_file)
        return 0

    # relative sources resolve against the map location first (emcc passes
    # --basepath=<output dir> to wasm-sourcemap.py), then any extra dirs
    search_dirs = [os.path.dirname(os.path.abspath(args.map_file))] + args.search_dirs
    embed_sources(args.map_file, search_dirs)
    return 0

if __name__ == '__main__':
    sys.exit(main())
