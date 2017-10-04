if iap == nil then
    iap = {}
end
iap.__facebook_helper_list = function(urls, cb)

    -- Clear list request response
    iap.__list_request = {
        urls    = urls,
        results = {},
        cb      = cb
    }

    -- Lookup table to convert from meta-property names into product field names used in Defold.
    local key_to_defold = {
        ["og:url"] = "ident",
        ["og:title"] = "title",
        ["og:description"] = "description",
        ["product:price:amount"] = "price",
        ["product:price:currency"] = "currency_code"
    }

    -- Snippet to create an case insensitive Lua pattern
    -- From: https://stackoverflow.com/a/11402486/129360
    local case_insensitive_pattern = function(pattern)

      -- find an optional '%' (group 1) followed by any character (group 2)
      local p = pattern:gsub("(%%?)(.)", function(percent, letter)

        if percent ~= "" or not letter:match("%a") then
          -- if the '%' matched, or `letter` is not a letter, return "as is"
          return percent .. letter
        else
          -- else, return a case-insensitive character class of the matched letter
          return string.format("[%s%s]", letter:lower(), letter:upper())
        end

      end)

      return p
    end

    -- Runs the callback with supplied error enum and string.
    local return_error = function(error, reason)
        iap.__list_request.cb(nil, {error = error, reason = reason})
    end

    -- Used to verify if a product entry has all the needed fields
    -- Returns a boolean success, if failed it also returns an error string
    local verify_product_entry = function (product)
        for k,v in pairs(key_to_defold) do
            if not product[v] then
                return false, "property '" .. tostring(k) .. "' could not be found"
            end
        end

        return true
    end

    -- Returns the <head> tag including contents, if not found it returns nil and an error string
    local head_pattern = case_insensitive_pattern("<head.+</head>")
    local find_head = function ( html )
        local r = string.match(html, head_pattern)
        if not r then
            return nil, "<head> not found"
        end

        return r
    end

    -- Parses the <head> content, looks for meta entries and returns a product entry table.
    -- If it doesn't find a valid product, it returns nil and an error string.
    local meta_pattern = case_insensitive_pattern("< -meta.-property -= -[\"'](.-)[\"'].-content -= -[\"'](.-)[\"']")
    local parse_head = function ( head )
        local r = {}
        for k,v in string.gmatch(head, meta_pattern) do
            k = string.lower(k)
            local k2 = key_to_defold[k]
            if k2 then

                -- if field is "price", lets cast it to a number directly here for ease before passing it back to the user
                if k2 == "price" then
                    v = tonumber(v)
                end
                r[k2] = v
            end
        end

        -- verify that we got all product properties needed
        local success, err = verify_product_entry(r)
        if success then
            -- concat price and currency_code (see library_facebook_iap.js:64)
            r.price_string = tostring(r.price) .. tostring(r.currency_code)
            return r, nil
        else
            return nil, err
        end
    end

    -- Parses the HTML body for a valid product entry.
    -- Returns the product entry table on success, on failure it returns nil and an error string.
    local parse_product = function(body)
        local head, err = find_head(body)
        if not err then
            return parse_head(head)
        else
            return nil, err
        end
    end

    -- forward decl req_next needed inside res_func
    local req_next

    -- Callback for HTTP results
    -- Checks if the response was valid (200), then proceeds to parse the HTML for a valid product entry.
    -- If a valid product is found, it will continue with a new HTTP request for the next product URL.
    -- If status is not valid, or no valid product entry was found, it will call the callback with error enum and reason string.
    local res_func = function(self, id, response)
        if (response.status == 200) then
            local product, error = parse_product(response.response)
            if not error then
                table.insert(iap.__list_request.results, product)
            else
                return return_error(iap.REASON_UNSPECIFIED, "Could not parse product '" .. tostring(iap.__list_request.current) .. "': " .. error)
            end
        else
            -- could not get product, run callback with error+reason
            return return_error(iap.REASON_UNSPECIFIED, "Could not get product URL '" .. tostring(iap.__list_request.current) .. "', HTTP request status: " .. tostring(response.status))
        end

        req_next()
    end

    -- Pops a product URL from the urls list and does a HTTP request.
    -- If the urls list is empty, it will call the result callback with the results list instead.
    req_next = function()
        if (#iap.__list_request.urls > 0) then
            iap.__list_request.current = table.remove(iap.__list_request.urls, 1)
            http.request(iap.__list_request.current, "GET", res_func)
        else
            iap.__list_request.cb(iap.__list_request.results, nil)
        end
    end

    -- Do an initial call to request the first product URL.
    req_next()
end
