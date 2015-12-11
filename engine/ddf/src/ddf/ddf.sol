module ddf

require C
require reflect
require io

-------------------------------------------------
-- WARNING: These strings are not sol strings
-- and bad things will happen when you use them
--
-- TODO: Maybe make these structs empty so they
-- can ony be used as pointers?
-------------------------------------------------

struct FieldDescriptor
    name               : String
    flags              : uint32
    message_descriptor : Descriptor
    offset             : uint32
    default            : String
end

struct Descriptor
    major_version : uint16
    minor_version : uint16
    name          : String
    name_hash     : uint64
    size          : uint32
    fields        : [@FieldDescriptor]
    field_count   : uint8
    next          : Descriptor
end

local RESULT_OK:int = 0
local RESULT_FIELDTYPE_MISMATCH:int = 1
local RESULT_WIRE_FORMAT_ERROR:int = 2
local RESULT_IO_ERROR:int = 3
local RESULT_VERSION_MISMATCH:int = 4
local RESULT_MISSING_REQUIRED:int = 5
local RESULT_INTERNAL_ERROR:int = 1000

struct LoadMessageResult
        message   : any
end

function from_buffer(type:reflect.Type, buffer:[uint8], offset:uint32, length:uint32):any
        local msg:LoadMessageResult = LoadMessageResult { }
        local res:uint32 = load_message_internal(buffer, offset, length, type, msg)
        return msg.message
end

--- internal

!symbol("SolDDFRegisterMessageType")
extern register_message_type_internal(descriptor:Descriptor, type:reflect.Type, array_type:reflect.Type)

!symbol("SolDDFGetTypeFromHash")
extern get_type_from_hash(hash:uint64):reflect.Type

function register_message_type(descriptor:Descriptor, type:reflect.Type, array_type:reflect.Type):bool
    register_message_type_internal(descriptor, type, array_type)
    return true
end

!symbol("SolDDFLoadMessage")
extern load_message_internal(buffer:[uint8], offset:uint32, length:uint32, type:reflect.Type, result:LoadMessageResult):uint32
