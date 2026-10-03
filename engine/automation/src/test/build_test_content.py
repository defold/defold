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
args = parser.parse_args()
source = Path(__file__).parent / 'fixture'
output = Path(args.output)
shutil.copytree(source, output, dirs_exist_ok=True)
classpath = os.pathsep.join((args.bob, args.builtins))
subprocess.run([args.java, '-cp', classpath, 'com.dynamo.bob.Bob', '--root', str(output), '--platform', args.platform, 'build'], check=True)
(output / '.complete').write_text('compiled fixture\n')
