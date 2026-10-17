---
name: editor-tests
description: Use when writing or reviewing Defold Editor tests.
---

## Test the actual product

Never use Var references (`#'...` or `(var ...)`) in tests. Test publicly observable behavior, not implementation details.
Never use `with-redefs` or `with-redefs-fn` in tests. Exercise the real product code rather than replacing it with test-defined stubs.
