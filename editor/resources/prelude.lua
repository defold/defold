-- Copyright 2020-2026 The Defold Foundation
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

-- Resource field

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

-- Bundling

-- Bundling: utils

editor.bundle.abort_message = {} -- marker error message object

function editor.bundle.assoc(t, k, v)
    t = t or {}
    if t[k] == v then
        return t
    else
        local ret = {}
        for key, value in pairs(t) do
            ret[key] = value
        end
        ret[k] = v
        return ret
    end
end

function editor.bundle.assoc_in(t, ks, v)
    local n = #ks
    if n == 0 then
        return v
    else
        local function impl(acc, i)
            if i == n then
                return editor.bundle.assoc(acc, ks[i], v)
            else
                local k = ks[i]
                return editor.bundle.assoc(acc, k, impl(acc and acc[k], i + 1))
            end
        end
        return impl(t, 1)
    end
end

function editor.bundle.make_to_string_lookup(table)
    return function(x) return table[x] or tostring(x) end
end

-- Bundling: UI components

function editor.bundle.select_box(config, set_config, key, options, to_string, rest_props)
    local props = rest_props or {}
    props.value = config[key]
    props.on_value_changed = function(value) set_config(editor.bundle.assoc, key, value) end
    props.options = options
    props.to_string = to_string
    return editor.ui.select_box(props)
end

function editor.bundle.grid_row(text, content)
    return {
        text and editor.ui.label({text = text, alignment = editor.ui.ALIGNMENT.TOP_RIGHT}) or false,
        type(content) == "table" and editor.ui.vertical({children = content}) or content
    }
end

local desktop_variants = {"debug", "release", "headless"}
local common_variants = {"debug", "release"}
local variant_to_string = editor.bundle.make_to_string_lookup({
    debug = localization.message("dialog.bundle.option.variant.debug"),
    release = localization.message("dialog.bundle.option.variant.release"),
    headless = localization.message("dialog.bundle.option.variant.headless")
})
function editor.bundle.desktop_variant_grid_row(config, set_config)
    return editor.bundle.grid_row(localization.message("dialog.bundle.label.variant"), editor.bundle.select_box(config, set_config, "variant", desktop_variants, variant_to_string))
end

function editor.bundle.common_variant_grid_row(config, set_config)
    return editor.bundle.grid_row(localization.message("dialog.bundle.label.variant"), editor.bundle.select_box(config, set_config, "variant", common_variants, variant_to_string))
end

local texture_compressions = {"enabled", "disabled", "editor"}
local texture_compression_to_string = editor.bundle.make_to_string_lookup({
    enabled = localization.message("dialog.bundle.option.texture-compression.enabled"),
    disabled = localization.message("dialog.bundle.option.texture-compression.disabled"),
    editor = localization.message("dialog.bundle.option.texture-compression.editor")
})
function editor.bundle.texture_compression_grid_row(config, set_config)
    return editor.bundle.grid_row(
        localization.message("dialog.bundle.label.texture-compression"),
        editor.bundle.select_box(config, set_config, "texture_compression", texture_compressions, texture_compression_to_string)
    )
end

function editor.bundle.check_box(config, set_config, key, text, rest_props)
    local props = rest_props or {}
    props.value = config[key]
    props.on_value_changed = function (value) set_config(editor.bundle.assoc, key, value) end
    props.text = text
    return editor.ui.check_box(props)
end

function editor.bundle.check_boxes_grid_row(config, set_config)
    return editor.bundle.grid_row(nil, {
        editor.bundle.check_box(config, set_config, "with_symbols", localization.message("dialog.bundle.label.with-symbols")),
        editor.bundle.check_box(config, set_config, "build_report", localization.message("dialog.bundle.label.build-report")),
        editor.bundle.check_box(config, set_config, "liveupdate", localization.message("dialog.bundle.label.liveupdate")),
        editor.bundle.check_box(config, set_config, "contentless", localization.message("dialog.bundle.label.contentless"))
    })
end

