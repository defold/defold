module hash

struct ReverseHashEntry
    local value  : handle
    local length : uint32
end

struct HashState32
    hash  : uint32
    tail  : uint32
    count : uint32
    size  : uint32
    local rev : @ReverseHashEntry
end

struct HashState64
    hash  : uint64
    tail  : uint64
    count : uint64
    size  : uint64
    local rev : @ReverseHashEntry
end

!symbol("SolHashString64")
extern hash_string64(s:String):uint64
!symbol("SolHashString32")
extern hash_string32(s:String):uint32
!symbol("SolHashInit32")
extern init(state:HashState32)
!symbol("SolHashUpdate32")
extern update(state:HashState32, obj:any)
!symbol("SolHashFinal32")
extern final(state:HashState32):uint32
