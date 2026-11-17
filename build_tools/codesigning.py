from contextlib import contextmanager
from datetime import datetime
import hashlib
import json
import math
import os
import optparse
import run
import sys
import shutil
import subprocess
import tempfile
import time
import zipfile

_SIGNING_SESSION_ENV = 'DEFOLD_SIGNING_SESSION'
_TOKEN_REFRESH_MARGIN = 300


@contextmanager
def signing_session():
    # Child Ninja/linker processes inherit this directory. Keep credentials out
    # of DYNAMO_HOME and the checkout, which may be archived or cached by CI.
    existing = os.environ.get(_SIGNING_SESSION_ENV)
    if existing and os.path.isdir(existing):
        yield existing
        return
    with tempfile.TemporaryDirectory(prefix='defold-signing-', dir=os.environ.get('RUNNER_TEMP')) as directory:
        os.environ[_SIGNING_SESSION_ENV] = directory
        try:
            yield directory
        finally:
            if existing is None:
                os.environ.pop(_SIGNING_SESSION_ENV, None)
            else:
                os.environ[_SIGNING_SESSION_ENV] = existing


def log(msg):
    print(msg)
    sys.stdout.flush()
    sys.stderr.flush()

def _platform_is_macos(platform):
    return platform.endswith("-macos")

def _platform_is_windows(platform):
    return platform.endswith("-win32")

def _platform_is_linux(platform):
    return platform.endswith("-linux")

class ExclusiveFileLock:
    def __init__(self, path):
        self.path = path
        self.file = None

    def __enter__(self):
        os.makedirs(os.path.dirname(self.path), exist_ok=True)
        self.file = open(self.path, 'a+')
        if os.name == 'nt':
            import msvcrt
            while True:
                try:
                    self.file.seek(0)
                    msvcrt.locking(self.file.fileno(), msvcrt.LK_LOCK, 1)
                    break
                except OSError:
                    time.sleep(1)
        else:
            import fcntl
            fcntl.flock(self.file.fileno(), fcntl.LOCK_EX)
        return self

    def __exit__(self, _exc_type, _exc_value, _traceback):
        if os.name == 'nt':
            import msvcrt
            self.file.seek(0)
            msvcrt.locking(self.file.fileno(), msvcrt.LK_UNLCK, 1)
        else:
            import fcntl
            fcntl.flock(self.file.fileno(), fcntl.LOCK_UN)
        self.file.close()

def _codesigning_lock_path():
    return os.path.join(os.environ.get('DYNAMO_HOME', tempfile.gettempdir()), 'codesigning.lock')

def mac_certificate(codesigning_identity):
    if run.command(['security', 'find-identity', '-p', 'codesigning', '-v']).find(codesigning_identity) >= 0:
        return codesigning_identity.replace("\"", "")
    else:
        return None


def _read_signing_token(filename):
    try:
        with open(filename, encoding='utf-8') as stream:
            token = json.load(stream)
        if (isinstance(token['access_token'], str) and token['access_token']
                and math.isfinite(token['expires_at'])):
            return token
    except (OSError, ValueError, KeyError, TypeError):
        pass
    return None


def _write_signing_token(filename, token):
    # mkstemp creates a private file; replace atomically so interrupted writes
    # cannot leave a partial credential for the next signer.
    descriptor, temporary = tempfile.mkstemp(dir=os.path.dirname(filename), prefix='token-')
    try:
        with os.fdopen(descriptor, 'w', encoding='utf-8') as stream:
            json.dump(token, stream)
        os.replace(temporary, filename)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def _windows_signing_token(options, gcloud, env, timings):
    with open(options.gcloud_keyfile, 'rb') as stream:
        key = stream.read()
    account = json.loads(key)['client_email']
    # A key rotation or a different signing context must not reuse old credentials.
    context = [options.gcloud_projectid, options.gcloud_location,
               options.gcloud_keyringname, options.gcloud_keyname]
    fingerprint = hashlib.sha256(key + json.dumps(context).encode('utf-8')).hexdigest()
    with signing_session() as directory:
        filename = os.path.join(directory, fingerprint + '.json')
        lock_started = time.monotonic()
        # gcloud.CMD invocations can collide on a shared temporary file. Check
        # the cache while holding the same lock used for authentication/refresh.
        with ExclusiveFileLock(_codesigning_lock_path()):
            timings['lock_wait'] = time.monotonic() - lock_started
            token = _read_signing_token(filename)
            if token and token['expires_at'] > time.time() + _TOKEN_REFRESH_MARGIN:
                return token['access_token']
            if token is None:
                started = time.monotonic()
                run.env_command(env, [gcloud, 'auth', 'activate-service-account',
                                      '--key-file', options.gcloud_keyfile], silent=True)
                timings['auth'] = time.monotonic() - started
            started = time.monotonic()
            # config-helper supplies the real expiry and refreshes credentials;
            # --account avoids relying on the mutable active gcloud account.
            result = subprocess.run(
                [gcloud, 'config', 'config-helper', '--account', account,
                 '--force-auth-refresh', '--format=json(credential)'],
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
                text=True, env=env)
            timings['token'] = time.monotonic() - started
            if result.returncode:
                # Neither stdout nor stderr is safe to print for a token command.
                sys.exit('gcloud signing token refresh failed with exit code %d' % result.returncode)
            try:
                # Ignore launcher warnings before the structured JSON response.
                credential = json.loads(result.stdout[result.stdout.index('{'):])['credential']
                expiry = datetime.fromisoformat(credential['token_expiry'].replace('Z', '+00:00'))
                access_token = credential['access_token']
                if not isinstance(access_token, str) or not access_token or expiry.tzinfo is None:
                    raise ValueError('Invalid credential')
                token = {'access_token': access_token, 'expires_at': expiry.timestamp()}
                if token['expires_at'] <= time.time() + _TOKEN_REFRESH_MARGIN:
                    raise ValueError('Credential expires too soon')
            except (ValueError, KeyError, TypeError, AttributeError):
                sys.exit('gcloud returned invalid or expiring signing credentials')
            _write_signing_token(filename, token)
            return token['access_token']


