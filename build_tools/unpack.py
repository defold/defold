from log import log
import os
import run
import platform
import tempfile
import shutil

def zip(filepath, dest_dir):
    run.shell_command('unzip %s -d %s' % (filepath, dest_dir))

def dmg(filepath, dest_dir):
    system = platform.system()
    if system == "Darwin":
        with tempfile.TemporaryDirectory(prefix="defold-dmg-") as mountpoint:
            run.command(["hdiutil", "attach", "-nobrowse", "-readonly", "-mountpoint", mountpoint, filepath])
            try:
                shutil.copytree(os.path.join(mountpoint, "Defold.app"), os.path.join(dest_dir, "Defold.app"))
            finally:
                run.command(["hdiutil", "detach", mountpoint])
    elif system == "Linux":
        mountpoint = tempfile.mkdtemp()
        run.shell_command("mount -t hfsplus %s %s" % (filepath, mountpoint))
        shutil.copytree(os.path.join(mountpoint, "Defold.app"), os.path.join(dest_dir, "Defold.app"))
        run.shell_command("umount %s" % (mountpoint))
    else:
        log("Unpacking .dmg is not supported on %d", system)
