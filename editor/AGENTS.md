# Editor guidance

Use Git only for read-only inspection; never stage or commit.

Follow the [editor build and REPL guide](README_BUILD.md) for setup, development commands, and REPL checks.

Use the `cljfx.plorer` namespace via the live REPL for all Defold Editor UI interactions.

For new or modified Clojure code, apply the [clojure-code-style](.codex/skills/clojure-code-style/) skill, check every rule, and run its required `clj-kondo` lint checks.

## Tests

- Put a comment inside each test body, stating what it verifies and the regression it guards against, if any.
