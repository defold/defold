# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
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

import maya.OpenMaya as OpenMaya

# Utility script to create a grid in Maya

def float_point_array(lst):
    a = OpenMaya.MPointArray()
    for tmp in lst:
        if len(tmp) == 2:
            x,y = tmp
            z = 0
        else:
            x, y, z = tmp

        a.append(x, y, z)
    return a

UVS = [(0, 1), (0, 0), (1, 0), (1, 1), (0.5, 0.5)]

def add_tri(mesh, lst, uv):
    mesh.addPolygon(float_point_array(lst), False)

    i = mesh.numPolygons() - 1
    a = OpenMaya.MIntArray()
    mesh.getPolygonVertices(i, a)

    for j in range(3):
        mesh.setUV(i * 3 + j, UVS[uv[j]][0], UVS[uv[j]][1])

    for j in range(3):
        mesh.assignUV(i, j, i * 3 + j)

def create_grid():
    mesh = OpenMaya.MFnMesh()

    NX = 18
    NY = 10
    width = 60 * NX
    height = 60 * NY
    NSPLIT = 16
    t = thickness = 1.5

    for j in range(NY):
        for i in range(NX):
            x = width * i / (NX - 1.0)
            y = -height * j / (NY - 1.0) + height
            xp = width * (i+1.0) / (NX - 1.0)
            yp = -height * (j+1.0) / (NY - 1.0) + height

            # four triangles around cross sections

            # left
            if i == 0:
                add_tri(mesh,
                        [(x, y), (x - t, y + t), (x - t, y - t)],
                        [4, 0, 3])
            else:
                add_tri(mesh,
                        [(x, y), (x - t, y + t), (x - t, y - t)],
                        [4, 0, 1])

            # top
            if j == 0:
                add_tri(mesh,
                        [(x, y), (x + t, y + t), (x - t, y + t)],
                        [4, 0, 3])
            else:
                add_tri(mesh,
                        [(x, y), (x + t, y + t), (x - t, y + t)],
                        [4, 0, 1])

            # right
            if i == NX-1:
                add_tri(mesh,
                        [(x, y), (x + t, y - t), (x + t, y + t)],
                        [4, 0, 3])
            else:
                add_tri(mesh,
                        [(x, y), (x + t, y - t), (x + t, y + t)],
                        [4, 1, 0])

            # bottom
            if j == NY-1:
                add_tri(mesh,
                        [(x, y), (x - t, y - t), (x + t, y - t)],
                        [4, 0, 3])
            else:
                add_tri(mesh,
                        [(x, y), (x - t, y - t), (x + t, y - t)],
                        [4, 1, 0])

            if i < (NX-1):
                # connect horizontally
                for k in range(NSPLIT):
                    startx = x + t
                    endx = xp - t

                    x0 = startx + (endx - startx) * float(k) / float(NSPLIT)
                    x1 = startx + (endx - startx) * float(k + 1) / float(NSPLIT)
                    add_tri(mesh,
                            [(x0, y - t), (x1, y - t), (x1, y + t)],
                            [1, 2, 3])

                    add_tri(mesh,
                            [(x0, y - t), (x1, y + t), (x0, y + t)],
                            [0, 1, 2])

            # connect vertically
            if j < (NY-1):
                for k in range(NSPLIT):
                    starty = y - t
                    endy = yp + t

                    y0 = starty + (endy - starty) * float(k) / float(NSPLIT)
                    y1 = starty + (endy - starty) * float(k + 1) / float(NSPLIT)

                    add_tri(mesh,
                            [(x - t, y0), (x - t, y1), (x + t, y1)],
                            [1, 2, 3])

                    add_tri(mesh,
                            [(x - t, y0), (x + t, y1), (x + t, y0)],
                            [0, 1, 2])

    for i in range(mesh.numEdges()):
        mesh.setEdgeSmoothing(i, False)

def create_lines(x_y):
    mesh = OpenMaya.MFnMesh()
    NX = 18
    NY = 10
    width = 60 * NX
    height = 60 * NY
    NSPLIT = 128
    t = thickness = 1.5

    if not x_y:
        i = 0
        for j in range(NY):
            x0 = 0
            y = -height * j / (NY - 1.0) + height
            print y
            for k in range(NSPLIT):
                startx = 0
                endx = width

                x0 = startx + (endx - startx) * float(k) / float(NSPLIT)
                x1 = startx + (endx - startx) * float(k + 1) / float(NSPLIT)
                add_tri(mesh,
                        [(x0, y - t, y), (x1, y - t, y), (x1, y + t, y)],
                        [1, 2, 3])

                add_tri(mesh,
                        [(x0, y - t, y), (x1, y + t, y), (x0, y + t, y)],
                        [0, 1, 2])
    else:
        j = 0
        for i in range(NX):
            y0 = 0
            x = width * i / (NX - 1.0)
            print x
            for k in range(NSPLIT):
                starty = 0
                endy = height

                y0 = starty + (endy - starty) * float(k) / float(NSPLIT)
                y1 = starty + (endy - starty) * float(k + 1) / float(NSPLIT)

                add_tri(mesh,
                        [(x - t, y0, x), (x - t, y1, x), (x + t, y1, x)],
                        [1, 2, 3])

                add_tri(mesh,
                        [(x - t, y0, x), (x + t, y1, x), (x + t, y0, x)],
                        [0, 1, 2])

        print height

#create_grid()
create_lines(True)
create_lines(False)
