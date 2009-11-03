#! /usr/bin/env python

import sys
from optparse import OptionParser
import Image, ImageFont, ImageDraw
import render_ddf_pb2

def Layout(width, characters, image_font):
    font = render_ddf_pb2.Font()

    height = 0
    line_height = 0
    x0, y0 = 0, 0
    for c in characters:
        w, h = image_font.getsize(c)
        if width - x0 < w:
            x0 = 0
            y0 += line_height 
            line_height = 0

        g = font.Glyphs.add()
        g.X = x0
        g.Y = y0
        g.Width = w
        g.Height = h

        x0 += w

        line_height = max(line_height, h)

    height = y0 + line_height
    font.ImageWidth = width
    font.ImageHeight = height
    return font
        
def Compile(font_name, font_size, output_file, vertex_program, fragment_program):
    characters = "".join([ chr(x) for x in range(255) ])

    image_font = ImageFont.truetype(font_name, font_size)
    font = Layout(1024, characters, image_font)
    font.VertexProgram = vertex_program
    font.FragmentProgram = fragment_program
    image = Image.new("RGBA", (font.ImageWidth, font.ImageHeight))
    draw = ImageDraw.Draw(image)

    x = 0
    for i,g in enumerate(font.Glyphs):
        c = chr(i)
        draw.text((g.X, g.Y), c, font=image_font)
    
    image = image.convert("L")
    font.ImageData = image.tostring()
    f = open(output_file, "wb")
    f.write(font.SerializeToString())
    f.close()
    image.save("foo.png", "png")

usage = "usage: %prog [options] font.ttf"
parser = OptionParser(usage)
parser.add_option("-o", dest="output_file", help="Output file", metavar="OUTPUT")
parser.add_option("-s", dest="size", help="Size", metavar="SIZE")
parser.add_option("--vp", dest="vertex_program", help="Vertex program", metavar="VERTEXPROGRAM")
parser.add_option("--fp", dest="fragment_program", help="Fragment program", metavar="FRAGMENTPROGRAM")

(options, args) = parser.parse_args()
if not options.output_file:
    parser.error("Output file not specified (-o)")

if not options.size:
    parser.error("Size not specified (-s)")

if not options.vertex_program:
    parser.error("Vertex program not specified (--vp)")

if not options.fragment_program:
    parser.error("Fragment program not specified (--fp)")

Compile(args[0], int(options.size), options.output_file, options.vertex_program, options.fragment_program)




