# Repository Guidelines

## Coding Style

Don't introduce let bindings unless the variable will be used in many places.
If a let binding may be used inside multiple places, but otherwise it's nilable in another, prefer duplicating code instead of introducing bindings.

## Git Guidelines

Use git in read-only mode (e.g., status, diff), never stage/commit anything.
