#! /usr/bin/env python
import dds, sys
from optparse import OptionParser
import graphics_ddf_pb2

if __name__ == "__main__":
    usage = "usage: %prog [options] infile"
    parser = OptionParser(usage)
    parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error("Output file not specified (-o)")

    image = dds.decode(open(args[0], "rb"))

    texture = graphics_ddf_pb2.TextureImage()
    texture.width, texture.height = image.Width, image.Height

    format_table = {
        ('DXT1', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGB_DXT1,
        ('DXT1', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT1,
        ('DXT3', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT3,
        ('DXT3', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT3,
        ('DXT5', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT5,
        ('DXT5', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT5
    }

    texture.format = format_table[(image.FourCC, image.Alpha)]
    d = ""
    offset = 0
    for m in image.MipMaps:
        texture.mip_map_offset.append(offset)
        texture.mip_map_size.append(len(m))
        d += m
        offset += len(m)

    texture.data = d

    f = open(options.output_file, "wb")
    f.write(texture.SerializeToString())
    f.close()

