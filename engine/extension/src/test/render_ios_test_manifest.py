"""Render the shipping iOS manifest for a scene application test bundle."""

import argparse
import plistlib
from pathlib import Path

import pystache


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--executable', required=True)
    parser.add_argument('--bundle-id', required=True)
    parser.add_argument('--platform', choices=('iPhoneOS', 'iPhoneSimulator'), required=True)
    parser.add_argument('--orientations', nargs='+', required=True)
    args = parser.parse_args()

    context = {
        'ios': {'default_language': 'en', 'pre_renderered_icons': 'false', 'bundle_identifier': args.bundle_id},
        'project': {'title': args.executable, 'version': '1.0', 'write_log': 'false'},
        'exe-name': args.executable,
        'bundle-name': args.executable,
        'bundle-version': '1',
        'application-localizations': [],
        'application-queries-schemes': [],
        'orientation-support': args.orientations,
        'variant_release': True,
        'launch-screen': 'LaunchScreen',
    }
    renderer = pystache.Renderer(missing_tags='strict')
    info = plistlib.loads(renderer.render(args.manifest.read_text(), context).encode('utf-8'))
    # Xcode uses this plist directly; the command-line runner also supplies the
    # selected platform when it assembles the test app.
    info['CFBundleSupportedPlatforms'] = [args.platform]
    args.output.write_bytes(plistlib.dumps(info))


if __name__ == '__main__':
    main()