def sign_windows_file(options, file):
    started = time.monotonic()
    gcloud = shutil.which('gcloud')
    if not gcloud:
        sys.exit("No gcloud tool found")

    # for some reason the following can be seen in the CI output even if Python is
    # installed:
    # To use the Google Cloud CLI, you must have Python installed and on your PATH.
    # As an alternative, you may also set the CLOUDSDK_PYTHON environment variable
    # to the location of your Python executable.
    #
    # Docs: https://docs.cloud.google.com/sdk/gcloud/reference/topic/startup
    env = os.environ.copy()
    env['CLOUDSDK_PYTHON'] = sys.executable

    timings = {'lock_wait': 0, 'auth': 0, 'token': 0}
    storepass = _windows_signing_token(options, gcloud, env, timings)

    jsign = os.path.join(os.environ['DYNAMO_HOME'], 'ext','share','java','jsign-4.2.jar')
    keystore = "projects/%s/locations/%s/keyRings/%s" % (options.gcloud_projectid, options.gcloud_location, options.gcloud_keyringname)
    sign_started = time.monotonic()
    run.command([
        'java', '-jar', jsign,
        '--storetype', 'GOOGLECLOUD',
        '--storepass', storepass,
        '--keystore', keystore,
        '--alias', options.gcloud_keyname,
        '--certfile', options.gcloud_certfile,
        '--tsmode', 'RFC3161',
        '--tsaurl', 'http://timestamp.globalsign.com/tsa/r6advanced1',
        file], silent = True, redact_flags=['--storepass'])
    finished = time.monotonic()
    if getattr(options, 'timings', False):
        log("Codesigning timing %s: lock_wait=%.3fs auth=%.3fs token=%.3fs sign_and_timestamp=%.3fs total=%.3fs" % (
            os.path.basename(file), timings['lock_wait'],
            timings['auth'], timings['token'],
            finished - sign_started, finished - started))

def _sign_macos(options, file, extra_args):
    codesigning_identity = options.codesigning_identity
    certificate = mac_certificate(codesigning_identity)
    if certificate is None:
        log("Codesigning certificate not found for signing identity %s" % codesigning_identity)
        sys.exit(1)

    args = ['codesign', '--force', '--timestamp', '--sign', certificate] + extra_args + [file]
    env = os.environ.copy()
    max_attempts = 5
    for attempt in range(max_attempts):
        try:
            # run.command exits on failure before the timestamp error can be retried.
            run.env_command(env, args)
            return
        except run.ExecException as error:
            if 'The timestamp service is not available.' not in error.output or attempt + 1 == max_attempts:
                sys.exit(error.retcode)
            delay = 5 * (2 ** attempt)
            log("Timestamp service unavailable while signing %s. Retrying in %d seconds (attempt %d/%d)." % (
                file, delay, attempt + 2, max_attempts))
            time.sleep(delay)

def sign_macos_file(options, file):
    if not options.codesigning_entitlements:
        log("No entitlements specified for signing %s" % file)
        sys.exit(1)

    _sign_macos(options, file, [
        '--deep',
        '--options', 'runtime',
        '--entitlements', options.codesigning_entitlements])

def sign_macos_dmg(options, file):
    _sign_macos(options, file, [])

def sign_file(platform, options, file):
    if _platform_is_windows(platform):
        sign_windows_file(options, file)

    if _platform_is_macos(platform):
        sign_macos_file(options, file)


def _main():
    parser = optparse.OptionParser()
    parser.add_option('--platform', dest='platform', help='Target platform')
    parser.add_option('--file', dest='file', help='File to sign')
    parser.add_option('--timings', action='store_true', default=False,
                      help='Report Windows signing stage durations')
    parser.add_option('--codesigning-identity', dest='codesigning_identity',
                      default='Developer ID Application: Stiftelsen Defold Foundation (26PW6SVA7H)',
                      help='Codesigning identity for macOS')
    parser.add_option('--gcloud-projectid', dest='gcloud_projectid',
                      help='Google Cloud project id where key ring is stored')
    parser.add_option('--gcloud-location', dest='gcloud_location',
                      help='Google Cloud region where key ring is located')
    parser.add_option('--gcloud-keyringname', dest='gcloud_keyringname',
                      help='Google Cloud key ring name')
    parser.add_option('--gcloud-keyname', dest='gcloud_keyname',
                      help='Google Cloud key name')
    parser.add_option('--gcloud-certfile', dest='gcloud_certfile',
                      help='Google Cloud certificate chain file')
    parser.add_option('--gcloud-keyfile', dest='gcloud_keyfile',
                      help='Google Cloud service account key file')
    parser.add_option('--codesigning_entitlements', dest='codesigning_entitlements',
                      help='Apple codesigning entitlements')

    options, _args = parser.parse_args()
    if not options.platform:
        parser.error('--platform is required')
    if not options.file:
        parser.error('--file is required')

    sign_file(options.platform, options, options.file)


if __name__ == '__main__':
    _main()
