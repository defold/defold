# JSONTestSuite fixtures

The complete `test_parsing` corpus and MIT license are copied without changes
from [nst/JSONTestSuite](https://github.com/nst/JSONTestSuite) at commit
[`1ef36fa01286573e846ac449e8683f8833c5b26a`](https://github.com/nst/JSONTestSuite/tree/1ef36fa01286573e846ac449e8683f8833c5b26a).
The manifest records the upstream Git blob SHA and size of every file. The
runner verifies them before parsing; `.gitattributes` preserves the raw bytes.
No network access or third-party parser is required to run the tests.

The corpus contains 95 `y_` cases (accept), 188 `n_` cases (reject), and 35 `i_`
cases (implementation-defined). Both `PARSE_DECODE_VALUES` and
`PARSE_PRESERVE_VALUES` run against each fixture. The manifest gives explicit,
reasoned expectations for every `i_` case and four `y_` cases where this API
deliberately restricts the JSON grammar:

- Duplicate decoded object keys are rejected in both modes.
- Object keys containing escaped NUL are rejected in both modes because keys
  are exposed as C strings.
- Escaped NUL in values is rejected when decoding and accepted when preserving.
- Invalid UTF-8, unpaired surrogates, UTF-16 input and an initial BOM are rejected.
- Numeric overflow is rejected when decoding and accepted when preserving;
  finite rounded values and underflow to zero are accepted.
- The recursive parser rejects nesting beyond its 64-level guard.

These expectations document the API's behavior, rather than claim full JSON
conformance. Keep the focused unit tests for decoded values, source locations,
buffer reuse, locale behavior, SIMD boundaries and string writing.

On native macOS, Linux and Windows, the CMake `run_tests` target includes
`run_test_json_corpus` and `run_test_json_scalar_corpus`. Each fixture runs in a
fresh process with a five-second timeout. Unexpected outcomes, crashes,
sanitizer diagnostics, missing files and timeouts fail the run. Cross-platform
unit tests remain in `run_test_json` and `run_test_json_scalar`.

To update the corpus, select an upstream commit, copy its complete
`test_parsing` directory and license byte-for-byte, regenerate the manifest's
file hashes and sizes, and review each implementation-defined expectation.
Retain the upstream license; do not insert headers into JSON payloads.
