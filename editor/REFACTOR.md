# Resource View Sidebar Refactor

## Goal

Make the editor's right pane be controlled by open views.

The new concept is called **Resource View Sidebar**:
- A view node defines what is shown in the right pane.
- The active tab/view decides the active sidebar.
- Sidebar rendering should progressively move toward a single Cljfx tree.

## Product Rules (Agreed)

1. Default: `Outline + Properties`
2. Views of debuggable resources: normal sidebar when debugger is inactive, `Call Stack + Variables` when debugger is active on a debuggable resource and suspended
3. Form views: `Outline` only
4. Script files: `Structure + Properties`
5. Code files: `Structure` only

## Current State (What Exists Today)

- Right pane content is statically declared in `resources/editor.fxml` as Outline + Properties (`#right-pane`, `#right-split`).
- `OutlineView` and `PropertiesView` are created once globally in `boot_open_project` and wired to `app-view`.
- `AppView` tracks open tab views via `:open-views`, but right pane content is not view-owned.
- Active outline wiring is currently resource-node based (`resource-node :node-outline -> app-view :active-outline` in `on-active-tab-changed!`).
- Debugger currently hijacks the right pane by toggling visibility between `#right-split` and `#debugger-data-split`.

## Target Architecture

### 1) WorkbenchView sidebar contract

Define sidebar policy on the view node contract:
- `WorkbenchView` exposes `:sidebar-panes` output.
- `:sidebar-panes` value is either:
  - an ordered vector of sidebar pane Cljfx descriptions and/or sidebar sentinels (`:outline`, `:properties`).
- `WorkbenchView` exposes connected output `:view-sidebar-panes` as `[_node-id panes]` for AppView aggregation.

### 2) App-level active sidebar selection

In `AppView`:
- Track sidebars for all open views via connected `:view-sidebar-panes` (similar to `:open-views` / `:open-dirty-views`).
- Expose `:active-sidebar` derived from active tab's view-node-id.
- Resolve sentinel panes (`:outline`, `:properties`) into corresponding pane descs.

### 3) AppView-owned sidebar renderer

Render the right pane from `AppView` (Cljfx-driven):
- Mounted into `#right-pane`
- Uses `:active-sidebar`
- Uses each sidebar pane as a Cljfx desc
- Skips panes that are error-valued or invalid
- Makes `AppView` the only owner of right-pane content
- Keeps ownership of `#right-split` SplitPane in one place and assigns rendered section views as split children
- Keeps split ids stable for existing prefs persistence behavior

### 4) Debugger integration in AppView

Debugger should not directly toggle right-pane controls.
View nodes should remain debugger-agnostic.
`AppView` should own debugger-aware sidebar selection:
- Receives debugger state inputs (session/suspension) from `DebugView`
- Checks if the active resource is debuggable
- If debugger is active and suspended on a debuggable resource, uses debugger sidebar panes
- Otherwise uses active tab sidebar panes from the `WorkbenchView` sidebar contract

## Suggested Sidebar Shape

Use EDN data (no JavaFX objects):

```clj
[{:fx/type ...} ; outline pane desc
 {:fx/type ...}] ; properties pane desc
```

Notes:
- Order defines vertical order in sidebar.
- Sidebar panes are Cljfx descriptions.
- `AppView` handles panes best-effort per item.
- If a pane is error-valued/invalid, `AppView` skips only that pane and continues rendering the rest.

## Split Ownership Decision

- `WorkbenchView :sidebar-panes` should not return a full Cljfx SplitPane description.
- `WorkbenchView :sidebar-panes` should return pane Cljfx descriptions/sentinels in order.
- `AppView` should be the only owner of split-pane creation and ids (`#right-split`).
- This preserves split prefs compatibility while still allowing per-view sidebar composition.

## Sidebar Panes

- Outline pane desc: existing outline tree semantics
- Properties pane desc: existing selected-node properties editor
- Structure pane desc: code/form structure tree (new abstraction; implementation source may be AST/LSP/outline-adapter)
- Call stack pane desc: debugger call stack UI
- Variables pane desc: debugger variables UI

## Migration Plan (Phased)

### Migration invariant (all phases)

- Keep JavaFX ids that are still referenced by persistence or command wiring.
- Remove/rename ids only after their readers/writers are migrated.

### ✅ Phase 1: Parity mode (same behavior)

