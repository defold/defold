"""Run the shared Bob Light and platform plugin build from a CMake target."""

import argparse
import json
import os
from pathlib import Path

import build


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('platform')
    parser.add_argument('--channel')
    parser.add_argument('--verbose', action='store_true')
    args = parser.parse_args()

    sdk = Path(os.environ['DYNAMO_HOME'])
    # build_engine supplies a fresh receipt for its build and run_tests phases.
    # Direct CMake requests always let Gradle check its complete set of inputs.
    receipt_path = os.environ.get('DEFOLD_BOB_LIGHT_RECEIPT')
    receipt = Path(receipt_path) if receipt_path else None
    if receipt and receipt.is_file():
        if (sdk / 'share/java/bob-light.jar').is_file():
            return
        receipt.unlink()

    build.build_private.set_target_platform(args.platform)
    configuration = build.Configuration(dynamo_home=str(sdk), target_platform=args.platform,
                                        channel=args.channel, verbose=args.verbose, no_colors='NOCOLOR' in os.environ)
    configuration.build_bob_light()
    if receipt:
        temporary = receipt.with_suffix('.tmp')
        temporary.write_text(json.dumps(configuration.build_tracker.get_summary()), encoding='utf-8')
        temporary.replace(receipt)


if __name__ == '__main__':
    main()
