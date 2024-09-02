local function make_on_pressed(value, on_value_changed, title, filters)
    return function()
        local path = editor.ui.show_external_file_dialog { path = value, title = title, filters = filters }
        if path then on_value_changed(path) end
    end
end

local function make_on_value_changed(set_value, on_value_changed)
    return function(text)
        local resolved_path = editor.external_file_attributes(text).path
        set_value(resolved_path)
        if on_value_changed then on_value_changed(resolved_path) end
    end
end

editor.ui.external_file_field = editor.ui.component(function(props)
    local value, set_value = editor.ui.use_state(props.value)
    local on_value_changed = editor.ui.use_memo(make_on_value_changed, set_value, props.on_value_changed)
    local on_pressed = editor.ui.use_memo(make_on_pressed, value, on_value_changed, props.title, props.filters)
    return editor.ui.horizontal {
        spacing = editor.ui.SPACING_SMALL,
        alignment = props.alignment,
        children = {
            editor.ui.string_field {
                expand = true,
                value = value,
                on_value_changed = on_value_changed,
                variant = props.variant,
                disabled = props.disabled
            },
            editor.ui.text_button {
                text = "...",
                on_pressed = on_pressed,
                disabled = props.disabled
            }
        }
    }
end)