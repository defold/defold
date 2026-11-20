#!/usr/bin/env python3
"""Activate shader tool releases after packages for every Bob host are available."""

from pathlib import Path
import re
import tarfile


ROOT = Path(__file__).resolve().parents[2]
PLATFORMS = ('arm64-macos', 'x86_64-macos', 'arm64-linux', 'x86_64-linux', 'x86_64-win32')
TOOLS = ('glslang', 'spirv-tools', 'spirv-cross', 'tint')


def main():
    versions = {}
    for tool in TOOLS:
        script = ROOT / 'share/ext' / tool / f'build_{tool}.sh'
        versions[tool] = re.search(r'^VERSION=([\w.-]+)$', script.read_text(), re.MULTILINE)[1]

    msan = (ROOT / 'external/msan/CMakeLists.txt').read_text()
    if f'SPIRV-Cross/tar.gz/refs/tags/{versions["spirv-cross"]}\n' not in msan:
        raise SystemExit('Update the SPIRV-Cross source URL and checksum in external/msan/CMakeLists.txt first.')

    # install_ext installs tools for all Bob hosts, even on a single-host build.
    # Check every archive before changing any consumer references.
    for tool, version in versions.items():
        for platform in PLATFORMS:
            package = ROOT / 'packages' / f'{tool}-{version}-{platform}.tar.gz'
            if not package.is_file():
                raise SystemExit(f'Missing {package.name}; build all five host platforms first.')
            with tarfile.open(package) as archive:
                names = {member.name.removeprefix('./') for member in archive.getmembers() if member.isfile()}
            if tool == 'spirv-cross':
                suffix = '.lib' if platform.endswith('win32') else '.a'
                required = {f'lib/{platform}/libspirv-cross-{backend}{suffix}'
                            for backend in ('c', 'core', 'glsl', 'hlsl', 'msl')}
                required.update(f'include/{platform}/spirv/{header}' for header in ('spirv.h', 'spirv_cross_c.h'))
            else:
                executable = 'spirv-opt' if tool == 'spirv-tools' else tool
                suffix = '.exe' if platform.endswith('win32') else ''
                required = {f'bin/{platform}/{executable}{suffix}'}
            if not required.issubset(names):
                raise SystemExit(f'{package.name} is missing: {sorted(required - names)}')

    build = ROOT / 'scripts/build.py'
    content = build.read_text()
    for tool, version in versions.items():
        content, count = re.subn(r'"' + re.escape(tool) + r'-[^"\n]+"', f'"{tool}-{version}"', content)
        if count != len(PLATFORMS):
            raise SystemExit(f'Expected five {tool} package references, found {count}.')
    build.write_text(content)
    print('Updated shader tool packages for all five Bob hosts.')


if __name__ == '__main__':
    main()
