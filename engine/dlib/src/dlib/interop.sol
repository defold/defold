module interop

-- The data structures here are untouchable now from sol,
-- but provided so structs can get the correct size

struct Proxy
    local actual : handle
end

typealias local hv:uint32 as Handle

struct HashTable
    local hash_table      : handle
    local hash_table_size : uint32
    local ptr_0           : handle
    local ptr_1           : handle
    local ptr_2           : handle
    local free_entries    : uint32
    local count           : uint32
    local state           : uint16
end

struct Array
    local array_front    : handle
    local array_end      : handle
    local array_back     : handle
    local flags          : uint16
end

