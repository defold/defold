# Native window browser tests

These tests compile the real `platform` backend into WebAssembly and drive it
through Chromium. They cover rendering, high-DPI mouse coordinates, keyboard
and focus callbacks, resize, window recreation, fullscreen, and pointer lock.
They currently support `wasm-web` only.

After setting up the web SDK and engine dependencies as described in
[`scripts/cmake/README.md`](../../../../../scripts/cmake/README.md), run from the
repository root:

```sh
npm ci --prefix engine/platform/src/test/browser
cmake -S . -B engine/build/wasm-web -G Ninja -DTARGET_PLATFORM=wasm-web -DBUILD_TESTS=ON
cmake --build engine/build/wasm-web --target test_native_web_browser
```

Run the full suite on Linux, as in CI:

```sh
npm exec --prefix engine/platform/src/test/browser -- playwright install --with-deps --no-shell chromium
xvfb-run --auto-servernum cmake --build engine/build/wasm-web --target run_test_native_web_browser
```

The browser runs with a visible window so fullscreen and pointer lock use real
user gestures. Chromium's software renderer avoids requiring a hardware GPU.
Automated pointer lock can fail in Chromium on macOS; see
[Playwright issue #20956](https://github.com/microsoft/playwright/issues/20956).
To run the compiled fixture from macOS, use the matching Linux container:

```sh
docker run --rm --init --ipc=host --platform linux/amd64 \
  -v "$PWD/engine/platform/src/test/browser:/tests:ro" \
  -v "$PWD/engine/platform/build/wasm-web/src/test:/wasm:ro" \
  -v "$PWD/engine/platform/build/wasm-web/browser-results:/results" \
  -w /tests -e CI=1 \
  -e DEFOLD_BROWSER_TEST_JS=/wasm/test_native_web_browser.js \
  -e PLAYWRIGHT_HTML_OUTPUT_DIR=/results/playwright-report \
  mcr.microsoft.com/playwright:v1.63.0-jammy \
  xvfb-run --auto-servernum node node_modules/@playwright/test/cli.js test --output=/results/test-results
```

The test server binds to a random loopback port and serves only the HTML fixture
and the compiled JavaScript/WebAssembly pair selected by CMake. Browser tests
have an explicit run target so ordinary `run_tests` does not require Playwright.
Missing dependencies, browser errors, assertion failures and timeouts fail the
run; the tests are not silently skipped.

CI runs the regular `wasm-web` tests during the engine build, then invokes this
browser target in a separate Chromium/Xvfb step. It uploads the Playwright
report, browser console logs, and failure screenshots/traces. Local reports
are in `playwright-report/` and `test-results/` alongside this file.
