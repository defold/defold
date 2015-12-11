module message

struct URL
    socket     : uint32
    func       : int32
    path       : uint64
    fragment   : uint64
end

struct Message
    sender     : @URL
    receiver   : @URL
    id         : uint64
    handle     : uint64
    descriptor : uint64
    data_size  : int32
    next       : handle
end
