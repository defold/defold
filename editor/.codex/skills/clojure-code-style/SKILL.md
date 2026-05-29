---
name: clojure-code-style
description: Use for all Clojure changes and final checks.
---

Applies to your changes only, keep the rest of the code untouched unless explicitly asked.

# Rules

## if/if-not

When one branch of an `if` is trivial, prefer placing the trivial branch in the true position.

## Functions

When a `defn` has a return type hint, put the type hint and argument vector on the next line.

Avoid introducing multiple arities to functions. Prefer updating call sites with added arguments instead. For recursive functions, prefer a separate private `-impl`-suffixed helper over an internal-only arity.

## Grouping and spacing

Use newlines between paired forms (e.g., `let`-bindings, `cond`, map literals) to keep indentation under control. Separate blocks with blank lines.

## Keep requires sorted

## Validate at edges, trust internals

See the public boundaries of the code and which invariants are enforced there. After that, trust the private code. This means there should be no error handling or checks for invariants.

## No lazy sequences

Use transducers/eductions. Avoid `seq`, `empty?`, `some` — there are better alternatives in `util.coll` ns. Some rare cases where lazy sequences are ok: when result is then sorted.
Note `coll/some` variants — `coll/first-where`, `coll/any?`

## `?` suffix is only for predicates

Ok: `even?` — predicate fn with ? suffix.
Ok: `(let [even (even? n)] ...)` — boolean without ? suffix.
Not ok: `(let [is-even? (even? n)] ...)` — boolean with ? suffix.

## Truthiness

Prefer `if-let`/`when-let` over `if-some`/`when-some`. Latter can be used only where false is a valid value. Prefer directly checking values for truthiness over boolean-returning checks.

## Linting

Use `clj-kondo` to find and fix lint issues in your changes.

## Inline single-use trivial helpers

They make the code worse because they make the logic/behavior non-local, introducing a separation where there is none.

## Use of `^:unsafe _evaluation-context` in graph outputs

`^:unsafe` is a code smell; its use will lead to invalidation issues. Use as a last resort. All existing use of unsafe has necessary preconditions that make it safe. All uses of unsafe have comments explaining why it's safe, and what the preconditions are.
