"""Exercise real HTTP, lifecycle and input ordering in the stock debug engine."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess
import threading
import time
import unittest
import urllib.error
import urllib.parse
import urllib.request

ARGS = None

class RuntimeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            port = reservation.getsockname()[1]
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            profiler_port = reservation.getsockname()[1]
        cls.url = f'http://127.0.0.1:{port}/automation-bridge/v3'
        cls.log = open(Path(ARGS.fixture) / (Path(ARGS.engine).name + '-runtime.log'), 'w+')
        cls.process = subprocess.Popen([ARGS.engine, f'--config=profiler.remotery_port={profiler_port}', 'build/default/game.projectc'],
                                     cwd=ARGS.fixture, env=dict(os.environ, DM_SERVICE_PORT=str(port)),
                                     stdout=cls.log, stderr=cls.log)
        cls.runtime = None
        try:
            health = cls.until(lambda: cls.request('/health')[1].get('data'), 15)
            if health['identity']['process_id'] != cls.process.pid:
                raise AssertionError('service belongs to another process')
            cls.runtime = health['engine_instance_id']
        except BaseException:
            cls.tearDownClass()
            raise

    @classmethod
    def tearDownClass(cls):
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
            if cls.process.poll() is not None:
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
        self.assertIsNone(observation['render_frame'])
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

    # The null HID backend must dispatch touch bindings, not merely complete input receipts.
    def test_touch_dispatch_in_headless(self):
        owner = {'client_id': 'runtime-test', 'session_id': 'fixture'}
        cursor = self.request('/events/cursor')[1]['data']['cursor']
        status, response = self.request('/input/click', 'POST', dict(owner, x=20, y=20, device='touch'))
        self.assertEqual(202, status)
        input_id = response['data']['input_id']
        def dispatched():
            events = self.request('/events?cursor=' + str(cursor))[1]['data']['events']
            return any(event['data'].get('action') == 'touch' for event in events)
        try:
            self.until(dispatched)
            self.until(lambda: self.request('/input/status?input_id=' + str(input_id))[1]['data'].get('delivered_frame'))
        finally:
            self.request('/input/flush', 'POST', dict(owner, release=True))

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
        status, response = self.request('/observations/' + urllib.parse.quote(snapshot_id, safe=''))
        self.assertEqual((410, 'stale_snapshot'), (status, response['error']['code']))
        status, response = self.request('/frame', headers={'X-Automation-Runtime': old_runtime})
        self.assertEqual((410, 'stale_runtime'), (status, response['error']['code']))
        self.assertEqual(200, self.request('/frame')[0])

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--engine', required=True)
    parser.add_argument('--extension', action='store_true')
    parser.add_argument('--fixture', required=True)
    ARGS, remaining = parser.parse_known_args()
    unittest.main(argv=[__file__] + remaining)
