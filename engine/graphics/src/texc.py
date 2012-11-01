#! /usr/bin/env python
import dds, pvrtc, sys, os
import tempfile, atexit
import subprocess
from subprocess import Popen, PIPE
import Image
from optparse import OptionParser
import graphics_ddf_pb2

def compile_uncompressed(source_name, source_image, texture_image):
    image = texture_image.alternatives.add()
    image.width, image.height = source_image.size
    image.original_width, image.original_height = source_image.size

    format_table = { 'L' : graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_LUMINANCE,
                     'RGB' : graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGB,
                     'RGBA' : graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA
                    }

    mip_maps = ""
    w, h = image.width, image.height
    current = source_image
    while not (w == 0 and h == 0):
        w = max(1, w)
        h = max(1, h)
        current = current.resize((w, h), Image.ANTIALIAS)
        data = current.tostring()
        image.mip_map_offset.append(len(mip_maps))
        image.mip_map_size.append(len(data))
        mip_maps += data
        w >>= 1
        h >>= 1

    image.format = format_table[source_image.mode]
    image.data = mip_maps

def compile_dds(source_name, source_image, texture_image):
    dds_file_no, dds_file = tempfile.mkstemp('.dds')
    os.close(dds_file_no)
    if sys.platform != 'win32':
        # TODO:
        # We get "The process cannot access the file because it is being used by another process"
        # on windows...
        atexit.register(lambda: os.remove(dds_file))

    p = Popen('nvcompress -bc3 -fast'.split() + [source_name, dds_file], stdout = PIPE, stderr = PIPE)
    out, err = p.communicate()
    if p.returncode != 0:
        print >>sys.stderr, err
        sys.exit(5)

    image = texture_image.alternatives.add()

    file = open(dds_file, "rb")
    source_image = dds.decode(file)
    format_table = {
        ('DXT1', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGB_DXT1,
        ('DXT1', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT1,
        ('DXT3', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT3,
        ('DXT3', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT3,
        ('DXT5', False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT5,
        ('DXT5', True):  graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_DXT5
    }
    image.format = format_table[(source_image.FourCC, source_image.Alpha)]

    file.close()
    image.width, image.height = source_image.Width, source_image.Height

    d = ""
    offset = 0
    for m in source_image.MipMaps:
        image.mip_map_offset.append(offset)
        image.mip_map_size.append(len(m))
        d += m
        offset += len(m)

    image.data = d

def compile_pvrtc(source_name, source_image, texture_image):
    pvrtc_file_no, pvrtc_file = tempfile.mkstemp('.pvr')
    os.close(pvrtc_file_no)

    if sys.platform != 'win32':
        # TODO:
        # We get "The process cannot access the file because it is being used by another process"
        # on windows...
        atexit.register(lambda: os.remove(pvrtc_file))

    p = Popen('PVRTexTool -m -fOGLPVRTC4 '.split() + ['-i' + source_name, '-o' + pvrtc_file], stdout = PIPE, stderr = PIPE)
    out, err = p.communicate()
    if p.returncode != 0:
        print >>sys.stderr, err
        sys.exit(5)

    image = texture_image.alternatives.add()

    file = open(pvrtc_file, "rb")
    source_image = pvrtc.decode(file)
    format_table = {
        (pvrtc.kPVRTextureFlagTypePVRTC_2, False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGB_PVRTC_2BPPV1,
        (pvrtc.kPVRTextureFlagTypePVRTC_4, False): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGB_PVRTC_4BPPV1,
        (pvrtc.kPVRTextureFlagTypePVRTC_2, True): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_PVRTC_2BPPV1,
        (pvrtc.kPVRTextureFlagTypePVRTC_4, True): graphics_ddf_pb2.TextureImage.TEXTURE_FORMAT_RGBA_PVRTC_4BPPV1,
    }
    image.format = format_table[(source_image.PixelFormat, source_image.Alpha)]

    file.close()
    image.width, image.height = source_image.Width, source_image.Height

    d = ""
    offset = 0
    for m in source_image.MipMaps:
        image.mip_map_offset.append(offset)
        image.mip_map_size.append(len(m))
        d += m
        offset += len(m)

    image.data = d

def is_pow2(x):
    return ((x != 0) and not (x & (x - 1)))

if __name__ == "__main__":
    usage = "usage: %prog [options] infile"
    parser = OptionParser(usage)
    parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
    (options, args) = parser.parse_args()
    if not options.output_file:
        parser.error("Output file not specified (-o)")

    texture_image = graphics_ddf_pb2.TextureImage()
    source_image = Image.open(args[0])
    if source_image.mode == 'P':
        source_image = source_image.convert('RGBA')
    width, height = source_image.size

#    compile_dds(args[0], source_image, texture_image)
#    # Only compile power of 2, min width/height 16 and square images
#    if is_pow2(width) and is_pow2(height) and width >= 16 and height >= 16 and width == height:
#        compile_pvrtc(args[0], source_image, texture_image)
#    else:
    # Fallback to uncompressed
    # TODO currently only compile uncompressed due to compression artifacts for pixel-sensitive images
    compile_uncompressed(args[0], source_image, texture_image)

    f = open(options.output_file, "wb")
    f.write(texture_image.SerializeToString())
    f.close()
