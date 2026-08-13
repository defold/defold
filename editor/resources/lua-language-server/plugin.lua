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

local lifecycle_signatures = {
    script = {
        init = {parameters = {'userdata', 'userdata'}},
        final = {parameters = {'userdata'}},
        update = {parameters = {'userdata', 'number'}},
        late_update = {parameters = {'userdata', 'number'}},
        fixed_update = {parameters = {'userdata', 'number'}},
        on_message = {parameters = {'userdata', 'hash', 'table<any, any>', 'url'}},
        on_input = {parameters = {'userdata', 'hash|nil', 'on_input.action'}, returns = {'boolean|nil'}},
        on_reload = {parameters = {'userdata'}},
    },
    gui_script = {
        init = {parameters = {'userdata'}},
        final = {parameters = {'userdata'}},
        update = {parameters = {'userdata', 'number'}},
        on_message = {parameters = {'userdata', 'hash', 'table<any, any>', 'url'}},
        on_input = {parameters = {'userdata', 'hash|nil', 'on_input.action'}, returns = {'boolean|nil'}},
        on_reload = {parameters = {'userdata'}},
    },
    render_script = {
        init = {parameters = {'userdata'}},
        update = {parameters = {'userdata', 'number'}},
        on_message = {parameters = {'userdata', 'hash', 'table<any, any>', 'url'}},
        on_reload = {parameters = {'userdata'}},
    },
}

local function resource_type(uri)
    return uri:match('%.([%w_]+)$')
end

local function annotations(signature, parameter_text, indentation, newline, existing_parameters, has_return)
    if parameter_text:find('[^%s,%w_]') then
        return nil
    end

    local parameter_names = {}
    for name in parameter_text:gmatch('[%a_][%w_]*') do
        parameter_names[#parameter_names + 1] = name
    end

    local lines = {}
    for index, type_name in ipairs(signature.parameters) do
        local parameter_name = parameter_names[index]
        if parameter_name and not existing_parameters[parameter_name] then
            lines[#lines + 1] = ('---@param %s %s'):format(parameter_name, type_name)
        end
    end
    if not has_return then
        for _, type_name in ipairs(signature.returns or {}) do
            lines[#lines + 1] = ('---@return %s'):format(type_name)
        end
    end
    if #lines == 0 then
        return nil
    end
    return table.concat(lines, newline .. indentation) .. newline .. indentation
end

local function preceding_annotations(text, line_start, indentation)
    local block_start
    local existing_parameters = {}
    local has_return = false
    local previous_line_break = line_start - 1
    while previous_line_break > 0 do
        local content_end = previous_line_break - 1
        if text:sub(content_end, content_end) == '\r' then
            content_end = content_end - 1
        end
        local previous_line_start = text:sub(1, content_end):match('.*\n()') or 1
        local line = text:sub(previous_line_start, content_end)
        local line_indentation, annotation = line:match('^([ \t]*)%-%-%-(.*)$')
        if not annotation or line_indentation ~= indentation then
            break
        end

        block_start = previous_line_start
        local parameter_name = annotation:match('^@param%s+([%a_][%w_]*)')
            or annotation:match('^@param%s+%[([%a_][%w_]*)%]')
        if parameter_name then
            existing_parameters[parameter_name] = true
        elseif annotation:match('^@return%s') then
            has_return = true
        end
        previous_line_break = previous_line_start - 1
    end
    return block_start and block_start + #indentation or nil, existing_parameters, has_return
end

local function long_bracket(text, start)
    if text:sub(start, start) ~= '[' then
        return nil
    end
    local cursor = start + 1
    while text:sub(cursor, cursor) == '=' do
        cursor = cursor + 1
    end
    if text:sub(cursor, cursor) ~= '[' then
        return nil
    end
    local equals = cursor - start - 1
    return cursor + 1, ']' .. string.rep('=', equals) .. ']'
end

local function mark_code_candidates(text, candidates)
    local candidate_by_start = {}
    for _, candidate in ipairs(candidates) do
        candidate_by_start[candidate.start] = candidate
    end

    local cursor = 1
    local text_length = #text
    while cursor <= text_length do
        local candidate = candidate_by_start[cursor]
        if candidate then
            candidate.code = true
        end

        local char = text:sub(cursor, cursor)
        local next_char = text:sub(cursor + 1, cursor + 1)
        if char == '-' and next_char == '-' then
            local content_start, close = long_bracket(text, cursor + 2)
            if close then
                local close_start = text:find(close, content_start, true)
                cursor = close_start and close_start + #close or text_length + 1
            else
                local newline = text:find('\n', cursor + 2, true)
                cursor = newline or text_length + 1
            end
        elseif char == '"' or char == "'" then
            local quote = char
            cursor = cursor + 1
            while cursor <= text_length do
                char = text:sub(cursor, cursor)
                if char == '\\' then
                    cursor = cursor + 2
                elseif char == quote then
                    cursor = cursor + 1
                    break
                else
                    cursor = cursor + 1
                end
            end
        else
            local content_start, close = long_bracket(text, cursor)
            if close then
                local close_start = text:find(close, content_start, true)
                cursor = close_start and close_start + #close or text_length + 1
            else
                cursor = cursor + 1
            end
        end
    end
end

local function lifecycle_candidates(text, signatures)
    local candidates = {}
    for start, name, parameter_text in text:gmatch('()function[ \t\r\n]+([%a_][%w_]*)[ \t\r\n]*%(([^)]*)%)') do
        local line_start = start
        while line_start > 1 and text:byte(line_start - 1) ~= 10 do
            line_start = line_start - 1
        end
        local indentation = text:sub(line_start, start - 1)
        local signature = indentation:match('^[ \t]*$') and signatures[name]
        if signature then
            local annotation_start, existing_parameters, has_return = preceding_annotations(text, line_start, indentation)
            candidates[#candidates + 1] = {
                start = start,
                insertion_start = annotation_start or start,
                signature = signature,
                parameter_text = parameter_text,
                indentation = indentation,
                existing_parameters = existing_parameters,
                has_return = has_return,
            }
        end
    end
    mark_code_candidates(text, candidates)
    return candidates
end

function OnSetText(uri, text)
    local signatures = lifecycle_signatures[resource_type(uri)]
    if not signatures then
        return nil
    end

    local diffs = {}
    local newline = text:find('\r\n', 1, true) and '\r\n' or '\n'
    for _, candidate in ipairs(lifecycle_candidates(text, signatures)) do
        if candidate.code then
            local inserted_text = annotations(candidate.signature,
                                              candidate.parameter_text,
                                              candidate.indentation,
                                              newline,
                                              candidate.existing_parameters,
                                              candidate.has_return)
            if inserted_text then
                diffs[#diffs + 1] = {
                    start = candidate.insertion_start,
                    finish = candidate.insertion_start - 1,
                    text = inserted_text,
                }
            end
        end
    end
    return #diffs > 0 and diffs or nil
end
