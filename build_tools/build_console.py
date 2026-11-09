"""Terminal colors shared by build commands, progress, and timing summaries."""

import os
import shlex
import subprocess
import sys


STEP_PREFIX = 'DEFOLD - '


class Style:
    """Build output roles mapped to ANSI SGR parameters in one place."""

    INFO = '36'
    PREFIX = INFO
    HEADING = '1;36'
    COMPONENT = '1;38;5;130'
    SUCCESS = '32'
    SUCCESS_HEADING = '1;32'
    ERROR = '31'
    ERROR_HEADING = '1;31'
    WARNING = '33'
    FAILED_CHECK = '38;5;208'
    COMMAND = COMPONENT
    ENVIRONMENT = '1;38;5;29'
    OPTION = INFO


def use_colors(stream=None, env=None):
    stream = sys.stdout if stream is None else stream
    env = os.environ if env is None else env
    return (stream.isatty() and 'NO_COLOR' not in env
            and not env.get('NOCOLOR') and env.get('CLICOLOR') != '0')


def colorize(message, code, enabled=None):
    if enabled is None:
        enabled = use_colors()
    return f'\033[{code}m{message}\033[0m' if enabled else message


def format_step(message, code=Style.INFO, colorize_fn=None, width=None):
    paint = colorize_fn or colorize
    prefix = STEP_PREFIX
    if width is not None:
        prefix = prefix[:width]
        message = message[:max(0, width - len(prefix))]
    return paint(prefix, Style.PREFIX) + (paint(message, code) if code else message)


def format_shell_command(args, environment=None, colorize_fn=None):
    """Format structured shell-style output without parsing or executing it."""
    paint = colorize_fn or colorize

    def quote(value):
        value = str(value)
        return subprocess.list2cmdline([value]) if os.name == 'nt' else shlex.quote(value)

    parts = []
    if environment:
        parts.append(paint('env', Style.COMMAND))
        for name, value in environment.items():
            parts.append(paint(str(name), Style.ENVIRONMENT) + '=' + quote(value))
    for index, arg in enumerate(args):
        arg = str(arg)
        if index == 0:
            parts.append(paint(quote(arg), Style.COMMAND))
        elif arg.startswith('-'):
            flag, separator, value = arg.partition('=')
            parts.append(paint(quote(flag), Style.OPTION) + (separator + quote(value) if separator else ''))
        else:
            parts.append(quote(arg))
    return ' '.join(parts)
