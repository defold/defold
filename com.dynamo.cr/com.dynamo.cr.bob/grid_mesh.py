import maya.OpenMaya as OpenMaya

# Utility script to create a grid in Maya

def create():
    mesh = OpenMaya.MFnMesh()
    face_index = xutil.createFromInt(0)

    NX = 18
    NY = 10
    width = 60 * NX
    height = 60 * NY
    t = thickness = 3

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
                add_tri(mesh,
                        [(x + t, y - t), (xp - t, y - t), (xp - t, y + t)],
                        [1, 2, 3])

                add_tri(mesh,
                        [(x + t, y - t), (xp - t, y + t), (x + t, y + t)],
                        [0, 1, 2])

            # connect vertically
            if j < (NY-1):
                add_tri(mesh,
                        [(x - t, y - t), (x - t, yp + t), (x + t, yp + t)],
                        [1, 2, 3])

                add_tri(mesh,
                        [(x - t, y - t), (x + t, yp + t), (x + t, y - t)],
                        [0, 1, 2])

    for i in range(mesh.numEdges()):
        mesh.setEdgeSmoothing(i, False)

create()
