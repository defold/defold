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

import sys, os
import mesh_ddf_pb2, material_ddf_pb2, model_ddf_pb2

from pyglet.gl import *

# https://www-viz.tamu.edu/students/ericp/glstate.html

class ModelViewer(pyglet.window.Window):
    def __init__(self, file_name):
        super(ModelViewer, self).__init__(800, 600, resizable = True, caption = "Model Viewer")

        self.AngleX = 0
        self.AngleY = 0
        self.CameraZ = -6

        glEnable(GL_DEPTH_TEST)
        self.LoadModel(file_name)

    def LoadModel(self, file_name):
        content_dir = os.path.dirname(file_name)
        content_dir = os.path.join(content_dir, '..')
        model = model_ddf_pb2.ModelDesc()
        model.ParseFromString(open(file_name, "rb").read())

        self.Mesh = mesh_ddf_pb2.Mesh()
        self.Mesh.ParseFromString(open(os.path.join(content_dir, model.Mesh), "rb").read())

        material_file = os.path.join(content_dir, model.Material)
        self.Material = material_ddf_pb2.MaterialDesc()
        self.Material.ParseFromString(open(material_file, "rb").read())

        self.VertexShader = self.LoadShader(os.path.join(content_dir, self.Material.VertexProgram) +  ".vpc", GL_VERTEX_PROGRAM_ARB)
        self.FragmentShader = self.LoadShader(os.path.join(content_dir, self.Material.FragmentProgram) + ".fpc", GL_FRAGMENT_PROGRAM_ARB)

        self.Texture0 = None
        # New texture format not yet supported
        #if model.Texture0:
        #    self.Texture0 = pyglet.image.load(os.path.join(content_dir, model.Texture0)).get_texture()
        #else:
        #    self.Texture0 = None

        self.CreateMesh()

    def CreateMesh(self):
        vertices = []
        normals = []
        texcoord0 = []
        for i in range(self.Mesh.PrimitiveCount):
            i1,i2,i3 = self.Mesh.Indices[i*3 + 0], self.Mesh.Indices[i*3 + 1], self.Mesh.Indices[i*3 + 2]
            for j in [i1,i2,i3]:
                vertices += [ self.Mesh.Positions[j*3+0], self.Mesh.Positions[j*3+1], self.Mesh.Positions[j*3+2] ]
                normals += [ self.Mesh.Normals[j*3+0], self.Mesh.Normals[j*3+1], self.Mesh.Normals[j*3+2] ]
                if self.Mesh.Texcoord0:
                    texcoord0 += [ self.Mesh.Texcoord0[j*2+0], self.Mesh.Texcoord0[j*2+1] ]

        self.VerticesGL = (GLfloat * len(vertices))(*vertices)
        self.NormalsGL = (GLfloat * len(normals))(*normals)
        if self.Mesh.Texcoord0:
            self.Texcoord0GL = (GLfloat * len(texcoord0))(*texcoord0)
        else:
            self.Texcoord0GL = None

    def LoadShader(self, file_name, type):
        glEnable(type)
        shader = (GLuint * 1)()
        glGenProgramsARB(1, shader)
        glBindProgramARB(type, shader[0])
        program = open(file_name).read()
        glProgramStringARB(type, GL_PROGRAM_FORMAT_ASCII_ARB, len(program), program)
        glDisable(type)
        return shader[0]

    def on_resize(self, width, height):
        glViewport(0, 0, width, height)
        glMatrixMode(GL_PROJECTION)
        glLoadIdentity()
        gluPerspective(60., width / float(height), .1, 1000.)
        glMatrixMode(GL_MODELVIEW)
        return pyglet.event.EVENT_HANDLED

    def on_key_press(self, symbol, modifiers):
        if symbol == pyglet.window.key.ESCAPE:
            sys.exit(0)
        elif symbol == pyglet.window.key.SPACE:
            mode = (GLint * 1)()
            glGetIntegerv(GL_POLYGON_MODE, mode)
            if mode[0] == GL_FILL:
                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
            else:
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)

    def on_mouse_drag(self, x, y, dx, dy, buttons, modifiers):
        if modifiers & pyglet.window.key.MOD_CTRL:
            self.CameraZ += dy*0.1
        else:
            self.AngleX -= dy * 0.2
            self.AngleY += dx * 0.2

    def on_draw(self):
        glClearColor(0.6, 0.6, 0.6, 0)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        glTranslatef(0, 0, self.CameraZ)
        glRotatef(self.AngleX, 1, 0, 0)
        glRotatef(self.AngleY, 0, 1, 0)

        view_proj = (GLdouble * 16)()
        world = (GLdouble * 16)()

        glGetDoublev(GL_PROJECTION_MATRIX, view_proj)
        glGetDoublev(GL_MODELVIEW_MATRIX, world)

        for i in range(0,16,4):
            index = i / 4
            glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 0+index, view_proj[i], view_proj[i+1], view_proj[i+2], view_proj[i+3])
            glProgramLocalParameter4fARB(GL_VERTEX_PROGRAM_ARB, 4+index, world[i], world[i+1], world[i+2], world[i+3])

        glEnable(GL_VERTEX_PROGRAM_ARB)
        glBindProgramARB(GL_VERTEX_PROGRAM_ARB, self.VertexShader)
        glEnable(GL_FRAGMENT_PROGRAM_ARB)
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, self.FragmentShader)
        for p in self.Material.FragmentParameters:
            glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB, p.Register, p.Value.x, p.Value.y, p.Value.z, p.Value.w)

        # Draw model
        glColor3f(0.5,0.5,1.0)
        glEnableClientState(GL_VERTEX_ARRAY)
        glEnableClientState(GL_NORMAL_ARRAY)
        if self.Texcoord0GL and self.Texture0:
            glEnableClientState(GL_TEXTURE_COORD_ARRAY)
            glActiveTexture(GL_TEXTURE0_ARB);
            glBindTexture(self.Texture0.target, self.Texture0.id)

        glVertexPointer(3, GL_FLOAT, 0, self.VerticesGL)
        glNormalPointer(GL_FLOAT, 0, self.NormalsGL)
        if self.Texcoord0GL and self.Texture0:
            glTexCoordPointer(2, GL_FLOAT, 0, self.Texcoord0GL)
        glDrawArrays(GL_TRIANGLES, 0, len(self.VerticesGL) // 3)

        glDisable(GL_FRAGMENT_PROGRAM_ARB)
        glDisable(GL_VERTEX_PROGRAM_ARB)

        # Draw grid
        grid_size = 10
        grid_y0 = -1
        glColor3f(0.4, 0.4, 0.4)
        glBegin(GL_LINES)
        for i in range(-grid_size, grid_size+1):
            x = i
            glVertex3f(x, grid_y0, grid_size)
            glVertex3f(x, grid_y0, -grid_size)
            glVertex3f(grid_size, grid_y0, x)
            glVertex3f(-grid_size, grid_y0, x)
        glEnd()


model_viewer = ModelViewer(sys.argv[1])
pyglet.app.run()
