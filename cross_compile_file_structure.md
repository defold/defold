# Cross-Compile File Structure Findings

Roots from `.defold-platforms`: PS4/PS5 -> `/Users/mathiaswesterdahl/work/defold-ps4`, Xbox -> `/Users/mathiaswesterdahl/work/defold-xbox`, NX64 -> `/Users/mathiaswesterdahl/work/defold-switch`.

PS4 and PS5 currently share files, but should stay separate because they may diverge. `js-web` / `jsweb` is old and removed; ignore stale files such as `dlib/js/socket_jsweb.cpp`. Active web is `wasm-web` using `dlib/js/socket_web.cpp`.

Platform comments omit architecture when the pattern applies to the whole platform family.

## Feature Trees

```text
condition_variable: done
    dlib/condition_variable_posix.cpp       # android, linux, darwin, ios
    dlib/condition_variable_win32.cpp       # win32, xbone
    dlib/condition_variable_web.cpp         # web
    dlib/condition_variable_switch.cpp      # nx64
    dlib/condition_variable_ps4.cpp         # ps4
    dlib/condition_variable_ps5.cpp         # ps5

crypt: done
    dlib/crypt.cpp                          # all active platforms
    dlib/crypt_mbedtls.cpp                  # android, linux, win32, web, ps4, ps5, xbone, nx64
    dlib/crypt_apple.mm                     # darwin, ios

file_descriptor: done
    dlib/file_descriptor_posix.cpp          # android, linux, darwin, ios, web
    dlib/file_descriptor_win32.cpp          # win32, xbone
    dlib/file_descriptor_switch.cpp         # nx64
    dlib/file_descriptor_ps4.cpp            # ps4
    dlib/file_descriptor_ps5.cpp            # ps5

mutex: done
    dlib/mutex_posix.cpp                    # android, linux, darwin, ios
    dlib/mutex_win32.cpp                    # win32, xbone
    dlib/mutex_web.cpp                      # web
    dlib/mutex_switch.cpp                   # nx64
    dlib/mutex_ps4.cpp                      # ps4
    dlib/mutex_ps5.cpp                      # ps5

socket: done
    dlib/socket.cpp                         # all active platforms
    dlib/socket_android.cpp                 # android
    dlib/socket_linux.cpp                   # linux
    dlib/socket_apple.mm                    # darwin, ios
    dlib/socket_win32.cpp                   # win32, xbone
    dlib/socket_web.cpp                     # web
    dlib/socket_ps4.cpp                     # ps4
    dlib/socket_ps5.cpp                     # ps5
    dlib/socket_switch.cpp                  # nx64

sslsocket: done
    dlib/sslsocket_mbedtls.cpp              # android, linux, win32
    dlib/sslsocket_apple.mm                 # darwin, ios
    dlib/sslsocket_web.cpp                  # web
    dlib/sslsocket_switch.cpp               # nx64
    dlib/sslsocket_ps4.cpp                  # ps4
    dlib/sslsocket_ps5.cpp                  # ps5
    dlib/sslsocket_xbone.cpp                # xbone

sys: done
    dlib/sys.cpp                            # all active platforms
    dlib/sys_posix.cpp                      # android, linux, darwin, ios, web
    dlib/sys_apple.mm                       # darwin, ios
    dlib/sys_win32.cpp                      # win32, xbone
    dlib/sys_ps4.cpp                        # ps4
    dlib/sys_ps5.cpp                        # ps5
    dlib/sys_xbox.cpp                       # xbone
    dlib/sys_switch.cpp                     # nx64

thread: done
    dlib/thread_posix.cpp                   # android, linux, darwin, ios, web
    dlib/thread_win32.cpp                   # win32, xbone
    dlib/thread_switch.cpp                  # nx64
    dlib/thread_ps4.cpp                     # ps4
    dlib/thread_ps5.cpp                     # ps5

time: done
    dlib/time_posix.cpp                     # android, linux, web
    dlib/time_apple.cpp                     # darwin, ios
    dlib/time_win32.cpp                     # win32, xbone
    dlib/time_switch.cpp                    # nx64
    dlib/time_ps4.cpp                       # ps4
    dlib/time_ps5.cpp                       # ps5

```

## Main Inconsistencies

* Xbox mostly reuses Win32 files and adds `dlib/sys_xbox.cpp`.
TODO: "Xbox mostly reuses Win32 files". This is fine, and the `waf_dynamo.find_feature_files()` should take that into consideration.

## Suggested Direction

Use one predictable filename pattern per feature:

```text
dlib/<feature>.cpp               # shared/core code
dlib/<feature>_<platform>.cpp    # platform-only implementation
dlib/<feature>_<product>.cpp     # product-family implementation
dlib/<feature>_<vendor>.cpp      # vendor-family implementation
dlib/<feature>_default.cpp       # default implementation
dlib/<feature>_null.cpp          # intentionally disabled/no-op implementation
```

Prefer descriptive backend names when multiple platforms share an implementation: `posix`, `win32`, `darwin`, `mbedtls`, `null`. Use exact platform names only when the implementation is truly platform-specific: `ps4`, `ps5`, `xbone`, `nx64`, `web`.

Avoid `fallback` as a general suffix. It reads like a last-resort path rather than a supported implementation. Better names:

* `default` for the ordinary implementation selected when no more specific file exists.
* `generic` for portable code with no platform assumptions.
* `posix`, `win32`, `darwin`, etc. when the dependency is really an OS/backend contract.
* `null` or `stub` for deliberately reduced behavior.

## Proposed Resolver

Add a helper that expands a known core file into optional implementation files based on platform metadata:

```python
waf_dynamo.find_file(
    ['dlib/condition_variable.cpp'],
    bld.env.PLATFORM,
    ['dlib/condition_variable_default.cpp'])
```

For `x86_64-ps5`, it should try:

```text
dlib/condition_variable.cpp                  # add if it exists
dlib/condition_variable_ps5.cpp              # platform
dlib/condition_variable_playstation.cpp      # product
dlib/condition_variable_sony.cpp             # vendor
```

If any platform/product/vendor implementation file exists, do not add the default file. For `arm64-macos`, if no specific file exists, add:

```text
dlib/condition_variable.cpp
dlib/condition_variable_default.cpp
```

Suggested lookup order:

```text
platform -> product -> vendor -> default
```

This keeps the source names easy to find in an IDE while allowing shared implementations for product families such as `xbox` or `switch`, and vendor families such as `apple`, `sony`, or `microsoft`.
