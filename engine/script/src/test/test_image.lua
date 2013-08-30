function test_image()
    local file = io.open("src/test/data/color_check_2x2.png",  "rb")
    assert(file ~= nil)
    local buf = file:read("*all")
    local img = image.load(buf)
    assert(img.width == 2)
    assert(img.height == 2)
    assert(#img.buffer == 2 * 2 * 4)
    assert(img.type == image.TYPE_RGBA)
    file:close()
end

functions = { test_image = test_image }
