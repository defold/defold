"""Capture acceptance against known pixels using the shared graphics likeness metric."""
import argparse
import hashlib
import io
import json
from pathlib import Path
import platform
import sys
import unittest
import urllib.parse
import urllib.request

import test_runtime

sys.path.insert(0, str(Path(__file__).resolve().parents[4] / 'scripts'))
import likeness

# Independent top-left pixel expectations for the 320x240 fixture, including its clipped fill.
RECTANGLES = {
    'capture_red': ((8, 32, 56, 64), (255, 0, 0)),
    'capture_green': ((224, 48, 288, 96), (0, 255, 0)),
    'capture_blue': ((40, 176, 88, 208), (0, 0, 255)),
    'capture_fill': ((136, 104, 184, 136), (223, 96, 32)),
}


class CaptureTest(test_runtime.EngineTest):
    def capture(self, output):
        status, response = self.request('/observations', 'POST',
                                        {'screenshot': True, 'include': 'basic,bounds', 'limit': 500})
        self.assertEqual(202, status, response)
        path = '/observations/' + urllib.parse.quote(response['data']['id'], safe='')
        def completed():
            value = self.request(path)[1]['data']
            if value['state'] == 'failed':
                self.fail(value['failure_reason'])
            artifact = (value.get('screenshot') or {}).get('artifact') or {}
            if artifact.get('state') == 'failed':
                self.fail('screenshot artifact failed')
            return value if value['state'] == 'complete' and artifact.get('state') == 'complete' else None
        observation = self.until(completed)
        screenshot = observation['screenshot']
        self.assertEqual(observation['simulation_frame'], observation['render_frame'])
        self.assertEqual(observation['render_frame'], screenshot['engine_frame'])
        self.assertEqual(observation['scene_sequence'], screenshot['scene_sequence'])
        artifact = screenshot['artifact']
        url = self.url + '/artifacts/' + urllib.parse.quote(artifact['id'], safe='')
        try:
            chunks = []
            for offset in range(0, artifact['size'], 1024 * 1024):
                end = min(offset + 1024 * 1024, artifact['size']) - 1
                request = urllib.request.Request(url, headers={'Range': f'bytes={offset}-{end}',
                                                              'X-Automation-Runtime': self.runtime})
                with urllib.request.urlopen(request, timeout=2) as response:
                    self.assertEqual(206, response.status)
                    chunks.append(response.read())
            data = b''.join(chunks)
            self.assertEqual(artifact['size'], len(data))
            self.assertEqual(artifact['sha256'], hashlib.sha256(data).hexdigest())
            output.write_bytes(data)
        finally:
            request = urllib.request.Request(url, method='DELETE', headers={'X-Automation-Runtime': self.runtime})
            with urllib.request.urlopen(request, timeout=2) as response:
                self.assertEqual(204, response.status)
        with likeness.Image.open(io.BytesIO(data)) as image:
            self.assertEqual((screenshot['width'], screenshot['height']), image.size)
            return observation, image.convert('RGB')

    # Readback must preserve colors/orientation, coherent bounds, input coordinates and later stencil rendering.
    def test_capture_pixels_coordinates_and_repeated_render(self):
        self.assertIsNotNone(likeness.Image, 'Pillow is required; use the Defold build Python environment')
        health = self.request('/health')[1]['data']
        self.assertIn('screenshot', health['capabilities'])
        self.command('capture_fixture')
        output = Path(test_runtime.ARGS.fixture) / ('capture-' + test_runtime.ARGS.adapter)
        output.mkdir(exist_ok=True)
        previous = None
        records = []
        for index in range(3):
            observation, actual = self.capture(output / f'actual-{index}.png')
            expected = likeness.Image.new('RGB', actual.size, (0, 0, 0))
            sx, sy = actual.width / 320, actual.height / 240
            for bounds, color in RECTANGLES.values():
                expected.paste(color, tuple(round(v * (sx if axis % 2 == 0 else sy)) for axis, v in enumerate(bounds)))
            expected.save(output / 'expected.png')
            comparison = likeness.compare(actual, expected, output / f'difference-{index}.png')
            records.append(dict(frame=observation['render_frame'], **comparison))
            (output / 'results.json').write_text(json.dumps(dict(platform=platform.platform(),
                adapter=test_runtime.ARGS.adapter, health=health, captures=records), indent=2) + '\n')
            self.assertGreaterEqual(comparison['likeness_percent'], 99, str(output))
            if previous is not None:
                self.assertGreater(observation['render_frame'], previous[0])
                self.assertEqual(previous[1].tobytes(), actual.tobytes(), 'readback changed subsequent rendering')
            previous = (observation['render_frame'], actual)
            elements = {item['name']: item for item in observation['elements']}
            for name in ('capture_red', 'capture_green', 'capture_blue'):
                bounds, color = RECTANGLES[name]
                expected_center = ((bounds[0] + bounds[2]) / 2, (bounds[1] + bounds[3]) / 2)
                rect = elements[name]['bounds']['screen']
                center = {'x': rect['x'] + rect['w'] / 2, 'y': rect['y'] + rect['h'] / 2}
                screen = observation['screen']
                status, converted = self.request('/coordinates/convert', 'POST',
                                                {'point': center, 'from_space': 'window', 'to_space': 'normalized_viewport'})
                self.assertEqual(200, status, converted)
                point = converted['data']['point']
                self.assertAlmostEqual(expected_center[0] / 320, point['x'], delta=1 / actual.width)
                self.assertAlmostEqual(expected_center[1] / 240, point['y'], delta=1 / actual.height)
                pixel = (round(center['x'] * actual.width / screen['window']['width']),
                         round(center['y'] * actual.height / screen['window']['height']))
                self.assertEqual(color, actual.getpixel(pixel))
                if index == 0:
                    status, receipt = self.request('/input/click', 'POST', dict(center, client_id='capture-test',
                        session_id='fixture', visualize=False, device='mouse'))
                    self.assertEqual(202, status, receipt)
                    self.delivered_input(receipt['data']['input_id'])
        self.assertEqual(['capture_red', 'capture_green', 'capture_blue'], self.command('capture_picks'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', required=True)
    parser.add_argument('--fixture', required=True)
    parser.add_argument('--adapter', required=True, choices=('metal', 'opengl', 'opengles', 'vulkan'))
    test_runtime.ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__] + remaining)
