-- Copyright 2020-2026 The Defold Foundation
-- Copyright 2014-2020 King
-- Copyright 2009-2014 Ragnar Svensson, Christian Murray
-- Licensed under the Defold License version 1.0 (the "License"); you may not use
-- this file except in compliance with the License.
--
-- You may obtain a copy of the License, together with FAQs at
-- https://www.defold.com/license
--
-- Unless required by applicable law or agreed to in writing, software distributed
-- under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
-- CONDITIONS OF ANY KIND, either express or implied. See the License for the
-- specific language governing permissions and limitations under the License.

function string.tohex(str)
    return (str:gsub('.', function (c)
        return string.format('%02X', string.byte(c))
    end))
end


local img_expected_bytes_rgba = {
    0xFF, 0x00, 0x00, 0xFF,
    0x00, 0xFF, 0x00, 0xD5,
    0x00, 0x00, 0xFF, 0x57,
    0xFF, 0xFF, 0xFF, 0x00 }

local img_expected_bytes_rgb = {
    0xFF, 0x00, 0x00,
    0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFF,
    0xFF, 0xFF, 0x00}

local img_expected_bytes_png_premultiplied = {
    0xFF, 0x00, 0x00, 0xFF,
    0x00, 0xD5, 0x00, 0xD5,
    0x00, 0x00, 0x57, 0x57,
    0x00, 0x00, 0x00, 0x00 }

local img_expected_bytes_jpg = {
    0xFE, 0x00, 0x00,
    0x00, 0xFF, 0x00,
    0x00, 0x00, 0xFE,
    0xFF, 0xFF, 0x01}

local function verify_image(img, bpp, image_type, expected_bytes)
    assert(img.width == 2)
    assert(img.height == 2)
    assert(#img.buffer == 2 * 2 * bpp)
    assert(img.type == image_type)

    local bytes = { string.byte(img.buffer, 1,-1) }

    for i = 1,#bytes do
        if expected_bytes[i] ~= bytes[i] then
            print("image   ", string.tohex(img.buffer))
            print("diff bytes", i, expected_bytes[i], bytes[i])
        end
        assert(expected_bytes[i] == bytes[i])
    end
end

local function test_image(path, bpp, image_type, premultiply, expected_bytes)
    local file = io.open(path, "rb")
    assert(file ~= nil)
    local buf = file:read("*all")
    file:close()

    print("Loading", path)
    local img = image.load(buf, premultiply)
    verify_image(img, bpp, image_type,  expected_bytes)

end

function test_images(mountfs)
    test_image(mountfs .. "src/gamesys/test/image/color_check_2x2.png.raw", 4, image.TYPE_RGBA, false, img_expected_bytes_rgba)
    test_image(mountfs .. "src/gamesys/test/image/color_check_2x2.png.raw", 4, image.TYPE_RGBA, true, img_expected_bytes_png_premultiplied)
    test_image(mountfs .. "src/gamesys/test/image/color_check_2x2.jpg.raw", 3, image.TYPE_RGB, false, img_expected_bytes_jpg)
    test_image(mountfs .. "src/gamesys/test/image/color_check_2x2_16.png.raw", 4, image.TYPE_RGBA, false, img_expected_bytes_rgba) -- 16 bit
    test_image(mountfs .. "src/gamesys/test/image/color_check_2x2_indexed.png.raw", 3, image.TYPE_RGB, false, img_expected_bytes_rgb) -- 3 color palette
end


functions = { test_images = test_images }
