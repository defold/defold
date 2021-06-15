
print("testing callstack generation")

local function testfn_with_a_very_long_name_3(value1, value2, value3)
    return testmodule.foo(value1, value2, value3)
end

local function testfn_with_a_very_long_name_2(value1, value2)
    testfn_with_a_very_long_name_3(value1, value2, value1 + value2)
end

local function testfn_with_a_very_long_name_(value)
    if value == 0 then
        testfn_with_a_very_long_name_2(value*2, value*3+1)
    else
        testfn_with_a_very_long_name_(value - 1)
    end
end

function do_crash()
    print("OUTPUT:", testfn_with_a_very_long_name_(200))
end