function editor.bundle.set_element_check_box(config, set_config, key, element, text, error)
    return editor.ui.check_box({
        value = config[key][element],
        on_value_changed = function (value) set_config(editor.bundle.assoc_in, {key, element}, value or nil) end,
        text = text,
        issue = error and {severity = editor.ui.ISSUE_SEVERITY.ERROR, message = error}
    })
end

function editor.bundle.external_file_field(config, set_config, key, error, rest_props)
    local props = rest_props or {}
    props.value = config[key]
    props.on_value_changed = function(value) set_config(editor.bundle.assoc, key, value or "") end
    props.issue = error and {severity = editor.ui.ISSUE_SEVERITY.ERROR, message = error}
    return editor.ui.external_file_field(props)
end

function editor.bundle.dialog(header, config, hint, error, content)
    return editor.ui.dialog({
        title = localization.message("dialog.bundle.title"),
        header = editor.ui.vertical({children = {
            editor.ui.heading({text = header}),
            editor.ui.paragraph({
                text = error or hint or localization.message("dialog.bundle.hint.select-output-folder"),
                color = error and editor.ui.COLOR.ERROR or editor.ui.COLOR.HINT
            })
        }}),
        content = editor.ui.scroll({
            content = editor.ui.grid({
                padding = editor.ui.PADDING.LARGE,
                columns = {{}, {grow = true}},
                children = content
            })
        }),
        buttons = {
            editor.ui.dialog_button({
                text = localization.message("dialog.button.close"),
                cancel = true
            }),
            editor.ui.dialog_button({
                text = localization.message("dialog.bundle.button.create"),
                default = true,
                result = config,
                enabled = not error
            }),
        }
    })
end

-- Bundling: flow

function editor.bundle.config(requested_dialog, prefs_key, dialog_component, errors_fn)
    -- TODO: remove migration from legacy prefs after sufficient time has passed (e.g. after 2026-01-13)
    if not editor.prefs.is_set(prefs_key .. ".variant") and editor.prefs.is_set("bundle.variant") then
        pcall(editor.prefs.set, prefs_key .. ".variant", editor.prefs.get("bundle.variant"))
        editor.prefs.set(prefs_key .. ".texture_compression", editor.prefs.get("bundle.texture-compression"))
        editor.prefs.set(prefs_key .. ".with_symbols", editor.prefs.get("bundle.debug-symbols"))
        editor.prefs.set(prefs_key .. ".build_report", editor.prefs.get("bundle.build-report"))
        editor.prefs.set(prefs_key .. ".liveupdate", editor.prefs.get("bundle.liveupdate"))
        editor.prefs.set(prefs_key .. ".contentless", editor.prefs.get("bundle.contentless"))
    end
    local config = editor.prefs.get(prefs_key)
    local errors = errors_fn and errors_fn(config)
    local show_dialog = requested_dialog or errors
    if show_dialog then
        config = editor.ui.show_dialog(dialog_component({config = config, errors = errors}))
        if not config then
            error(editor.bundle.abort_message)
        end
        editor.prefs.set(prefs_key, config)
    end
    return config
end

function editor.bundle.output_directory(requested_dialog, output_subdir)
    local output_root = editor.prefs.get("bundle.output-directory")
    local prompt_overwrite = requested_dialog or #output_root == 0
    if prompt_overwrite then
        output_root = editor.ui.show_external_directory_dialog({
            title = localization.message("dialog.bundle.output-directory.title"),
            path = #output_root > 0 and output_root or nil
        })
        if output_root then
            editor.prefs.set("bundle.output-directory", output_root)
        else
            error(editor.bundle.abort_message)
        end
    end
    local output_directory = output_root .. "/" .. output_subdir
    if not prompt_overwrite then return
        output_directory
    end
    local attrs = editor.external_file_attributes(output_directory)
    if attrs.exists then
        if attrs.is_file then
            editor.ui.show_dialog(editor.ui.dialog({
                title = localization.message("dialog.bundle.output-directory.error-file.title"),
                header = editor.ui.heading({text = localization.message("dialog.bundle.output-directory.error-file.header")}),
                content = editor.ui.vertical({ padding = editor.ui.PADDING.LARGE, children = {
                    editor.ui.paragraph({text = localization.message("dialog.bundle.output-directory.error-file.message", {path = attrs.path})})
                }})
            }))
            error(editor.bundle.abort_message)
        else
            local overwrite = editor.ui.show_dialog(editor.ui.dialog({
                title = localization.message("dialog.bundle.output-directory.overwrite.title"),
                header = editor.ui.vertical({children = {
                    editor.ui.heading({text = localization.message("dialog.bundle.output-directory.overwrite.header")}),
                    editor.ui.paragraph({text = localization.message("dialog.bundle.output-directory.overwrite.message", {path = attrs.path})})
                }}),
                buttons = {
                    editor.ui.dialog_button({
                        text = localization.message("dialog.button.cancel"),
                        cancel = true,
                        result = false
                    }),
                    editor.ui.dialog_button({
                        text = localization.message("dialog.bundle.output-directory.button.overwrite"),
                        default = true,
                        result = true
                    }),
                }
            }))
            if not overwrite then
                error(editor.bundle.abort_message)
            end
        end
    end
    return output_directory
