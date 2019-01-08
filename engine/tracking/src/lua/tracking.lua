local MAX_TRACKING_FILES  = 10
local MAX_EVENTS_PER_FILE = 50

local INITIAL_RETRY_TIME = 1.0
local SAVE_INTERVAL = 5.0

-- client configuration
local app_id, app_save_dir
local config_url
local sys_info = sys.get_sys_info()
local defold_version

local sys_field_mapping = {
    country = "territory",
    platform = "system_name",
    platform_version = "system_version",
    model = "device_model",
    manufacturer = "manufacturer"
}

local app_version
local sess_start = os.time()

-- fetched from server
local server_config;

-- in memory tracking
local meta_data = {}
local need_save = false
local file_data = {}
local file_state = {}
local meta_fn;
local tracking_enable = false

local time_to_next_send = 0
local time_to_next_save = SAVE_INTERVAL
local retry_timer = INITIAL_RETRY_TIME
local failing = false

-- filename base
local fn_base

function wrap_sys_load(fn)
    local res
    function real()
        res = sys.load(fn)
    end
    if pcall(real) then
        return res;
    else
        return {}
    end
end

function wrap_sys_save(fn, t)
    local res = false
    function real()
            res = sys.save(fn, t)
    end
    pcall(real)
    return res
end

function make_new_meta()
    local m = {}
    m.version = "1.0"
    m.files = {}
    for i=1,MAX_TRACKING_FILES do
        m.files[i] = {}
        m.files[i].name = fn_base .. "_" .. tostring(i) .. ".dat"
        m.files[i].message_id = 0
        m.files[i].num_events = 0
        m.files[i].order = 0
    end
    return m
end

function convert_platform_name(system_name)
    if system_name == "iPhone OS" then
        return "ios"
    end
    if system_name == "HTML5" then
        return "web"
    end
    return system_name
end

function start(save_directory, engine_version)

    tracking_enable = true

    math.randomseed(os.time())
    defold_version = engine_version
    app_save_dir = save_directory
    config_url = sys.get_config("tracking.url");
    app_id = sys.get_config("tracking.app_id");
    app_version = sys.get_config("project.version");
    if not app_version then
        app_version = "unknown"
    end

    if app_id == nil or string.len(app_id) < 1 then
        tracking_enable = false
        return
    end

    local new_install = false
    if not config_url or config_url == "" then
        config_url = "https://g.defold.com/conf";
    end

    fn_base = "t" .. app_id
    meta_fn = sys.get_save_file(app_save_dir, fn_base .. "_meta.dat");
    meta_data = wrap_sys_load(meta_fn);
    if not meta_data.version or table.getn(meta_data.files) < 1 then
        local new_meta = make_new_meta();
        wrap_sys_save(meta_fn, new_meta);
        meta_data = wrap_sys_load(meta_fn);
        if meta_data.version == new_meta.version then
            new_install = true
        else
            -- failed to save
            tracking_enable = false
            return
        end
    end

    -- Disable if version mismatch for now.
    if meta_data.version ~= "1.0" then
        tracking_enable = false;
        return
    end

    -- If we get this far, we could either load the old meta or successfully
    -- save a new one.
    local count = table.getn(meta_data.files)
    for i=1,count do
        file_data[i] = {}
        file_state[i] = {}
        file_state[i].persist = false
        file_state[i].dirty = false
        if meta_data.files[i] and meta_data.files[i].name and meta_data.files[i].num_events > 0 then
            file_data[i] = wrap_sys_load(sys.get_save_file(app_save_dir, meta_data.files[i].name));
            if not file_data[i].events or table.getn(file_data[i].events) ~= meta_data.files[i].num_events then
                -- mismatch, clear
                file_data[i] = {}
                file_data[i].events = {}
            end
            meta_data.files[i].num_events = table.getn(file_data[i].events);
        end
    end

    if new_install then
        local evt = {}
        evt.type = "@Install"
        evt.attributes = {}
        evt.metrics = {}
        evt.time_stamp = os.time()
        insert_event(evt);
    end
end

