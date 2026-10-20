---
name: cljfx
description: cljfx semantics and composition. Use for any cljfx code, design, debugging, or review.
---

## Lifecycles and components

```text
description + opts             --Lifecycle.create-->  component
component + description + opts --Lifecycle.advance--> next component
component + opts               --Lifecycle.delete-->  disposal
component                      --Component.instance--> value
```

- A lifecycle interprets descriptions and manages component state/resources. A component is not its instance: the instance may be a JavaFX object, scalar, callback, collection, or a wrapped child's instance. Advancement can mutate existing objects while returning updated component state.
- `fx/create-component`, `fx/advance-component`, and `fx/delete-component` run synchronously through `lifecycle/root`. JavaFX mutations require the FX thread. Retain the component returned by advancement.
- `lifecycle/dynamic` resolves `:fx/type` through `:fx.opt/type->lifecycle`, falling back to the type itself. The default resolver supports JavaFX keywords, functions deriving descriptions, and lifecycle values. Identical resolved lifecycles advance the child; different lifecycles delete/create it. Keep custom lifecycle values stable.
- Refer to JavaFX lifecycles explicitly, e.g. `cljfx.fx.v-box/lifecycle`, rather than using lazily resolved keywords such as `:v-box`. Require the lifecycle namespaces directly.
- Function types receive props without `:fx/type`. Equal descriptions reuse the previous derived description but still advance its lifecycle. View functions should derive descriptions without effects.
- Composites construct native instances and preserve them on advance. `cljfx.composite/describe` defines constructors, constructor arguments, props, and optional prop order.
- Sequential child collections using `wrap-many` reconcile by `:fx/key` and occurrence; unkeyed children match by occurrence among unkeyed children. Put `:fx/key` on each element map consumed by the collection lifecycle, not inside a function's returned description or a wrapper's nested `:desc`. The collection reads and strips it before interpreting the child. Stable keys do not preserve instances across lifecycle changes. `map-of` reconciles entries by map keys instead.
- Equal descriptions do not share instances. Use `ext-let-refs`/`ext-get-ref` when several descriptions need the same managed object.

## Props and mutators

```text
prop value-description --prop lifecycle--> component --instance--> value
value                  --coercion--> native value --mutator(owner)--> assignment
```

- A prop combines a value lifecycle, mutator, and optional coercion/default. The lifecycle creates/advances/deletes the value's component; the mutator connects its interpreted value to the owner. This is how nested descriptions become native child objects, listeners, or property values.
- Mutators implement `assign!`, `replace!`, and `retract!`. Replacement compares old/new interpreted values, not the current native property. `setter` skips equal values and retracts to nil; `adder-remover` removes/adds on changes; list/map mutators replace contents. Direct native mutations are not repaired by repeating an equal described value.
- `:default` supplies a replacement value on prop removal, not an absent value at creation. Assignment order is not guaranteed by description map order; composites can specify `:prop-order`.
- `fx/make-prop` returns a prop usable **directly as a description key** on composites and detached prop maps. Define it once. Keyword keys use the prop registry; prop keys supply their own config.
- Prefer direct prop keys when a composite description is available; function views can forward them. `fx/make-ext-with-props` is a last resort when no composite description is available. On creation/advancement, it processes the wrapped lifecycle first, then creates/advances and applies the wrapper props. This ordering matters when props must act on an already updated child; direct composite props follow the composite's prop ordering instead.
- `fx/make-binding-prop` accepts `bind(owner-instance, prop-instance)` and a value lifecycle. The bind function may return cleanup. Changed interpreted values dispose/rebind; prop removal disposes. Advancing a stable callback's captured function does not rebind. Use binding props for closure-based bindings and `make-prop` for existing mutators.

Some keys belong to the surrounding context rather than the child's own lifecycle. For example, a VBox's `:children` lifecycle reads and strips `:v-box/vgrow`/`:v-box/margin`, then applies those constraints to the resulting child instance. Put these keys on the outermost child map in that collection, even when the child is a function component or extension. HBox and GridPane constraints work the same way. Namespacing alone does not establish where a key is interpreted; check the enclosing prop lifecycle.

```clojure
;; Require [cljfx.fx.v-box :as fx.v-box].
{:fx/type fx.v-box/lifecycle
 :children [{:fx/type editor-view
             :fx/key :editor
             :v-box/vgrow :always}]}
```

With `fx`, `mutator`, and `lifecycle` aliases, `[cljfx.fx.web-view :as fx.web-view]`, and a `WebView` import:

```clojure
(def prop-on-location-changed
  (fx/make-prop
    (mutator/property-change-listener
      #(.locationProperty (.getEngine ^WebView %)))
    lifecycle/change-listener))

{:fx/type fx.web-view/lifecycle
 prop-on-location-changed on-location-changed}
```

