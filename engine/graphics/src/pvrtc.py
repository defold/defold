# Copyright 2020 The Defold Foundation
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
# 
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
# 
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

import sys, struct

# Stolen and slightly modified (DDSException to Exception) from dds.py
class _filestruct(object):
    def __init__(self, data):
        if len(data) < self.get_size():
            raise Exception('Not a pvrtc file')
        items = struct.unpack(self.get_format(), data)
        for field, value in map(None, self._fields, items):
            setattr(self, field[0], value)

    def __repr__(self):
        name = self.__class__.__name__
        return '%s(%s)' % \
            (name, (', \n%s' % (' ' * (len(name) + 1))).join( \
                      ['%s = %s' % (field[0], repr(getattr(self, field[0]))) \
                       for field in self._fields]))

    @classmethod
    def get_format(cls):
        return '<' + ''.join([f[1] for f in cls._fields])

    @classmethod
    def get_size(cls):
        return struct.calcsize(cls.get_format())

class PVRTexHeader(_filestruct):
    _fields = [
        ('headerLength', 'I'),
        ('height', 'I'),
        ('width', 'I'),
        ('numMipmaps', 'I'),
        ('flags', 'I'),
        ('dataLength', 'I'),
        ('bpp', 'I'),
        ('bitmaskRed', 'I'),
        ('bitmaskGreen', 'I'),
        ('bitmaskBlue', 'I'),
        ('bitmaskAlpha', 'I'),
        ('pvrTag', '4s'),
        ('numSurfs', 'I')
    ]

    def __init__(self, data):
        super(PVRTexHeader, self).__init__(data)

class PVRTCImage(object):
    def __init__(self, width, height, four_cc, alpha, pixel_format, mip_maps):
        self.Width = width
        self.Height = height
        self.FourCC = four_cc
        self.Alpha = alpha
        self.PixelFormat = pixel_format
        self.MipMaps = mip_maps

PVR_TEXTURE_FLAG_TYPE_MASK = 0xff
kPVRTextureFlagTypePVRTC_2 = 24
kPVRTextureFlagTypePVRTC_4 = 25

def decode(file):
    header_data = file.read(PVRTexHeader.get_size())
    header = PVRTexHeader(header_data)

    if header.pvrTag != 'PVR!':
        raise Exception('Invalid pvrtc header tag: %s' % header.pvrTag)

    pixel_format = header.flags & PVR_TEXTURE_FLAG_TYPE_MASK

    mips = []

    if not (pixel_format == kPVRTextureFlagTypePVRTC_2 or pixel_format == kPVRTextureFlagTypePVRTC_4):
        raise Exception('Unsupported pvrtc format %d' % pixel_format)

    has_alpha = (header.flags & 0x8000) == 0x8000
    bytes = file.read()
    data_offset = 0
    data_length = header.dataLength
    width, height = header.width, header.height

    while data_offset < data_length:
        if pixel_format == kPVRTextureFlagTypePVRTC_4:
            block_size = 4 * 4
            width_blocks = width / 4
            height_blocks = height / 4
            bpp = 4
        else:
            block_size = 8 * 4
            width_blocks = width / 8
            height_blocks = height / 4
            bpp = 2

        width_blocks = max(width_blocks, 2)
        height_blocks = max(height_blocks, 2)

        data_size = width_blocks * height_blocks * ((block_size  * bpp) / 8)
        mips.append(bytes[data_offset:(data_offset+data_size)])
        data_offset += data_size
        width = max(width >> 1, 1)
        height = max(height >> 1, 1)

    return PVRTCImage(header.width, header.height, header.pvrTag, has_alpha, pixel_format, mips)

if __name__ == '__main__':
    f = open(sys.argv[1], 'rb')
    image = decode(f)
    print image.Width, image.Height, image.Alpha
    for i, m in enumerate(image.MipMaps):
        print i, len(m)

