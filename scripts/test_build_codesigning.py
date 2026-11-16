import contextlib
from datetime import datetime, timezone
import io
import json
import multiprocessing
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'build_tools'))
import codesigning


def credential_response(token, expires_at):
    credential = {'access_token': token,
                  'token_expiry': datetime.fromtimestamp(expires_at, timezone.utc).isoformat()}
    return SimpleNamespace(returncode=0, stdout=json.dumps({'credential': credential}), stderr='')


def concurrent_signer(options, events, authentication_started=None, release_authentication=None,
                      lock_attempted=None):
    original_enter = codesigning.ExclusiveFileLock.__enter__

    def enter(lock):
        if lock_attempted is not None:
            lock_attempted.set()
        return original_enter(lock)

    def authenticate(*args, **kwargs):
        events.put(('auth', None))
        if authentication_started is not None:
            authentication_started.set()
            if not release_authentication.wait(20):
                raise AssertionError('Authentication was not released')

    def token(*args, **kwargs):
        events.put(('token', None))
        return credential_response('concurrent-test-token', codesigning.time.time() + 3600)

    def sign(args, **kwargs):
        events.put(('sign', args[args.index('--storepass') + 1]))

    with mock.patch.object(codesigning.ExclusiveFileLock, '__enter__', enter), \
            mock.patch.object(codesigning.shutil, 'which', return_value='gcloud'), \
            mock.patch.object(codesigning.run, 'env_command', side_effect=authenticate), \
            mock.patch.object(codesigning.subprocess, 'run', side_effect=token), \
            mock.patch.object(codesigning.run, 'command', side_effect=sign):
        codesigning.sign_windows_file(SimpleNamespace(**options), 'test.exe')


