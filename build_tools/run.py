from log import log
import subprocess
import sys

class ExecException(Exception):
    def __init__(self, retcode, output):
        self.retcode = retcode
        self.output = output

def _exec_command(arg_list, **kwargs):
    arg_str = arg_list
    if not isinstance(arg_str, basestring):
        arg_str = ' '.join(arg_list)
    log('[exec] %s' % arg_str)

    if sys.stdout.isatty():
        # If not on CI, we want the colored output, and we get the output as it runs, in order to preserve the colors
        if not 'stdout' in kwargs:
            kwargs['stdout'] = subprocess.PIPE # Only way to get output from the command
        process = subprocess.Popen(arg_list, **kwargs)
        output = process.communicate()[0]
        if process.returncode != 0:
            log(output)
    else:
        # On the CI machines, we make sure we produce a steady stream of output
        # However, this also makes us lose the color information
        if 'stdout' in kwargs:
            del kwargs['stdout']
        process = subprocess.Popen(arg_list, stdout = subprocess.PIPE, stderr = subprocess.STDOUT, **kwargs)

        output = ''
        while True:
            line = process.stdout.readline()
            if line != '':
                output += line
                log(line.rstrip())
            else:
                break

    if process.wait() != 0:
        raise ExecException(process.returncode, output)

    return output

def command(args, **kwargs):
    if kwargs.get("shell") is None:
        kwargs["shell"] = False
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, **kwargs)
    except ExecException, e:
        sys.exit(e.retcode)

def shell_command(args):
    # Executes a command, and exits if it fails
    try:
        return _exec_command(args, shell = True)
    except ExecException, e:
        sys.exit(e.retcode)


def env_command(env, args, **kwargs):
    return _exec_command(args, shell = False, stdout = None, env = env, **kwargs)

def env_shell_command(env, args, **kwargs):
    return _exec_command(args, shell = True, env = env, **kwargs)
