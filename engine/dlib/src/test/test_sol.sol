module test_sol

require interop

struct InteropStruct1Array
    array : @interop.Array
end

struct InteropStruct2Array
    array1 : @interop.Array
    array2 : @interop.Array
end

struct InteropStruct1HashTable
    hash1 : @interop.HashTable
end

struct InteropStruct2HashTable
    hash1 : @interop.HashTable
    hash2 : @interop.HashTable
end

struct InteropStruct1Array1HashTable
    array  : @interop.Array
    hash   : @interop.HashTable
end

!export("test_sol_alloc_item")
function test_sol_alloc_item(which:int):any
    if which == 0 then
        return InteropStruct1Array { }
    elseif which == 1 then
        return InteropStruct2Array { }
    elseif which == 2 then
        return InteropStruct1HashTable { }
    elseif which == 3 then
        return InteropStruct2HashTable { }
    elseif which == 4 then
        return InteropStruct1Array1HashTable { }
    end
    local tmp:handle = nil
    return tmp
end

