"""Exercise real HTTP, lifecycle and input ordering in the stock debug engine."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import threading
import time
import unittest
import urllib.error
import urllib.parse
import urllib.request

sys.path.insert(0, str(Path(__file__).resolve().parents[3] / 'debugger/src/test'))
from test_dap import Client as DAPClient

ARGS = None

class EngineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.process = None
        cls.log = None
        cls.runtime = None
        service_url = getattr(ARGS, 'service_url', None)
        if service_url:
            cls.url = service_url.rstrip('/')
            if not cls.url.endswith('/automation-bridge/v3'):
                cls.url += '/automation-bridge/v3'
            health = cls.until(lambda: cls.request('/health')[1].get('data'), 15)
            cls.runtime = health['engine_instance_id']
            states = cls.request('/state?name=fixture.ready')[1]['data']['states']
            if not any(state['value'] is True for state in states):
                raise AssertionError('supplied service must run the automation acceptance fixture')
            return
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            profiler_port = reservation.getsockname()[1]
        cls.url = f'http://127.0.0.1:{port}/automation-bridge/v3'
        adapter = getattr(ARGS, 'adapter', None)
        suffix = '-' + adapter if adapter else ''
        cls.log = open(Path(ARGS.fixture) / (Path(ARGS.engine).name + suffix + '-runtime.log'), 'w+')
        graphics_args = ['--graphics-adapter=' + adapter] if adapter else []
        cls.process = subprocess.Popen([ARGS.engine, f'--config=profiler.remotery_port={profiler_port}', 'build/default/game.projectc'] + graphics_args,
                                     cwd=ARGS.fixture, env=dict(os.environ, DM_SERVICE_PORT=str(port)),
                                     stdout=cls.log, stderr=cls.log)
        cls.runtime = None
        try:
            health = cls.until(lambda: cls.request('/health')[1].get('data'), 15)
            if health['identity']['process_id'] != cls.process.pid:
                raise AssertionError('service belongs to another process')
            cls.runtime = health['engine_instance_id']
            if adapter:
                installed = "Installed graphics device 'ADAPTER_FAMILY_" + adapter.upper() + "'"
                if installed not in Path(cls.log.name).read_text():
                    raise AssertionError('requested graphics adapter was not installed: ' + adapter)
        except BaseException:
            cls.tearDownClass()
            raise

    @classmethod
    def tearDownClass(cls):
        if cls.process is None:
            return
        cls.process.terminate()
        try:
            cls.process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            cls.process.kill()
            cls.process.wait()
        cls.log.close()

    @classmethod
    def until(cls, probe, timeout=5):
        deadline = time.monotonic() + timeout
        while True:
            try:
                value = probe()
                if value:
                    return value
            except (OSError, urllib.error.URLError):
                pass
            if cls.process is not None and cls.process.poll() is not None:
                raise AssertionError('engine exited; see runtime.log')
            if time.monotonic() >= deadline:
                raise AssertionError('completion was not observed before deadline')
            threading.Event().wait(0.01)

    @classmethod
    def request(cls, path, method='GET', body=None, raw=None, headers=None):
        data = json.dumps(body).encode() if body is not None else raw
        request_headers = {'Content-Type': 'application/json'}
        if cls.runtime: request_headers['X-Automation-Runtime'] = cls.runtime
        request_headers.update(headers or {})
        request = urllib.request.Request(cls.url + path, data=data, method=method, headers=request_headers)
        try:
            response = urllib.request.urlopen(request, timeout=2)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, json.load(response)

    def command(self, name, data=None):
        status, response = self.request('/commands', 'POST', {'name': name, 'data': data})
        self.assertEqual(202, status)
        command_id = response['data']['command_id']
        def completed():
            current = self.request('/commands?id=' + str(command_id))[1]['data']
            if current['state'] in ('failed', 'cancelled'):
                self.fail(str(current))
            return current if current['state'] == 'completed' else None
        return self.until(completed)['result']

    def reload_resource(self, path):
        # resource.Reload has one repeated string field; these fixture paths fit a one-byte length.
        encoded = path.encode()
        self.assertLess(len(encoded), 128)
        request = urllib.request.Request(self.url.split('/automation-bridge/')[0] + '/post/@resource/reload',
                                         data=b'\x0a' + bytes([len(encoded)]) + encoded, method='POST')
        with urllib.request.urlopen(request, timeout=2) as response:
            self.assertEqual(200, response.status)

    def input_events(self, action):
        return [event for event in self.command('input_events') if event['action'] == action]

    def delivered_input(self, input_id):
        def delivered():
            value = self.request('/input/status?input_id=' + str(input_id))[1]['data']
            return value if value.get('delivered_frame') else None
        return self.until(delivered)

    def advance_frames(self, count):
        frame = self.request('/frame')[1]['data']['engine_frame']
        self.until(lambda: self.request('/frame')[1]['data']['engine_frame'] >= frame + count)


class RuntimeTest(EngineTest):
    # Reload notifications invalidate only affected lifetimes without per-instance version storage.
    def test_hot_reload_identity_and_owner_cleanup(self):
        def element_ids():
            elements = self.request('/elements?limit=500')[1]['data']['elements']
            return {element['url']: element['id'] for element in elements if element.get('url')}

        def owned_count(name):
            return self.request('/application/catalog?name=' + name)[1]['data']['count']

        self.command('create_owner')
        self.until(lambda: owned_count('owned') == 1)
        try:
            initial = element_ids()
            script = next(url for url in initial if url.endswith('#owner'))
            gui = next(url for url in initial if url.endswith('#gui/recreated'))
            unchanged = 'main:/controller#script'

            self.reload_resource('/owner.scriptc')
            self.until(lambda: owned_count('owned') == 0)
            after_script = element_ids()
            self.assertNotEqual(initial[script], after_script[script])
            self.assertEqual(initial[gui], after_script[gui])
            self.assertEqual(initial[unchanged], after_script[unchanged])
            self.assertEqual(0, self.request('/elements?automation_id=owned.annotation')[1]['data']['matched'])
            self.assertEqual({'live': True}, self.command('echo', {'live': True}))

            self.reload_resource('/owner.gui_scriptc')
            self.until(lambda: owned_count('owned.gui.recreate') == 0)
            after_gui_script = element_ids()
            self.assertNotEqual(after_script[gui], after_gui_script[gui])
            self.assertEqual(after_script[script], after_gui_script[script])

            previous = after_gui_script
            for resource in ('/owner.guic', '/owner.guic', '/owner.goc', '/owner.goc'):
                with self.subTest(resource=resource):
                    self.reload_resource(resource)
                    def replaced():
                        current = element_ids()
                        return current if current.get(gui) != previous[gui] else None
                    current = self.until(replaced)
                    self.assertEqual(initial[unchanged], current[unchanged])
                    if resource.endswith('.goc'):
                        self.assertNotEqual(previous[script], current[script])
                    else:
                        self.assertEqual(previous[script], current[script])
                    previous = current
        finally:
            self.command('delete_owner')
            self.until(lambda: owned_count('owned.gui.recreate') == 0)

    # Health must remain cheap and omit capture and window capabilities in headless engines.
    def test_health_and_headless_capabilities(self):
        status, response = self.request('/health')
        self.assertEqual(200, status)
        health = response['data']
        self.assertEqual('3', health['version'])
        self.assertTrue(health['backend']['headless'])
        self.assertEqual('null', health['backend']['adapter'])
        self.assertNotIn('native_version', health)
        for name in ('scene', 'observations', 'input.key', 'application.state'):
            self.assertIn(name, health['capabilities'])
        for name in ('screenshot', 'screen.resize', 'recording.video', 'metal.capture'):
            self.assertNotIn(name, health['capabilities'])
        sequence = health['scene_sequence']
        self.assertEqual(sequence, self.request('/health')[1]['data']['scene_sequence'])
        self.assertEqual(501, self.request('/screenshot', 'POST', {})[0])
        extension = self.request('/state?name=fixture.extension')[1]['data']['states'][0]['value']
        self.assertEqual(ARGS.extension, extension)

    # Real error statuses and strict typed JSON prevent malformed mutations entering the queue.
    def test_transport_validation(self):
        cases = [('/input/click', b'{', 400),
                 ('/input/click', b'{"x":"2","y":3}', 400),
                 ('/input/click', b'{"x":1,"x":2,"y":3}', 400),
                 ('/coordinates/convert', b'{"point":{"x":"1","y":2}}', 400),
                 ('/markers', b'{"data":{"x":1,"x":2}}', 400),
                 ('/markers', b'{}\0{}', 400),
                 ('/markers', b'{"name":"\\uD800"}', 400),
                 ('/markers', b'{"data":' + b'[' * 17 + b'0' + b']' * 17 + b'}', 400),
                 ('/markers', b' ' * 65537, 413)]
        for path, raw, expected in cases:
            with self.subTest(raw=raw[:32]):
                status, response = self.request(path, 'POST', raw=raw)
                self.assertEqual(expected, status)
                self.assertFalse(response['ok'])
        self.assertEqual(415, self.request('/markers', 'POST', raw=b'{}', headers={'Content-Type': 'text/plain'})[0])
        self.assertEqual(405, self.request('/health', 'POST', {})[0])
        self.assertEqual(400, self.request('/input/click?x=1', 'POST', {})[0])

    def openapi(self):
        with urllib.request.urlopen(self.url.split('/automation-bridge/')[0] + '/openapi.json', timeout=2) as response:
            return json.load(response)

    # Discovery includes typed mutations, selectors, opaque path IDs, and bounded binary artifact transfers.
    def test_openapi_discovery(self):
        document = self.openapi()
        self.assertEqual('3.0.3', document['openapi'])
        paths = document['paths']
        prefix = '/automation-bridge/v3'
        self.assertEqual({'get', 'put'}, set(paths[prefix + '/screen']))
        self.assertEqual({'get', 'post', 'delete'}, set(paths[prefix + '/commands']))
        self.assertEqual({'200', 'default'}, set(paths[prefix + '/health']['get']['responses']))
        self.assertEqual({'202', 'default'}, set(paths[prefix + '/commands']['post']['responses']))
        click = paths[prefix + '/input/click']['post']
        schema = click['requestBody']['content']['application/json']['schema']
        self.assertFalse(schema['additionalProperties'])
        self.assertEqual('number', schema['properties']['x']['type'])
        self.assertEqual('array', schema['properties']['modifiers']['type'])
        self.assertEqual('string', schema['properties']['client_id']['type'])
        observation = paths[prefix + '/observations']['post']['requestBody']['content']['application/json']['schema']
        self.assertEqual('string', observation['properties']['automation_id']['type'])
        self.assertEqual({'type': 'string'}, observation['properties']['ids']['items'])
        self.assertEqual({}, paths[prefix + '/commands']['post']['requestBody']['content']['application/json']['schema']['properties']['data'])
        elements = paths[prefix + '/elements']['get']['parameters']
        self.assertEqual('string', next(p for p in elements if p['name'] == 'cursor')['schema']['type'])
        self.assertIn('X-Automation-Runtime', [p['name'] for p in elements])
        self.assertEqual('boolean', next(p for p in elements if p['name'] == 'visible')['schema']['type'])
        artifact = paths[prefix + '/artifacts/{id}']
        self.assertTrue(next(p for p in artifact['get']['parameters'] if p['name'] == 'id')['required'])
        self.assertTrue(next(p for p in artifact['get']['parameters'] if p['name'] == 'Range')['required'])
        self.assertEqual('binary', artifact['get']['responses']['206']['content']['application/octet-stream']['schema']['format'])
        self.assertIn('204', artifact['delete']['responses'])
        self.assertIn(prefix + '/observations/{id}', paths)
        self.assertIn(prefix + '/events/wait', paths)
        self.assertIn('/ping', paths)
        self.assertEqual(200, self.request('/health')[0])

    # Route schemas reject coerced strings, mixed-type IDs, invalid selectors, and misspelled options.
    def test_request_field_types(self):
        cases = [('POST', '/input/key', {'text': value}) for value in (123, [1, 2], {}, True, None)]
        cases += [('POST', '/input/key', {'keys': ['KEY_SPACE']}),
                  ('POST', '/input/key', {'text': 'a', 'client_id': 123}),
                  ('POST', '/input/click', {'id': 123}),
                  ('POST', '/input/click', {'x': 1, 'y': 2, 'device': []}),
                  ('POST', '/input/drag', {'x1': 1, 'y1': 1, 'x2': 2, 'y2': 2, 'duraiton': 1}),
                  ('DELETE', '/commands', {'id': '1'}),
                  ('POST', '/coordinates/convert', {'point': [1, 2], 'from_space': 'window', 'to_space': 'window'}),
                  ('POST', '/observations', {'ids': [123]}),
                  ('POST', '/observations', {'visible': 'false'})]
        for method, path, body in cases:
            with self.subTest(path=path, body=body):
                self.assertEqual(400, self.request(path, method, body)[0])
        data = {'text': [1, 2], 'duration': 'application-owned', 'id': False}
        self.assertEqual(data, self.command('echo', data))

    # Explicit invalid optional numbers must fail instead of taking the omitted-value default.
    def test_invalid_optional_numbers(self):
        drag = b'{"x1":1,"y1":1,"x2":2,"y2":2,'
        cases = [('/input/drag', drag, field) for field in ('duration', 'hold_before', 'hold_after', 'lease')]
        cases += [('/input/pointer/open', b'{"x":1,"y":2,', 'pointer_lease'),
                  ('/input/key', b'{"keys":"{KEY_SPACE}",', 'hold')]
        for path, prefix, field in cases:
            for value in (b'null', b'"0.1"', b'[]', b'{}', b'true', b'1e400', b'-1e400', b'1e40'):
                with self.subTest(path=path, field=field, value=value):
                    raw = prefix + json.dumps(field).encode() + b':' + value + b'}'
                    self.assertEqual(400, self.request(path, 'POST', raw=raw)[0])
        self.assertEqual(400, self.request('/observations', 'POST', raw=b'{"frame":18446744073709551616}')[0])
        self.assertEqual(400, self.request('/coordinates/convert', 'POST', raw=b'{"point":{"x":1e400,"y":2},"x":1,"from_space":"window","to_space":"window"}')[0])

    # Omitted durations keep their defaults, explicit zero remains zero, and missing holds return an error.
    def test_optional_input_defaults(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False}
        for duration in (None, 0):
            body = dict(owner, x1=20, y1=20, x2=40, y2=40)
            if duration is not None: body['duration'] = duration
            status, response = self.request('/input/drag', 'POST', body)
            self.assertEqual(202, status)
            self.assertAlmostEqual(0.35 if duration is None else duration, response['data']['requested_duration'])
            self.delivered_input(response['data']['input_id'])
        status, response = self.request('/input/pointer/open', 'POST', dict(owner, x=20, y=20))
        self.assertEqual(202, status)
        input_id = response['data']['input_id']
        try:
            self.assertEqual(400, self.request('/input/pointer/hold', 'POST', dict(owner, input_id=input_id))[0])
        finally:
            self.request('/input/pointer/up', 'POST', dict(owner, input_id=input_id))
            self.delivered_input(input_id)

    # Reject malformed raw UTF-8 before key decoding or retained application JSON can consume it.
    def test_utf8_validation(self):
        invalid = (b'\x80' * 7, b'\xc0\x80', b'\xc1\xbf', b'\xc2', b'\xc2A',
                   b'\xe0\x80\x80', b'\xed\xa0\x80', b'\xf0\x80\x80\x80',
                   b'\xf4\x90\x80\x80', b'\xf5\x80\x80\x80', b'\xff')
        for text in invalid:
            for path, prefix in (('/input/key', b'{"text":"'), ('/markers', b'{"name":"utf8","data":"')):
                with self.subTest(path=path, text=text):
                    self.assertEqual(400, self.request(path, 'POST', raw=prefix + text + b'"}')[0])
        # Raw multi-byte characters and escaped surrogate pairs must decode identically.
        text = '\u0080\u07ff\u0800\ud7ff\ue000\uffff\U00010000\U0010ffff'
        for ensure_ascii in (False, True):
            raw = json.dumps({'name': 'echo', 'data': text}, ensure_ascii=ensure_ascii).encode('utf-8')
            status, response = self.request('/commands', 'POST', raw=raw)
            self.assertEqual(202, status)
            command_id = response['data']['command_id']
            def completed():
                value = self.request('/commands?id=' + str(command_id))[1]['data']
                return value if value['state'] == 'completed' else None
            self.assertEqual(text, self.until(completed)['result'])

    # Invalid final array members must not silently become unmodified input.
    def test_invalid_modifier_arrays(self):
        for modifiers in ([], ['KEY_BOGUS'], ['KEY_LSHIFT', 'KEY_BOGUS'], ['KEY_LSHIFT,KEY_LCTRL']):
            with self.subTest(modifiers=modifiers):
                status, response = self.request('/input/click', 'POST', {'x': 1, 'y': 1, 'modifiers': modifiers})
                self.assertEqual((400, 'unsupported_key'), (status, response['error']['code']))

    # Snapshot-qualified cursors preserve a page and explicitly expire after retention eviction.
    def test_immutable_pagination_and_observation(self):
        first = self.request('/elements?limit=1')[1]['data']
        cursor = first['next_cursor']
        self.assertIsNotNone(cursor)
        second = self.request('/elements?limit=1&cursor=' + urllib.parse.quote(cursor))[1]['data']
        self.assertEqual(first['snapshot_id'], second['snapshot_id'])
        self.assertNotEqual(first['elements'][0]['id'], second['elements'][0]['id'])
        status, pending = self.request('/observations', 'POST', {})
        self.assertEqual(202, status)
        path = '/observations/' + urllib.parse.quote(pending['data']['id'], safe='')
        def complete():
            value = self.request(path)[1]['data']
            return value if value['state'] == 'complete' else None
        observation = self.until(complete)
        if self.request('/health')[1]['data']['backend']['headless']:
            self.assertIsNone(observation['render_frame'])
        else:
            self.assertEqual(observation['simulation_frame'], observation['render_frame'])
        self.assertTrue(observation['published_state']['states'])
        for _ in range(17): self.request('/elements?limit=0')
        status, response = self.request('/elements?cursor=' + urllib.parse.quote(cursor))
        self.assertEqual((410, 'stale_snapshot'), (status, response['error']['code']))

    # A future observation completed later stays retained even if it was created before older snapshots.
    def test_observation_retention_uses_completion_order(self):
        frame = self.request('/frame')[1]['data']['engine_frame']
        status, pending = self.request('/observations', 'POST', {'frame': frame + 120})
        self.assertEqual(202, status)
        path = '/observations/' + urllib.parse.quote(pending['data']['id'], safe='')
        for _ in range(17): self.request('/elements?limit=0')
        def complete():
            status, response = self.request(path)
            self.assertIn(status, (200, 202))
            return response['data'] if response['data']['state'] == 'complete' else None
        observation = self.until(complete)
        self.assertGreaterEqual(observation['simulation_frame'], frame + 120)
        self.assertEqual(200, self.request(path)[0])

    # Application polling returns promptly and published counters survive JSON serialization exactly.
    def test_application_progress_and_json_values(self):
        before = self.request('/frame')[1]['data']['engine_frame']
        status, accepted = self.request('/commands', 'POST', {'name': 'publish', 'data': {'nested': [True, None, 7]}})
        command_id = accepted['data']['command_id']
        def completed():
            value = self.request('/commands?id=' + str(command_id))[1]['data']
            return value if value.get('state') in ('complete', 'failed', 'completed') else None
        result = self.until(completed)
        self.assertNotEqual('failed', result['state'])
        states = self.request('/state?name=fixture.value')[1]['data']['states']
        self.assertEqual({'nested': [True, None, 7]}, states[0]['value'])
        start = time.monotonic()
        self.request('/state/wait?after_revision=0&timeout_ms=30000')
        self.assertLess(time.monotonic() - start, 1)
        self.until(lambda: self.request('/frame')[1]['data']['engine_frame'] > before)
        self.assertIsInstance(states[0]['revision'], int)

    # The documented event-wait route shares cursor polling and validation with /events.
    def test_event_wait_route(self):
        cursor = self.request('/events/cursor')[1]['data']['cursor']
        path = '/events/wait?cursor=' + str(cursor) + '&timeout_ms=30000'
        status, response = self.request(path)
        self.assertEqual(200, status)
        self.assertFalse(any(event['name'] == 'fixture.event_wait' for event in response['data']['events']))
        self.assertEqual(200, self.request('/markers', 'POST', {'name': 'fixture.event_wait'})[0])
        status, response = self.request(path)
        self.assertEqual(200, status)
        self.assertTrue(any(event['name'] == 'fixture.event_wait' for event in response['data']['events']))
        self.assertEqual(400, self.request('/events/wait?timeout_ms=30001')[0])
        self.assertEqual(405, self.request('/events/wait', 'POST', {})[0])

    # FIFO delivery and competing controller rejection are observable after real engine input dispatch.
    def test_input_receipts_and_ownership(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture'}
        receipts = []
        for key in ('SPACE', 'SPACE'):
            status, response = self.request('/input/key', 'POST', dict(owner, keys='{KEY_'+key+'}', hold=0.01))
            self.assertEqual(202, status)
            receipts.append(response['data']['input_id'])
        status, response = self.request('/input/key', 'POST', {'keys': '{KEY_SPACE}', 'client_id': 'competitor'})
        self.assertEqual(409, status)
        delivered = []
        for receipt in receipts:
            def complete():
                value = self.request('/input/status?input_id=' + str(receipt))[1]['data']
                return value if value.get('delivered_frame') else None
            delivered.append(self.until(complete))
        self.assertLessEqual(delivered[0]['delivered_frame'], delivered[1]['start_frame'])
        self.assertEqual(200, self.request('/input/flush', 'POST', dict(owner, release=True))[0])

    def check_pointer_hold_and_move(self, device, action):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False, 'device': device}
        self.command('reset_input')
        status, response = self.request('/input/pointer/open', 'POST', dict(owner, x=20, y=20, pointer_lease=5))
        self.assertEqual(202, status)
        input_id = response['data']['input_id']
        try:
            self.advance_frames(4)
            events = self.input_events(action)
            self.assertGreaterEqual(len(events), 4)
            self.assertEqual(1, sum(event['pressed'] for event in events))
            self.assertFalse(any(event['released'] for event in events))
            position = (events[0]['x'], events[0]['y'])
            self.assertTrue(all((event['x'], event['y']) == position for event in events))
            status, _ = self.request('/input/pointer/move', 'POST', dict(owner, input_id=input_id, x=40, y=40, duration=0))
            self.assertEqual(200, status)
            self.advance_frames(4)
            events = self.input_events(action)
            self.assertNotEqual(position, (events[-1]['x'], events[-1]['y']))
            self.assertTrue(all((event['x'], event['y']) == (events[-1]['x'], events[-1]['y']) for event in events[-4:]))
            self.assertEqual(1, sum(event['pressed'] for event in events))
            self.assertFalse(any(event['released'] for event in events))
            self.assertEqual(200, self.request('/input/pointer/up', 'POST', dict(owner, input_id=input_id))[0])
            self.delivered_input(input_id)
            self.advance_frames(3)
            self.assertEqual(1, sum(event['released'] for event in self.input_events(action)))
        finally:
            self.request('/input/flush', 'POST', dict(owner, release=True))

    # Open pointers retain position and button state across HID polls, moves, and idle frames.
    def test_pointer_hold_and_move(self):
        self.check_pointer_hold_and_move('mouse', 'pointer')

    # A held touch is replaced each frame without duplicating contacts or losing the release.
    def test_touch_hold_and_move(self):
        self.check_pointer_hold_and_move('touch', 'touch')

    # Holds surrounding a drag must not introduce intermediate releases or repeated presses.
    def test_drag_holds(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False}
        self.command('reset_input')
        status, response = self.request('/input/drag', 'POST', dict(owner, x1=20, y1=20, x2=40, y2=40, device='mouse',
                                                                 duration=0.1, hold_before=0.1, hold_after=0.1))
        self.assertEqual(202, status)
        self.delivered_input(response['data']['input_id'])
        events = self.input_events('pointer')
        self.assertEqual(1, sum(event['pressed'] for event in events))
        self.assertEqual(1, sum(event['released'] for event in events))
        self.assertTrue(events[0]['pressed'])
        self.assertTrue(events[-1]['released'])
        self.assertGreater(events[-1]['frame'] - events[0]['frame'], 6)
        for samples in (events[:3], events[-3:]):
            self.assertTrue(all((event['x'], event['y']) == (samples[0]['x'], samples[0]['y']) for event in samples))

    # Repeated taps exceed packet capacity without retaining old touches or losing releases.
    def test_touch_taps(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False}
        self.command('reset_input')
        for _ in range(16):
            status, response = self.request('/input/click', 'POST', dict(owner, x=20, y=20, device='touch'))
            self.assertEqual(202, status)
            self.delivered_input(response['data']['input_id'])
        events = self.input_events('touch')
        self.assertEqual(16, sum(event['pressed'] for event in events))
        self.assertEqual(16, sum(event['released'] for event in events))
        self.assertTrue(events[-1]['released'])
        self.advance_frames(3)
        self.assertEqual(events, self.input_events('touch'))

    # Cancellation must dispatch exactly one release even when another touch is queued behind it.
    def test_touch_cancellation(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False, 'device': 'touch'}
        self.command('reset_input')
        status, response = self.request('/input/pointer/open', 'POST', dict(owner, x=20, y=20, pointer_lease=5))
        self.assertEqual(202, status)
        input_id = response['data']['input_id']
        try:
            self.advance_frames(3)
            status, queued = self.request('/input/click', 'POST', dict(owner, x=40, y=40))
            self.assertEqual(202, status)
            self.assertEqual(200, self.request('/input/cancel', 'POST', dict(owner, input_id=input_id))[0])
            self.assertEqual('cancelled', self.delivered_input(input_id)['state'])
            self.delivered_input(queued['data']['input_id'])
            events = self.input_events('touch')
            self.assertEqual(2, sum(event['pressed'] for event in events))
            self.assertEqual(2, sum(event['released'] for event in events))
            self.assertTrue(events[-1]['released'])
            self.advance_frames(3)
            self.assertEqual(events, self.input_events('touch'))
        finally:
            self.request('/input/flush', 'POST', dict(owner, release=True))

    # Synthetic HID events retain the upstream source classification in both GO and GUI callbacks.
    def test_input_sources(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False}
        cases = [('/input/key', {'keys': '{KEY_SPACE}'}, 'space', 'keyboard'),
                 ('/input/key', {'text': 'aé'}, 'text', 'text'),
                 ('/input/click', {'x': 20, 'y': 20, 'device': 'touch'}, 'touch', 'touch'),
                 ('/input/click', {'x': 60, 'y': 40, 'device': 'mouse'}, 'pointer', 'mouse')]
        try:
            for path, body, action, source in cases:
                with self.subTest(source=source):
                    self.command('reset_input')
                    self.command('gui_reset_input')
                    status, response = self.request(path, 'POST', dict(owner, **body))
                    self.assertEqual(202, status)
                    self.delivered_input(response['data']['input_id'])
                    for command in ('input_events', 'gui_input_events'):
                        with self.subTest(callback=command):
                            events = self.command(command)
                            samples = [event for event in events if event['action'] == action]
                            self.assertTrue(samples)
                            self.assertEqual({source}, {sample['source'] for sample in samples})
                            if source == 'text':
                                self.assertEqual(body['text'], ''.join(sample['text'] for sample in samples))
                            else:
                                self.assertEqual(1, sum(sample['pressed'] for sample in samples))
                                self.assertEqual(1, sum(sample['released'] for sample in samples))
                            if source == 'mouse':
                                moves = [event for event in events if event['action'] == 'move']
                                self.assertTrue(moves)
                                self.assertEqual({'mouse'}, {event['source'] for event in moves})
        finally:
            self.request('/input/flush', 'POST', dict(owner, release=True))

    # DAP stops keep diagnostics/cancellation responsive, expire leases, and resume without stuck input.
    def test_debugger_pause_and_resume(self):
        port = self.command('start_debugger')
        for resume in ('continue', 'disconnect'):
            with self.subTest(resume=resume):
                client = DAPClient(port)
                try:
                    client.initialize()
                    client.attach()
                    client.configured()
                    self.command('reset_input')
                    owner = {'client_id': 'runtime-test', 'session_id': 'fixture', 'visualize': False}
                    status, response = self.request('/input/pointer/open', 'POST',
                                                    dict(owner, x=20, y=20, lease=10, pointer_lease=1))
                    self.assertEqual(202, status)
                    input_id = response['data']['input_id']
                    self.until(lambda: any(event['pressed'] for event in self.input_events('pointer')))
                    thread_id = client.request('threads')['threads'][0]['id']
                    client.request('pause', {'threadId': thread_id})
                    stopped = client.event('stopped')
                    self.assertTrue(self.request('/health')[1]['data']['debugger_paused'])
                    frame = self.request('/frame')[1]['data']['engine_frame']
                    for path, method, body in (('/scene', 'GET', None), ('/commands', 'POST', {'name': 'echo'}),
                                               ('/observations', 'POST', {}), ('/input/click', 'POST', {'x': 1, 'y': 1})):
                        status, response = self.request(path, method, body)
                        self.assertEqual((409, 'debugger_paused'), (status, response['error']['code']))
                    def expired():
                        receipt = self.request('/input/status?input_id=' + str(input_id))[1]['data']
                        return receipt if receipt['state'] == 'cancelled' else None
                    self.assertEqual('pointer_lease_expired', self.until(expired)['reason'])
                    self.assertEqual(frame, self.request('/frame')[1]['data']['engine_frame'])
                    self.assertEqual(200, self.request('/input/flush', 'POST', dict(owner, release=True))[0])
                    client.request(resume, {'threadId': stopped['threadId']})
                    self.until(lambda: not self.request('/health')[1]['data']['debugger_paused'])
                    self.advance_frames(3)
                    events = self.input_events('pointer')
                    self.assertEqual(1, sum(event['released'] for event in events))
                    self.assertEqual({'resumed': True}, self.command('echo', {'resumed': True}))
                finally:
                    client.close()

    # Script/GUI owner cleanup removes callbacks and annotations; recreated GUI nodes get new IDs.
    def test_owner_deletion_and_recreation(self):
        def command(name, data=None):
            status, response = self.request('/commands', 'POST', {'name': name, 'data': data})
            self.assertEqual(202, status)
            command_id = response['data']['command_id']
            def completed():
                current = self.request('/commands?id=' + str(command_id))[1]['data']
                return current if current['state'] == 'completed' else None
            return self.until(completed)['result']
        command('create_owner')
        self.until(lambda: self.request('/application/catalog?name=owned')[1]['data']['count'] == 1)
        before = self.request('/elements?type=scriptc&name_exact=owner')[1]['data']['elements'][0]['id']
        annotation = self.request('/elements?automation_id=owned.annotation')[1]['data']['elements']
        self.assertEqual(1, len(annotation))
        gui_before = self.request('/elements?name_exact=recreated')[1]['data']['elements'][0]['id']
        command('owned.gui.recreate')
        gui_after = self.request('/elements?name_exact=recreated')[1]['data']['elements'][0]['id']
        self.assertNotEqual(gui_before, gui_after)
        command('delete_owner')
        self.until(lambda: self.request('/application/catalog?name=owned')[1]['data']['count'] == 0)
        self.until(lambda: self.request('/application/catalog?name=owned.gui.recreate')[1]['data']['count'] == 0)
        self.assertEqual(0, self.request('/elements?automation_id=owned.annotation')[1]['data']['matched'])
        self.assertEqual({'live': True}, command('echo', {'live': True}))
        command('create_owner')
        self.until(lambda: self.request('/application/catalog?name=owned')[1]['data']['count'] == 1)
        after = self.request('/elements?type=scriptc&name_exact=owner')[1]['data']['elements'][0]['id']
        self.assertNotEqual(before, after)
        command('delete_owner')

    # Duplicate names across collections and collection reloads must not alias lifetime IDs.
    def test_collection_identity_and_reload(self):
        def secondary():
            elements = self.request('/elements?name_exact=/controller&type_exact=goc')[1]['data']['elements']
            return elements if len(elements) == 2 else None
        before = self.until(secondary)
        self.assertEqual(2, len({element['id'] for element in before}))
        by_url = {element['url']: element['id'] for element in before}
        self.assertEqual({'main:/controller', 'secondary:/controller'}, set(by_url))
        status, response = self.request('/commands', 'POST', {'name': 'reload_collection'})
        self.assertEqual(202, status)
        def replaced():
            elements = secondary()
            if not elements: return None
            current = {element['url']: element['id'] for element in elements}
            return current if current['secondary:/controller'] != by_url['secondary:/controller'] else None
        after = self.until(replaced)
        self.assertEqual(by_url['main:/controller'], after['main:/controller'])

    # Runtime identity guards prevent old clients from mutating a newly initialized engine.
    def test_stale_runtime_rejection(self):
        status, response = self.request('/input/key', 'POST', {'key': 'SPACE'}, headers={'X-Automation-Runtime': 'engine:previous'})
        self.assertEqual((410, 'stale_runtime'), (status, response['error']['code']))

    # Reboot replaces the attached runtime and invalidates retained snapshots and client guards.
    def test_z_reboot_invalidates_runtime_and_snapshots(self):
        old_runtime = self.runtime
        old_openapi = self.openapi()
        snapshot_id = self.request('/elements?limit=1')[1]['data']['snapshot_id']
        request = urllib.request.Request(self.url.split('/automation-bridge/')[0] + '/post/@system/reboot',
                                         data=b'', method='POST')
        with urllib.request.urlopen(request, timeout=2) as response:
            self.assertEqual(200, response.status)
        def restarted():
            data = self.request('/health')[1].get('data', {})
            return data if data.get('engine_instance_id') not in (None, old_runtime) else None
        health = self.until(restarted)
        type(self).runtime = health['engine_instance_id']
        self.assertEqual(old_openapi, self.openapi())
        status, response = self.request('/observations/' + urllib.parse.quote(snapshot_id, safe=''))
        self.assertEqual((410, 'stale_snapshot'), (status, response['error']['code']))
        status, response = self.request('/frame', headers={'X-Automation-Runtime': old_runtime})
        self.assertEqual((410, 'stale_runtime'), (status, response['error']['code']))
        self.assertEqual(200, self.request('/frame')[0])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', required=True)
    parser.add_argument('--adapter')
    parser.add_argument('--extension', action='store_true')
    parser.add_argument('--fixture', required=True)
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__] + remaining)