end

function editor.bundle.create(config, output_directory, extra_bob_opts)
    local bob_opts = extra_bob_opts or {}
    bob_opts.variant = config.variant
    if config.contentless then
        bob_opts.exclude_archive = true
    else
        bob_opts.archive = true
    end
    local compression = config.texture_compression
    bob_opts.bundle_output = output_directory
    bob_opts.texture_compression = compression == "editor" and editor.prefs.get("build.texture-compression") or compression == "enabled"
    bob_opts.strip_executable = config.variant == "release"
    bob_opts.with_symbols = config.with_symbols
    bob_opts.build_report_html = config.build_report and (output_directory .. "/report.html") or nil
    bob_opts.liveupdate = config.liveupdate and "yes" or nil
    bob_opts.output = "build/default_bundle/"

    if not editor.external_file_attributes(output_directory).exists then
        if editor.platform:sub(-#"win32") == "win32" then
            editor.execute("cmd.exe", "/c", "mkdir", output_directory:gsub("/", "\\"), {reload_resources = false})
        else
            editor.execute("mkdir", "-p", output_directory, {reload_resources = false})
        end
    end

    editor.bob(bob_opts, "resolve", "build", "bundle")

    if editor.prefs.get("bundle.open-output-directory") then
        editor.open_external_file(output_directory)
    end
end

-- Bundling: command

function editor.bundle.command(label, id, fn, rest)
    local result = rest or {}
    result.label = label
    result.id = id
    result.locations = {"Bundle"}
    result.query = {argument = true}
    result.run = function(opts)
        local argument = opts.argument
        if opts.argument == nil then
            argument = true
        else
        end
        local success, ret_or_message = pcall(fn, argument)
        if success or ret_or_message == editor.bundle.abort_message then
            return
        end
        io.stderr:write(("Bundling '%s' failed: %s\n"):format(tostring(label), tostring(ret_or_message))):flush()
        editor.ui.show_dialog(editor.ui.dialog({
            title = localization.message("dialog.bundle.failed.title"),
            header = editor.ui.vertical({children={
                editor.ui.heading({text = localization.message("dialog.bundle.failed.header")}),
                editor.ui.paragraph({
                    text = localization.message("dialog.bundle.failed.hint"),
                    color = editor.ui.COLOR.HINT
                })
            }})
        }))
    end
    return result
end

-- Bundling: prefs schema

editor.bundle.desktop_variant_schema = editor.prefs.schema.enum({ values = desktop_variants })
editor.bundle.common_variant_schema = editor.prefs.schema.enum({ values = common_variants })

function editor.bundle.config_schema(variant_schema, properties)
    properties = properties or {}
    properties.variant = variant_schema
    properties.texture_compression = editor.prefs.schema.enum({values = texture_compressions})
    properties.with_symbols = editor.prefs.schema.boolean({default = true})
    properties.build_report = editor.prefs.schema.boolean()
    properties.liveupdate = editor.prefs.schema.boolean()
    properties.contentless = editor.prefs.schema.boolean()
    return editor.prefs.schema.object({scope = editor.prefs.SCOPE.PROJECT, properties = properties})
end