function insert_event(event)
    -- ordered will be the metadata file table but sorted
    -- with order>highest so events are always appended to the
    -- latest entry.
    local ordered = {}
    for k,v in pairs(meta_data.files) do
        ordered[k] = {}
        ordered[k].index = k
        ordered[k].value = v
    end
    local cmp = function(a, b)
        return a.value.order > b.value.order;
    end
    table.sort(ordered, cmp);

    -- pick first unsent one
    local highest_order = 0
    for k,v in pairs(ordered) do
        local filemeta = meta_data.files[v.index]
        if filemeta.order > highest_order then
            highest_order = filemeta.order
        end
        if filemeta.message_id == 0 and filemeta.num_events < MAX_EVENTS_PER_FILE then
            if filemeta.num_events == 0 then
                filemeta.order = highest_order + 1
                file_data[v.index] = {}
                file_data[v.index].events = {}
                file_state[v.index].persist = failing
            end
            table.insert(file_data[v.index].events, event);
            filemeta.num_events = filemeta.num_events + 1
            file_state[v.index].dirty = true
            -- need save if this goes onto disk.
            need_save = need_save or file_state[v.index].persist
            return
        end
    end
end

local last_persist_count = 0

function save(force)

    -- temp meta table so can clear out things that are not persisted
    local save_meta = {}
    save_meta.version = "1.0";
    save_meta.stid = meta_data.stid
    save_meta.files = {}

    local count = table.getn(meta_data.files)
    local persist_count = 0

    for i=1,count do
        local state = file_state[i]
        if state.persist then
            if state.dirty and meta_data.files[i].num_events > 0 then
                local fn = sys.get_save_file(app_save_dir, meta_data.files[i].name);
                if not wrap_sys_save(fn, file_data[i]) then
                    -- just abort with fail without resetting the dirty flag
                    return false
                end
                state.dirty = false
            end
            -- keep persist entries
            save_meta.files[i] = meta_data.files[i]
            persist_count = persist_count + 1
       else
            -- save blanks for those that arent
            local src = meta_data.files[i]
            local out = {}
            out.name = src.name
            out.num_events = 0
            out.message_id = 0
            out.order = 0
            save_meta.files[i] = out
       end
    end

    -- if there was nothing to save and there is nothing to save, avoid
    -- disk talk altogether
    if force or last_persist_count ~= 0 or persist_count ~= 0 then
        if not wrap_sys_save(meta_fn, save_meta) then
            -- bail; no point in continuing here.
            tracking_enable = false
            return false
        else
            need_save = false
        end
    end
    last_persist_count = persist_count
    return true
end

function on_http_response()
end

function proto_headers()
    local hdr = {}
    hdr["x-gather-version"] = "2"
    hdr["x-app"] = app_id
    return hdr;
end

function on_request_failure()
    time_to_next_send = (1.0 + 0.5 * math.random()) * retry_timer
    retry_timer = retry_timer * 2
    local count = table.getn(meta_data.files)
    for i=1,count do
        file_state[i].persist = true
    end

    if not failing then
        save(true)
    end

    failing = true
end

function on_request_success()
    time_to_next_send = 0
    retry_timer = INITIAL_RETRY_TIME
    failing = false
end

function on_config_response(self, id, response)
    if response.status ~= 200 then
        on_request_failure();
    else
        server_config = json.decode(response.response)
        if server_config["stid_url"] and server_config["event_url"] then
            on_request_success();
    else
            -- go into fail mode.
            on_request_failure();
        end
    end
end

function on_stid_response(self, id, response)
    if response.status ~= 200 then
        on_request_failure();
    else
        meta_data.stid = response.response;
        on_request_success();
        -- now time to force save.
        save(true)
    end
end

local escapes = {
    ["\x22"] = "\\\"",
    ["\x5C"] = "\\",
    ["\x2F"] = "\\/",
    ["\x08"] = "\\b",
    ["\x0C"] = "\\f",
    ["\x0A"] = "\\n",
    ["\x0D"] = "\\r",
    ["\x09"] = "\\t"
}

function json_str(value)
    return "\"" .. string.gsub(value, ".", escapes) .. "\""
end

function json_field(name, value)
    return json_str(name) .. ":" .. value
end

function json_str_field(name, value)
    return json_str(name) .. ":" .. json_str(value)
end

function json_array(t, insert)
    local out = "["
    local sep = ""
    local n = table.getn(t)
    for i=1,n do
        out = out .. sep .. insert(t[i])
        sep = ","
    end
    return out .. "]"
end

function json_map(t, value_fn)
    local out = "{"
    local sep = ""
    for k,v in pairs(t) do
        out = out .. sep .. value_fn(k, v)
        sep = ","
    end
    return out .. "}"
end

