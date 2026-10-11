import os
from pathlib import Path
import subprocess
import sys
import urllib.request


# Use the same pinned Microsoft installers as setup-dotnet v6.0.0, without its
# separate installation of the latest LTS runtime. The SDK includes its runtime.
INSTALLER_REVISION = 'a98b56852c35b8e3190ac28c8c2271da59106c68'


def main():
    version = os.environ['DOTNET_SDK_VERSION']
    install_dir = Path(os.environ['DOTNET_INSTALL_DIR']).resolve()
    windows = sys.platform == 'win32'
    executable = install_dir / ('dotnet.exe' if windows else 'dotnet')
    if not executable.is_file() or not (install_dir / 'sdk' / version / 'dotnet.dll').is_file():
        script_name = 'install-dotnet.ps1' if windows else 'install-dotnet.sh'
        script = Path(os.environ['RUNNER_TEMP']) / script_name
        url = f'https://raw.githubusercontent.com/actions/setup-dotnet/{INSTALLER_REVISION}/externals/{script_name}'
        urllib.request.urlretrieve(url, script)
        if windows:
            command = ['pwsh', '-NoLogo', '-NoProfile', '-NonInteractive',
                       '-ExecutionPolicy', 'Unrestricted', '-File', str(script),
                       '-Version', version, '-InstallDir', str(install_dir), '-NoPath']
        else:
            command = ['bash', str(script), '--version', version,
                       '--install-dir', str(install_dir), '--no-path']
        subprocess.run(command, check=True)

    with open(os.environ['GITHUB_PATH'], 'a', encoding='utf-8') as output:
        output.write(f'{install_dir}\n')
    with open(os.environ['GITHUB_ENV'], 'a', encoding='utf-8') as output:
        output.write(f'DOTNET_ROOT={install_dir}\n')
    print(f'Using .NET SDK {version} in {install_dir}')


if __name__ == '__main__':
    main()
