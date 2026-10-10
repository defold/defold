#!/usr/bin/env python3
# Copyright 2020-2026 The Defold Foundation
# Licensed under the Defold License version 1.0 (https://www.defold.com/license).
"""Install test_app_graphics once and collect captures from an iOS simulator."""
from contextlib import contextmanager
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import uuid

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'build_tools'))
import build_ios


def completed_capture(process):
    # simctl can exit successfully when the app crashes or returns a failure.
    results = re.findall(r'^GRAPHICS_CAPTURE_RESULT=(-?\d+)$', process.stdout, re.MULTILINE)
    code = int(results[0]) if len(results) == 1 and process.returncode == 0 else 1
    if len(results) != 1:
        process.stdout += '\nERROR:GRAPHICS: Simulator capture did not report exactly one completion result\n'
    return subprocess.CompletedProcess(process.args, code, process.stdout)


class SimulatorCapture:
    def __init__(self, runner, simulator, bundle_id, directory):
        self.runner = runner
        self.simulator = simulator
        self.bundle_id = bundle_id
        self.directory = directory
        version = simulator.runtime.rsplit('iOS-', 1)[-1].replace('-', '.')
        self.metadata = dict(platform='iOS Simulator ' + version, machine='arm64',
                             target_platform='arm64_sim-ios', device=simulator.name,
                             simulator=simulator.udid)

    def run(self, command, **options):
        output = Path(command[command.index('--output-file') + 1])
        device_output = self.directory / 'captures' / output.parent.name / output.name
        device_output.parent.mkdir(parents=True, exist_ok=True)
        arguments = list(command[1:])
        arguments[arguments.index('--output-file') + 1] = str(device_output)
        launch = ['xcrun', 'simctl', 'launch', '--console', '--terminate-running-process',
                  self.simulator.udid, self.bundle_id, *arguments]
        env = dict(os.environ, SIMCTL_CHILD_DEFOLD_TEST_WORKDIR=str(self.directory))
        if 'MTL_DEBUG_LAYER' in os.environ:
            env['SIMCTL_CHILD_MTL_DEBUG_LAYER'] = os.environ['MTL_DEBUG_LAYER']
        try:
            return completed_capture(subprocess.run(launch, env=env, **options))
        except subprocess.TimeoutExpired:
            self.runner._simctl(['terminate', self.simulator.udid, self.bundle_id], allow_failure=True)
            raise
        finally:
            # Failed captures can still contain useful images and diagnostics.
            for image in device_output.parent.glob(device_output.name + '*'):
                if image.suffix == '.png':
                    shutil.copy2(image, output.parent / image.name)


@contextmanager
def simulator_capture(executable, device=None):
    runner = build_ios.IOSSimulatorTestRunner(device=device)
    simulator = runner.select_simulator()
    runner._boot_simulator_if_needed(simulator)
    bundle_id = 'com.defold.graphics-capture.' + uuid.uuid4().hex
    with tempfile.TemporaryDirectory(prefix='defold-graphics-simulator-') as temporary:
        app = str(Path(temporary) / 'GraphicsCapture.app')
        runner._create_app_bundle(str(executable), app, bundle_id)
        runner._sign_app_bundle(app)
        try:
            runner._install_app(simulator, app)
            directory = Path(runner._data_container(simulator, bundle_id)) / 'Documents/graphics-capture'
            directory.mkdir(parents=True)
            shutil.copytree(Path(__file__).with_name('texture_formats'), directory / 'texture_formats')
            yield SimulatorCapture(runner, simulator, bundle_id, directory)
        finally:
            runner._uninstall_app(simulator, bundle_id)