## Callbacks

- `lifecycle/event-handler` wraps functions/maps in stable callbacks whose captured handler is refreshed on advance. Fresh function closures receive current props without listener churn. Prefer functions unless map dispatch is useful; maps invoke `:fx.opt/map-event-handler` with `:fx/event` added. Changing dispatcher or handler kind can replace the wrapper.
- `lifecycle/callback` provides stable wrappers for variadic functions. `change-listener` and `list-change-listener` adapt event handlers to JavaFX listener interfaces. Match the lifecycle/coercion to the custom prop.
- Capture the information needed to express the user's intent, such as an item identity and requested action, rather than old state needed to execute it. Resolve that intent against current state when handling the callback: state may change while the user is expressing the intent. Don't depend on an intervening render to refresh callback captures.
- `lifecycle/root` binds `*in-progress?*` true; wrapped handlers/callbacks suppress invocation under that binding. Raw JavaFX listeners bypass suppression, and scalar function props do not acquire callback semantics automatically.
- `fx/run-later` conveys dynamic bindings, so code queued during rendering can retain `*in-progress?*` true after rendering ends. Suppression depends on the binding at invocation.
- `fx/on-fx-thread` runs immediately on the FX thread, otherwise queues; `fx/run-later` always queues. Both return derefable results that rethrow captured Throwables. Do not dereference queued FX work while blocking that thread.
- Node/Scene `:event-filter` and `:event-handler` listen for `Event/ANY`: filters run before control consumption, handlers during bubbling.

## State and rendering

- Prefer `fx/ext-state` for component-owned state. It owns an atom and injects `:state`/`:swap-state`; rename with `:key`/`:swap-key`. Updates use retryable, any-thread `swap!` semantics; keep IO outside update functions.
- Changed `:initial-state` resets state while advancing the child. Optional `:reset(current-state, new-initial-state)` controls the reset; default takes the new initial state. Unchanged initial state preserves accumulated state.
- Prefer `fx/ext-watcher` for externally owned refs. It injects the ref's value under `:key`, default `:value`. Changing `:ref` replaces the subscription and advances the existing child with the new ref's value; nil supplies nil without subscribing. Wrap it in `ext-recreate-on-key-changed` when ref changes should recreate the child. Independent components can watch independent refs without a renderer or global atom.
- State updates are synchronous; FX rendering is queued/coalesced and may skip intermediate states. Returning from `swap!` does not mean the view rendered; no-op swaps can notify watches. Resolve state-dependent actions inside `swap-state`, then perform subsequent work outside the retryable update function using its returned state. Rendering, CSS/layout, and JavaFX pulses are separate.
- Local state requires its root instance to remain equal, as does a watcher while its ref stays the same; descendants may change. Put `ext-recreate-on-key-changed` outside this boundary to reset state and view together. Watcher deletion or ref replacement removes the old watch and invalidates its queued renders.

## Composition and effects

- Prefer composing existing extensions to custom lifecycles. Defold `ui/defc` nests `:compose` entries with the first outermost. Injected props are available inward; watcher/state updates advance only the inner subtree. Place state-dependent entries inside the watcher/state entry.
- `ext-on-instance-lifecycle` calls created/deleted hooks after child creation/deletion, and advanced only when old/new instances are `not=`. It does not observe ordinary property updates. Express properties/listeners as props.
- Use callbacks for event actions; use `ext-effect` when work or subscriptions should start/restart/stop with component dependencies. Effect `:fn` runs synchronously with `:args`; launch asynchronous work explicitly if needed. A returned function is cleanup.
- Effects start before child creation. Advancement advances the child, then cleans up/restarts when fn/args change; deletion cleans up before deleting the child. Effects are not after-render/layout hooks.

## Ownership and deletion

- Let composites own native construction and give each property/resource one owner.
- **Prop removal and composite deletion differ.** Composite deletion deletes prop lifecycles without retracting native values, so binding-prop cleanup is not invoked through retraction. A listener attached only to a discarded owned node can die with it; subscriptions to external objects/resources and reused native nodes need explicit lifecycle cleanup.
- Async cancellation/stale-result rejection is separate from listener cleanup.

## References

- [Changelog](https://github.com/cljfx/cljfx/blob/master/CHANGELOG.md); [binding-prop tests](https://github.com/cljfx/cljfx/blob/master/test/cljfx/props_test.clj); [FX scheduling implementation](https://github.com/cljfx/cljfx/blob/master/src/cljfx/platform.clj).
- Examples: [local state](https://github.com/cljfx/cljfx/blob/master/examples/e42_local_state.clj), [nested state](https://github.com/cljfx/cljfx/blob/master/examples/e43_nested_local_state.clj), [lazy tree](https://github.com/cljfx/cljfx/blob/master/examples/e38_lazy_loaded_tree_view.clj).
