# Issue 7737 Plan: Choicebox Editability Regression

## Problem
In this branch (branched off `dev`), `form-input-view :choicebox` was migrated 
from JavaFX `ComboBox` to `editor.fxui.combo-box/view`.
During this migration, support for `:from-string` was dropped from the form choicebox renderer.

Previously, `:from-string` had two effects:

1. It enabled manual text entry (`:editable (some? from-string)`).
2. It converted typed text into a value for `:on-value-changed`.

Now all choiceboxes behave as closed pickers (selection only), which is correct for most `:options` fields, but incorrect for specific settings that intentionally allow custom values.

## Code References

Current choicebox rendering path (manual-entry hook missing):

1. `src/clj/editor/cljfx_form_view.clj:459-470`
2. `src/clj/editor/fxui/combo_box.clj:166-183`

Choicebox call sites that explicitly request manual-entry conversion:

1. `src/clj/editor/live_update_settings.clj:78-90`
2. `src/clj/editor/protobuf_forms.clj:266-271`

General settings metadata path that adds `:from-string`/`:to-string` for option-backed settings:

1. `src/clj/editor/settings_core.clj:193-201`

Cell-edit focus behavior that should remain unaffected by this fix:

1. `src/clj/editor/cljfx_form_view.clj:475-478`

## Required Behavior

Default behavior for choiceboxes with `:options`:

1. Treat options as a closed set.
2. Do not allow arbitrary custom values by typing.

Explicit exceptions that must remain manually editable:

1. Live Update settings:
   - `liveupdate.amazon-credential-profile`
   - `liveupdate.amazon-bucket`
2. Protobuf texture profile path settings:
   - `texture-profiles.path-settings.profile`

## Scope Clarification

This issue is not about making all choiceboxes editable again.
It is about restoring manual entry only where metadata/form definitions explicitly require it.

## Proposed Solution

Use `:from-string` as an explicit manual-entry signal and stop auto-injecting it for all option-backed settings.

1. Remove automatic `:from-string`/`:to-string` injection from settings metadata:
   - Remove `add-to-from-string` in `src/clj/editor/settings_core.clj`.
   - Remove `remove-to-from-string`.
2. Keep `:from-string`/`:to-string` only in form definitions that intentionally support custom values:
   - `src/clj/editor/live_update_settings.clj`
   - `src/clj/editor/protobuf_forms.clj`
3. In `src/clj/editor/cljfx_form_view.clj`, make `:choicebox` rendering conditional:
   - If `:from-string` is present, render with legacy editable JavaFX `ComboBox` behavior (old converter path).
   - If `:from-string` is absent, keep current `editor.fxui.combo-box/view` behavior (closed picker).
4. Preserve table-cell popup behavior:
   - Keep current `show-on-focus` behavior for non-editable choiceboxes.
   - Use old cell-edit popup-on-create behavior for editable `:choicebox` cells.

## Implementation Notes

1. `:to-string` alone should not imply editability.
2. Editable fallback should be keyed on `:from-string` presence.
3. Any `game_project` code that strips auto-injected functions (for build metadata) should be updated accordingly when those functions are no longer injected.

## Acceptance Criteria

1. Choiceboxes without an explicit "editable/custom value" signal remain selection-only.
2. The three fields listed above accept typed values not present in `:options`.
3. Typed custom values are correctly converted and dispatched through existing value-change flow.
4. No regressions in table-cell choicebox editing behavior (popup-on-focus remains intact).