class WindowsSigningTests(unittest.TestCase):
    def setUp(self):
        self.directory = Path(self.enterContext(tempfile.TemporaryDirectory()))
        self.keyfile = self.directory / 'key.json'
        self.keyfile.write_text(json.dumps({'client_email': 'signer@example.invalid',
                                            'private_key_id': 'test-key'}), encoding='utf-8')
        self.options = SimpleNamespace(gcloud_keyfile=str(self.keyfile), gcloud_projectid='project',
                                       gcloud_location='region', gcloud_keyringname='ring',
                                       gcloud_keyname='key', gcloud_certfile='certificate.pem', timings=True)
        self.enterContext(mock.patch.dict(os.environ, {
            'DYNAMO_HOME': str(self.directory / 'build'), 'RUNNER_TEMP': str(self.directory)}))
        os.environ.pop(codesigning._SIGNING_SESSION_ENV, None)

    def mock_commands(self, responses):
        self.enterContext(mock.patch.object(codesigning.shutil, 'which', return_value='gcloud'))
        auth = self.enterContext(mock.patch.object(codesigning.run, 'env_command'))
        token = self.enterContext(mock.patch.object(codesigning.subprocess, 'run', side_effect=responses))
        sign = self.enterContext(mock.patch.object(codesigning.run, 'command'))
        return auth, token, sign

    # Separate binaries in one build reuse credentials but retain their own signatures and timestamps.
    def test_session_reuses_credentials_for_multiple_files(self):
        now = 1800000000
        auth, token, sign = self.mock_commands([credential_response('test-access-token', now + 3600)])
        with mock.patch.object(codesigning.time, 'time', return_value=now), \
                codesigning.signing_session() as directory, contextlib.redirect_stdout(io.StringIO()) as output:
            for file in ('one.dll', 'two.exe', 'three.exe'):
                codesigning.sign_windows_file(self.options, file)
            self.assertFalse(Path(directory).is_relative_to(self.directory / 'build'))
            self.assertEqual(1, len(list(Path(directory).glob('*.json'))))
            for call in sign.call_args_list:
                args = call.args[0]
                self.assertEqual('test-access-token', args[args.index('--storepass') + 1])
                self.assertEqual('RFC3161', args[args.index('--tsmode') + 1])
                self.assertEqual(['--storepass'], call.kwargs['redact_flags'])
            self.assertNotIn('test-access-token', output.getvalue())
        self.assertFalse(Path(directory).exists())
        self.assertNotIn(codesigning._SIGNING_SESSION_ENV, os.environ)
        auth.assert_called_once()
        token.assert_called_once()
        self.assertIn('--force-auth-refresh', token.call_args.args[0])
        self.assertIn('signer@example.invalid', token.call_args.args[0])
        self.assertEqual(['one.dll', 'two.exe', 'three.exe'], [c.args[0][-1] for c in sign.call_args_list])

    # Near-expiry tokens are refreshed from the authenticated account without importing the key again.
    def test_refreshes_token_before_expiry_without_reauthenticating(self):
        now = 1800000000
        auth, token, sign = self.mock_commands([
            credential_response('first-token', now + 3600),
            credential_response('refreshed-token', now + 7200)])
        with mock.patch.object(codesigning.time, 'time', return_value=now) as clock, \
                codesigning.signing_session(), contextlib.redirect_stdout(io.StringIO()):
            codesigning.sign_windows_file(self.options, 'one.exe')
            clock.return_value = now + 3400
            codesigning.sign_windows_file(self.options, 'two.exe')
        auth.assert_called_once()
        self.assertEqual(2, token.call_count)
        args = sign.call_args.args[0]
        self.assertEqual('refreshed-token', args[args.index('--storepass') + 1])

    # Rotating a service-account key at the same path invalidates the cached authentication context.
    def test_key_rotation_does_not_reuse_old_token(self):
        now = 1800000000
        auth, token, sign = self.mock_commands([
            credential_response('first-token', now + 3600),
            credential_response('rotated-token', now + 3600)])
        with mock.patch.object(codesigning.time, 'time', return_value=now), \
                codesigning.signing_session(), contextlib.redirect_stdout(io.StringIO()):
            codesigning.sign_windows_file(self.options, 'one.exe')
            self.keyfile.write_text(json.dumps({'client_email': 'signer@example.invalid',
                                                'private_key_id': 'rotated-key'}), encoding='utf-8')
            codesigning.sign_windows_file(self.options, 'two.exe')
        self.assertEqual(2, auth.call_count)
        self.assertEqual(2, token.call_count)
        args = sign.call_args.args[0]
        self.assertEqual('rotated-token', args[args.index('--storepass') + 1])

    # An interrupted or corrupt cache is rebuilt instead of making a valid build fail to sign.
    def test_corrupt_cache_is_replaced(self):
        now = 1800000000
        auth, token, sign = self.mock_commands([
            credential_response('first-token', now + 3600),
            credential_response('recovered-token', now + 3600)])
        with mock.patch.object(codesigning.time, 'time', return_value=now), \
                codesigning.signing_session() as directory, contextlib.redirect_stdout(io.StringIO()):
            codesigning.sign_windows_file(self.options, 'one.exe')
            cache = next(Path(directory).glob('*.json'))
            cache.write_text('{partial', encoding='utf-8')
            codesigning.sign_windows_file(self.options, 'two.exe')
            self.assertEqual('recovered-token', json.loads(cache.read_text())['access_token'])
        self.assertEqual(2, auth.call_count)
        self.assertEqual(2, sign.call_count)

    # Failed token commands must not sign or leak token-bearing output into logs or error messages.
    def test_token_command_failure_does_not_expose_credentials(self):
        secret = 'do-not-print-this-token'
        response = SimpleNamespace(returncode=7, stdout=secret, stderr=secret)
        auth, token, sign = self.mock_commands([response])
        with codesigning.signing_session() as directory, contextlib.redirect_stdout(io.StringIO()) as output, \
                contextlib.redirect_stderr(io.StringIO()) as error, self.assertRaises(SystemExit) as raised:
            codesigning.sign_windows_file(self.options, 'test.exe')
        self.assertNotIn(secret, str(raised.exception) + output.getvalue() + error.getvalue())
        sign.assert_not_called()
        self.assertFalse(Path(directory).exists())

    # Missing, malformed and already-expiring credentials cannot become reusable signing tokens.
    def test_invalid_credentials_fail_without_logging_response(self):
        now = 1800000000
        invalid = [
            'not-json-secret',
            json.dumps({'credential': {'access_token': 'secret', 'token_expiry': 'secret'}}),
            credential_response('secret', now + 60).stdout,
            json.dumps({'credential': {'access_token': '', 'token_expiry': '2030-01-01T00:00:00Z'}}),
        ]
        for response in invalid:
            with self.subTest(response=response), mock.patch.object(codesigning.time, 'time', return_value=now), \
                    mock.patch.object(codesigning.shutil, 'which', return_value='gcloud'), \
                    mock.patch.object(codesigning.run, 'env_command'), \
                    mock.patch.object(codesigning.subprocess, 'run', return_value=SimpleNamespace(
                        returncode=0, stdout=response, stderr='')), \
                    mock.patch.object(codesigning.run, 'command') as sign, \
                    codesigning.signing_session() as directory, self.assertRaises(SystemExit) as raised:
                codesigning.sign_windows_file(self.options, 'test.exe')
            self.assertNotIn('secret', str(raised.exception))
            sign.assert_not_called()
            self.assertFalse(Path(directory).exists())

    # Direct API callers get a temporary session too, and launcher warning lines do not mask valid JSON.
    def test_standalone_signer_handles_launcher_warning_and_cleans_up(self):
        now = 1800000000
        response = credential_response('test-token', now + 3600)
        response.stdout = 'Windows launcher warning\n' + response.stdout
        auth, token, sign = self.mock_commands([response])
        with mock.patch.object(codesigning.time, 'time', return_value=now), contextlib.redirect_stdout(io.StringIO()):
            codesigning.sign_windows_file(self.options, 'test.exe')
        sign.assert_called_once()
        self.assertEqual([], list(self.directory.glob('defold-signing-*')))
        self.assertNotIn(codesigning._SIGNING_SESSION_ENV, os.environ)

    # Nested build/signing processes borrow the parent's credential directory without deleting it.
    def test_nested_session_preserves_parent_and_cleans_up_on_failure(self):
        with self.assertRaisesRegex(RuntimeError, 'build failed'):
            with codesigning.signing_session() as parent:
                with codesigning.signing_session() as child:
                    self.assertEqual(parent, child)
                self.assertTrue(Path(parent).is_dir())
                raise RuntimeError('build failed')
        self.assertFalse(Path(parent).exists())

    # A saved build environment may point to an earlier, already-cleaned-up session.
    def test_stale_inherited_session_is_replaced(self):
        stale = str(self.directory / 'old-session')
        with mock.patch.dict(os.environ, {codesigning._SIGNING_SESSION_ENV: stale}):
            with codesigning.signing_session() as directory:
                self.assertNotEqual(stale, directory)
                self.assertTrue(Path(directory).is_dir())
                self.assertEqual(directory, os.environ[codesigning._SIGNING_SESSION_ENV])
            self.assertFalse(Path(directory).exists())
            self.assertEqual(stale, os.environ[codesigning._SIGNING_SESSION_ENV])

    # Concurrent Ninja signer processes must import the key and refresh the token only once.
    def test_concurrent_processes_share_one_authentication(self):
        context = multiprocessing.get_context('spawn')
        events = context.Queue()
        authentication_started, release_authentication, lock_attempted = (context.Event() for _ in range(3))
        children = []
        with codesigning.signing_session():
            try:
                first = context.Process(target=concurrent_signer, args=(
                    vars(self.options), events, authentication_started, release_authentication))
                first.start()
                children.append(first)
                self.assertTrue(authentication_started.wait(20), 'First signer did not authenticate')
                second = context.Process(target=concurrent_signer, args=(
                    vars(self.options), events, None, None, lock_attempted))
                second.start()
                children.append(second)
                self.assertTrue(lock_attempted.wait(20), 'Second signer did not attempt the authentication lock')
                release_authentication.set()
                for child in children:
                    child.join(20)
                    self.assertEqual(0, child.exitcode)
                messages = [events.get(timeout=5) for _ in range(4)]
                self.assertEqual(1, sum(kind == 'auth' for kind, _ in messages))
                self.assertEqual(1, sum(kind == 'token' for kind, _ in messages))
                self.assertEqual(['concurrent-test-token'] * 2,
                                 [value for kind, value in messages if kind == 'sign'])
            finally:
                release_authentication.set()
                for child in children:
                    if child.is_alive():
                        child.kill()
                    child.join()
                events.close()
                events.join_thread()


if __name__ == '__main__':
    unittest.main()