function array_to_map(t, insert_fn)
    local n = table.getn(t)
    local out = { }
    for i=1,n do
        insert_fn(out, t[i])
    end
    return out
end

function json_event(evt)
    local mk_attr = function(obj, attr)
        obj[attr.key] = attr.value
    end
    local mk_metric = function(obj, attr)
        obj[attr.key] = attr.value
    end
    return "{" .. json_str_field("type", evt.type) .. "," ..
           json_field("time_stamp", evt.time_stamp) .. "," ..
           json_field("attributes", json_map(array_to_map(evt.attributes, mk_attr), json_str_field)) .. "," ..
           json_field("metrics", json_map(array_to_map(evt.metrics, mk_metric), json_field)) .. "}"
end

local msg_seq = 0

function send_events_file(idx)
    local data = meta_data.files[idx]

    if data.message_id == 0 then
        -- events must be batched with a message_id, and never be retransmitted
        -- with a different one, should the save fail here we must abort and not send.
        data.message_id = tostring(sess_start) .. "-" .. tostring(msg_seq)
        msg_seq = msg_seq + 1
        need_save = need_save or file_state[idx].persist
        if not save() then
            data.message_id = 0
            tracking_enable = false
            return
        end
    end



    local post_data = "{";
    for k,v in pairs(sys_field_mapping) do
        local sv = sys_info[v]

        -- A temporary compensation for the fact that we have another "fixup" in the defold/gather lib (https://github.com/defold/gather/blob/a05fa408b27abd52b69085654603b32bd4ac381a/src/main/java/com/king/gather/api/MessageConverter.java)
        if v == "system_name" then
            sv = convert_platform_name(sv)
        end

        if sv and sv ~= "" then
            post_data = post_data .. json_str_field(k, sv) .. ","
        end
    end

    post_data = post_data .. json_str_field("app_version", app_version) .. ","
    post_data = post_data .. json_str_field("defold_version", defold_version) .. ","

    local evt_data = json_array(file_data[idx].events, json_event)
    post_data = post_data .. json_field("events", evt_data) .. "}"

    local headers = proto_headers()
    headers["Content-Type"] = "application/json";
    headers["x-message-id"] = data.message_id
    headers["x-stid"] = meta_data.stid

    local on_event_response = function(s, id, response)
        if response.status and response.status ~= 0 then
            file_state[idx].dirty = false
            file_state[idx].persist = false
            need_save = true
            data.message_id = 0
            data.num_events = 0
            on_request_success()
        else
            on_request_failure()
        end
    end

    http.request(server_config["event_url"], "POST", on_event_response, headers, post_data);
end

function send_next()
    if not server_config then
        -- get config json containing the server urls
        http.request(config_url, "GET", on_config_response, proto_headers())
        return true
    elseif not meta_data.stid then
        local hdr = proto_headers()
        hdr["message-id"] = os.time()
        http.request(server_config["stid_url"], "GET", on_stid_response, proto_headers());
        return true
    else
        -- ordered will be the metadata file table but sorted
        -- with order>highest so events are always appended to the
        -- latest entry.
        local ordered = {}
        for k,v in pairs(meta_data.files) do
            ordered[k] = {}
            ordered[k].index = k
            ordered[k].value = v
        end
        local cmp = function(a, b)
            return a.value.order > b.value.order;
        end
        table.sort(ordered, cmp);
        for k,v in pairs(ordered) do
            local meta = v.value
            if meta.message_id ~= 0 then
                send_events_file(v.index)
                return true
            end
        end
        for k,v in pairs(ordered) do
            local meta = v.value
            if meta.message_id == 0 and meta.num_events > 0 then
                send_events_file(v.index)
                return true
            end
        end
    end
    return false
end

-- Invoked for every event message passed to tracking system
function on_event(event)
    if tracking_enable then
        event.time_stamp = os.time()
        insert_event(event)
    end
end

function update(dt)
    if not tracking_enable then
        return
    end
    if time_to_next_send >= 0 then
        time_to_next_send = time_to_next_send - dt
        if time_to_next_send <= 0 then
            if send_next() then
                time_to_next_send = -1
            else
                time_to_next_send = 0
            end
        end

    end
    time_to_next_save = time_to_next_save - dt
    if time_to_next_save <= 0 then
        time_to_next_save = SAVE_INTERVAL
        if need_save then
            save()
    end
    end
end

function finalize()
    if tracking_enable then
        save(true)
    end
end
