module indexpool

struct IndexPool
    free_list : [int32]
    alloc_ptr : int32
end

function make(entries:uint32):IndexPool
    local p = IndexPool {
            free_list = [entries:int32],
            alloc_ptr = 0
    }
    for i=0,entries do
            p.free_list[i] = i
    end
    return p
end

function alloc(pool:IndexPool):int32
    if pool.alloc_ptr < #pool.free_list then
            local k = pool.alloc_ptr
            pool.alloc_ptr = pool.alloc_ptr + 1
            return pool.free_list[k]
    else
            return -1
    end
end

function free(pool:IndexPool, index:int32)
    local k = pool.alloc_ptr - 1
    pool.free_list[k] = index
    pool.alloc_ptr = k
end
