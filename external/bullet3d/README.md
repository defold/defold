#

Build steps:

* Download archive
* Delete demos, extras etc.
* Convert line endings
* Apply patch
* Configure waf
* Build
* Package

Example:

    $ sh install.sh
    $ waf configure --platform=x86_64-win32
    $ waf install

It will install to `<defold>\packages\bullet-2.77-x86_64-win32.tar.gz`
