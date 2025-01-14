-- Copyright 2020-2025 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.

local function make_external_field_selector_callback(value, on_value_changed, title, filters)
    return function()
        local path = editor.ui.show_external_file_dialog { path = value, title = title, filters = filters }
        if path then on_value_changed(path) end
    end
end

local function make_external_field_string_field_callback(set_value, on_value_changed)
    return function(value)
        if value == "" then
            value = nil
        else
            value = editor.external_file_attributes(value).path
        end
        set_value(value)
        if on_value_changed then on_value_changed(value) end
    end
end

editor.ui.external_file_field = editor.ui.component(function(props)
    local value, set_value = editor.ui.use_state(props.value)
    local on_value_changed = editor.ui.use_memo(make_external_field_string_field_callback, set_value, props.on_value_changed)
    local on_pressed = editor.ui.use_memo(make_external_field_selector_callback, value, on_value_changed, props.title, props.filters)
    return editor.ui.horizontal {
        spacing = editor.ui.SPACING.SMALL,
        alignment = props.alignment,
        children = {
            editor.ui.string_field {
                grow = true,
                value = value,
                on_value_changed = on_value_changed,
                issue = props.issue,
                tooltip = props.tooltip,
                enabled = props.enabled
            },
            editor.ui.button {
                text = "...",
                on_pressed = on_pressed,
                enabled = props.enabled
            }
        }
    }
end)

local function make_resource_field_string_field_callback(set_value, on_value_changed)
    return function(value)
        local first_byte = string.byte(value, 1)
        if not first_byte then
            value = nil
        elseif first_byte ~= 47 then
            value = '/' .. value
        end
        set_value(value)
        if on_value_changed then on_value_changed(value) end
    end
end

local function make_resource_field_selector_callback(on_value_changed, title, extensions)
    return function()
        local resource_path = editor.ui.show_resource_dialog({ title = title, extensions = extensions})
        if resource_path then on_value_changed(resource_path) end
    end
end

local function make_resource_field_open_resource_callback(value)
    return function() editor.ui.open_resource(value) end
end

editor.ui.resource_field = editor.ui.component(function(props)
    local value, set_value = editor.ui.use_state(props.value)
    local on_value_changed = editor.ui.use_memo(make_resource_field_string_field_callback, set_value, props.on_value_changed)
    local selector_callback = editor.ui.use_memo(make_resource_field_selector_callback, on_value_changed, props.title, props.extensions)
    local open_resource_callback = editor.ui.use_memo(make_resource_field_open_resource_callback, value)

    local enabled = props.enabled
    if enabled == nil then enabled = true end

    return editor.ui.horizontal({
        spacing = editor.ui.SPACING.SMALL,
        alignment = props.alignment,
        children = {
            editor.ui.string_field({
                grow = true,
                value = value,
                on_value_changed = on_value_changed,
                issue = props.issue,
                tooltip = props.tooltip,
                enabled = enabled
            }),
            editor.ui.button({
                icon = editor.ui.ICON.OPEN_RESOURCE,
                enabled = enabled and value ~= nil and editor.resource_attributes(value).is_file,
                on_pressed = open_resource_callback
            }),
            editor.ui.button({
                text = "...",
                on_pressed = selector_callback,
                enabled = enabled
            })
        }
    })
end)