1. Add `WorkbenchView :sidebar-panes` plus connected `:view-sidebar-panes` (`[_node-id panes]`).
2. Use explicit pane sentinels in `WorkbenchView` (`:outline`, `:properties`) so `:sidebar-panes` always returns a vector.
3. Refactor `OutlineView` and `PropertiesView` to expose pane outputs instead of mounting into injected JavaFX nodes. Connect `OutlineView` and `PropertiesView` into `AppView` when they are constructed.
4. Compute/store per-tab sidebar panes in `AppView` from `:view-sidebar-panes`, resolving per-item sentinels to pane descs.
5. Render right pane from `AppView` using active sidebar pane descs.
6. Keep `#right-split` SplitPane in FXML as the persisted container, but make it content-empty by default and populated by `AppView` sidebar rendering (preserving split ids/prefs behavior).
7. Keep debugger behavior unchanged: `debug_view` still controls right-pane vs debugger-pane visibility.

Acceptance:
- Sidebar policy is defined by `WorkbenchView :sidebar-panes`, and active tab selects the resulting sidebar panes.
- User-visible debugger behavior is unchanged.
- Outline/properties panes come from view outputs (not direct mount points).
- No regression in outline/properties wiring or split persistence.

### ✅ Phase 2: First policy rollout (small visible change)

1. Introduce first non-default view sidebar policy in a narrow scope.
2. Form `WorkbenchView :sidebar-panes` emits only outline pane desc (no properties pane desc).
3. Keep debugger behavior unchanged (`debug_view` still controls pane visibility).

Acceptance:
- Forms no longer show properties in the sidebar.
- Other views stay on default sidebar behavior.
- No debugger behavior change.

### ⏳ Phase 3: Debugger unification in AppView

1. Connect debugger state into `AppView`.
2. In `AppView`, override active sidebar panes with debugger pane descs (call stack + variables) when conditions match.
3. Remove direct right-pane visibility toggles in `debug_view` after AppView override path is verified.
4. Ownership boundary: `AppView` is the single owner of sidebar switching; view nodes remain debugger-agnostic.
5. Remove `#debugger-data-split` from `editor.fxml` after migration (debugger sidebar content is rendered through the AppView-owned sidebar path).

Acceptance:
- Debugging transitions (suspend/resume/stop) correctly switch sidebar item set.

### Phase 4: Code sidebar policy rollout

1. Implement code-focused sidebar policies:
   - `.script` views emit structure + properties pane descs
   - other code views emit structure pane desc
2. Keep `AppView` as the sole sidebar renderer/owner for these policies.
3. Remove ids only when no persistence/command readers remain.

Acceptance:
- `.script` resources show `Structure + Properties`.
- Other code resources show `Structure` only.
- Policy switching works by active tab without debugger regressions.

## Code Touchpoints

Primary files:
- `src/clj/editor/view.clj`
- `src/clj/editor/app_view.clj`
- `src/clj/editor/boot_open_project.clj`
- `src/clj/editor/debug_view.clj`
- `src/clj/editor/outline_view.clj`
- `src/clj/editor/properties_view.clj`
- `src/clj/editor/code/view.clj`
- `src/clj/editor/cljfx_form_view.clj`
- `resources/editor.fxml`

## Risks and What To Watch

1. **Tab close/dispose race**
   - Tab close asynchronously deletes view graph.
   - Sidebar renderer must not retain stale node references.
   - Pane production must skip stale or error-valued entries instead of failing the sidebar.

2. **Debugger transitions**
   - Suspend/resume and resource-switch behavior must not flicker or leave stale sidebar state.

3. **Selection model coupling**
   - Selection/properties are currently keyed by resource node, not per view instance.
   - Multiple views of same resource must remain correct.

4. **Prefs persistence compatibility**
   - Existing split ids and hidden-pane state are persisted.
   - Preserve compatibility during migration.

5. **Incremental rollout safety**
   - Keep sentinel default fallback for views that do not yet provide explicit sidebars.

6. **Per-item failure handling**
   - One failing sidebar item must not blank out the whole right pane.

## Testing Strategy

Integration checks should cover:

1. Active tab switch changes active sidebar.
2. Sidebar panes for default resource.
3. Sidebar panes for form view.
4. Sidebar panes for script view.
5. Sidebar panes for code view.
6. Form views use `Outline`.
7. Script views use `Structure + Properties`.
8. Other code views use `Structure`.
9. Debugger state override (`normal -> debugger -> normal`).
10. Open/close tabs with no stale sidebar references or exceptions.
11. Pane visibility and split persistence still restored correctly.
12. Error-valued sidebar panes are skipped without breaking the whole sidebar.
