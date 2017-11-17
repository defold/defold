import optparse, os, shutil, stat, sys, tarfile, tempfile

def _get_s3_bucket(bucket_name):
    from ConfigParser import ConfigParser
    config = ConfigParser()
    configpath = os.path.expanduser("~/.s3cfg")
    config.read(configpath)

    key = config.get('default', 'access_key')
    secret = config.get('default', 'secret_key')

    if not (key and secret):
        self._log('key/secret not found in "%s"' % configpath)
        sys.exit(5)

    from boto.s3.connection import S3Connection
    from boto.s3.connection import OrdinaryCallingFormat
    from boto.s3.key import Key

    # NOTE: We hard-code host (region) here and it should not be required.
    # but we had problems with certain buckets with period characters in the name.
    # Probably related to the following issue https://github.com/boto/boto/issues/621
    conn = S3Connection(key, secret, host='s3-eu-west-1.amazonaws.com', calling_format=OrdinaryCallingFormat())
    bucket = conn.get_bucket(bucket_name)
    return bucket

def _set_executable(path):
    os.chmod(path, stat.S_IXUSR | os.stat(dst)[stat.ST_MODE])

if __name__ == '__main__':
    boto_path = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '../packages/boto-2.28.0-py2.7.egg'))
    sys.path.insert(0, boto_path)
    usage = '''usage: %prog --target <target> --git-sha1 <sha1> --version <version>'''
    parser = optparse.OptionParser(usage)

    parser.add_option('--target', dest='target',
                      default = None,
                      help = 'Target name, e.g. apkc')

    parser.add_option('--git-sha1', dest='git_sha1',
                      default = None,
                      help = 'Git revision check sum, must exist on S3')

    parser.add_option('--version', dest='version',
                      default = None,
                      help = 'Version when naming the package')

    options, all_args = parser.parse_args()

    if not options.git_sha1 or not options.version:
        parser.print_help()
        sys.exit(1)
    target = options.target
    git_sha1 = options.git_sha1
    version = options.version

    b = _get_s3_bucket('d.defold.com')
    prefix = 'archive/%s/go/' % git_sha1
    root = tempfile.mkdtemp()
    len_p = len(prefix)
    for k in b.list(prefix=prefix):
        platform, curr_target = k.name[len_p:].split('/')
        if target in curr_target:
            dst = os.path.join(root, '%s-%s-%s' % (target, options.version, platform), 'bin', platform, curr_target)
            os.makedirs(os.path.dirname(dst))
            print('  Downloading %s for %s to %s' % (curr_target, platform, dst))
            k.get_contents_to_filename(dst)
            if ('darwin' in platform) or ('linux' in platform):
                _set_executable(dst)
            print('  Done!')
    for d in os.listdir(root):
        arc_path = 'packages/%s.tar.gz' % d
        with tarfile.open(arc_path, 'w:gz') as tar:
            d_path = os.path.join(root, d)
            for subd in os.listdir(d_path):
                tar.add(os.path.join(d_path, subd), arcname=subd)
        print('Wrote archive %s' % arc_path)
    shutil.rmtree(root)
    print('Done')
