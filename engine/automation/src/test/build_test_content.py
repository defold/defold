"""Compile the extension-free runtime fixture with the SDK's Bob."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--java', required=True)
parser.add_argument('--bob', required=True)
parser.add_argument('--builtins', required=True)
parser.add_argument('--platform', required=True)
parser.add_argument('--output', required=True)
parser.add_argument('--bundle-engine', type=Path, help='Bundle this local debug engine for arm64_sim-ios using full Bob')
args = parser.parse_args()
if args.bundle_engine and (args.platform != 'arm64_sim-ios' or not args.bundle_engine.is_file()):
    parser.error('--bundle-engine requires an existing arm64_sim-ios engine binary')
source = Path(__file__).parent / 'fixture'
output = Path(args.output)
shutil.copytree(source, output, dirs_exist_ok=True)
classpath = os.pathsep.join((args.bob, args.builtins))
command = [args.java, '-cp', classpath, 'com.dynamo.bob.Bob', '--root', str(output), '--platform', args.platform]
if args.bundle_engine:
    project = output / 'game.project'
    project.write_text(project.read_text().replace('title = Automation engine fixture', 'title = Defold Automation Acceptance')
                       + '\n[ios]\nbundle_identifier = com.defold.automation.acceptance\n')
    command += ['--variant', 'debug', '--archive', '--bundle-output', str(output / 'bundles')]
subprocess.run(command + ['build'], check=True)
if args.bundle_engine:
    # Bob's build cleans the engine cache: stage our binary after build and before bundle.
    binary = output / 'build' / args.platform / 'dmengine'
    binary.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(args.bundle_engine, binary)
    subprocess.run(command + ['bundle'], check=True)
(output / '.complete').write_text('compiled fixture\n')
