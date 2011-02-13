# --------------------------------------------------------------------------
# Illusoft Collada 1.4 plugin for Blender
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2006: Illusoft - colladablender@illusoft.com
#    - 2008.08: multiple bugfixes by migius (AKA Remigiusz Fiedler)
#    - 2009.05: bugfixes by jan (AKA Jan Diederich)
#    - 2009.08: bugfixed by nico (AKA Nicolai Wojke, Labor Bilderkennen Uni-Koblenz)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License,
# or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ***** END GPL LICENCE BLOCK *****
# --------------------------------------------------------------------------

# History
# 2009.08.22 by nico:
# - Fixed a bug where visual scene nodes containing instances of nodes in the nodes library, 
#   which themselves instantiate geometry in the geometry library, where not imported
#   correctly (only the node instantiation was created, not the geometry)
# - Fixed a bug where nodes in the nodes library that have children instantiating other
#   nodes in the nodes library where not resolved properly. Added a post-library-creation
#   phase where DaeInstance object references are updated after the entire library is created
# - Changed nodes library syntax from 'library_NODES' to 'library_nodes'
# 2009.05.17 by jan:
# - More information for the user if an error happened (wrong/missing parenting).
# - Added a progress bar for export (bar for import already exists). 
# 2009.05.11 by jan:
# - Perfected the id renaming to fulfill at 100.0% the COLLADA standard for allowed UTF-8 
#	chars. The new renaming method is also much faster then the old one (benchmarked with
#	Python 2.5).
# - Fixed the skeleton/joint controller!
#	(Corrected the referencing of the 
#	"<skin><source><technique_common>,<accessor source="...">"
#	attribute. It pointed to its <source> node, and not the array in <source>.) 
# 2009.04.16 by jan:
# - Added the possibility to export models with modifiers (mirrors, transformers, etc.).
# - Added helpful messages for users in case something goes wrong, so they know how and
#	where to fix their model.
# - The patch from 2008.08.31, implementing the patch from Dmitri was incomplete.
#	The joints were renamed (replace('.','_')), but not the vertex group names!
# 2008.09.20 by migius:
# - bugfix meshes with more than 16 materials: material index bigger than 15 replaced with 15.
# 2008.08.31 by migius:
# - added support for import IPOs interpolation type: LINEAR,BEZIER
# - include patch jointVertexWeight from Dmitri: http://projects.blender.org/tracker/index.php?func=detail&aid=17427
# - non-armature-animation export/import correted
# - still buggy: armatures-position and armature-animation export&import
# 2008.08.04 by migius:
# - bugfix/refactor localTransformMatrix usage in hierarchies:
#   it preserves local orientation for each child, so imported IPOs are correct working
# 2008.05.08 by migius: modif. for debug mode

from cutils import *
import collada
import Blender
from Blender.Mathutils import *
import math
import datetime
from helperObjects import *

import BPyMesh
import BPyObject

debprn = 0 #--- print debug "print 'deb: ..."
dmitri = 0 #switch for testing patch from Dmitri

class Translator(object):
	isImporter = False

	def __init__(self, isImporter, version, debugM, fileName, _useTriangles, \
				_usePolygons, _bakeMatrices, _exportSelection, _createNewScene, \
				_clearScene, _lookAt, _exportPhysics, _exportCurrentScene, \
				_exportRelativePaths, _useUV, _sampleAnimation, _onlyMainScene, \
				_applyModifiers):
		
		global __version__, debugMode, usePhysics, useTriangles, usePolygons, \
		bakeMatrices, exportSelection, createNewScene, clearScene, lookAt, \
		replaceNames, exportPhysics, exportCurrentScene, useRelativePaths, \
		useUV, sampleAnimation, onlyMainScene, applyModifiers
		
		#if debprn: print 'deb:class_Translator isImporter=', isImporter #deb---------
		__version__ = version
		debugMode = debugM
		usePhysics = None
		useTriangles = _useTriangles
		usePolygons = _usePolygons
		bakeMatrices = _bakeMatrices
		exportSelection = _exportSelection
		createNewScene = _createNewScene
		clearScene = _clearScene
		lookAt = _lookAt
		exportPhysics = _exportPhysics
		usePhysics = _exportPhysics
		exportCurrentScene = _exportCurrentScene
		useRelativePaths = _exportRelativePaths
		useUV = _useUV
		sampleAnimation = _sampleAnimation
		onlyMainScene= _onlyMainScene
		applyModifiers= _applyModifiers

		replaceNames = clearScene

		self.isImporter = isImporter
		self.fileName = ''
		#if debprn: print 'deb:class_Translator fileName=', fileName #deb---------
		if self.isImporter:
			self.__Import(fileName)
		else:
			self.__Export(fileName)

	def __Import(self,fileName=''):
		#if debprn: print 'deb:translator__Import fileName=', fileName #deb---------
		documentTranslator = DocumentTranslator(fileName)
		documentTranslator.Import(fileName)

	def __Export(self,filename=''):
		documentTranslator = DocumentTranslator(filename)
		documentTranslator.Export(filename)


class DocumentTranslator(object):
	isImport = None
	ids = []
	sceneGraph = None

	# Keep track of the layers on import
	layers = None

	cameraLibrary = None
##	  geometryLibrary = None
	controllersLibrary = None
	animationsLibrary = None
##	  materialLibrary = None
	texturesLibrary = None
	lampsLibrary = None
	colladaDocument = None
	scenesLibrary = None
	fps = 25

	def __init__(self, fileName):
		global waitingControllers, armatures
		# Keep track of the controller that are waiting to be applied
		waitingControllers = dict()
		# Keep track of the armatures created
		armatures = dict()

		self.isImport = False
		self.scenesLibrary = ScenesLibrary(self)
		self.sceneGraph = SceneGraph(self)
		self.lampsLibrary = LampsLibrary(self)
		self.colladaDocument = None
		self.texturesLibrary = TexturesLibrary(self)
		self.camerasLibrary = CamerasLibrary(self)
		self.materialsLibrary = MaterialsLibrary(self)
		self.meshLibrary = MeshLibrary(self)
		self.animationsLibrary = AnimationsLibrary(self)
		self.controllersLibrary = ControllersLibrary(self)

		self.filename = None
		self.filePath = ''


		self.currentBScene = Blender.Scene.GetCurrent()

		self.progressCount = 0.4
		self.progressField = (1.0 - self.progressCount)
		self.progressStep = 0.0
		self.progressPartCount = 0.0

		self.tMatOLD = Matrix()
		self._zUpMatrix = Matrix()
		self._yUpMatrix = Matrix(
		[0,0,1,0],
		[1,0,0,0],
		[0,1,0,0],
		[0,0,0,1])

		self.axisTransformMatrix = Matrix()
		self.inverseAxisTransformMatrix  = Matrix()
		self.orgAxiss = ["X","Y","Z"]

	def CreateID(self, name, typeName=None):
		if len(name) > 0 and not name[0].isalpha():
			name = "i"+name

		if not (name in self.ids):
			self.ids.append(name)
			return name
		else:
			tempName = name
			if not(typeName is None) and name.rfind(typeName) >= 0:
				# Check for existing number at the end?
				return self.IncrementString(tempName, True)
			else:
				# First check if no Blender Object exists with the name 'tempName + typeName'
				if (tempName + typeName) in self.allBlenderNames:
					return self.IncrementString(tempName + typeName, True)
				else:
					return self.CreateID(tempName+typeName, typeName)

	def IncrementString(self, name, checkName):
		tempName = name
		if name.rfind('.') >= 0:
			while tempName[-1:].isdigit():
				tempName =	tempName[:-1]
			digitStr = name[-(len(name)-len(tempName)):]
			digit = 1
			if len(digitStr) > 0 and len(digitStr) != len(name):
				digit = int(digitStr)+1
			newName = tempName+str(digit).zfill(3)
		else:
			newName = name+'.001'

		if not (newName in self.ids) and (not checkName or not (newName in self.allBlenderNames)):
			self.ids.append(newName)
			return newName
		else:
			return self.IncrementString(newName, checkName)


	def CreateNameForObject(self, name, replace, myType):
		if not replace:
			return name

		if myType == 'object':
			try:
				ob = Blender.Object.Get(name)
				ob.name = self.CreateNameForObject(self.IncrementString(ob.name, False), True, 'object')
			except ValueError:
				pass
		elif myType == 'mesh':
			try:
				mesh = Blender.Mesh.Get(name)
				if not mesh is None:
					mesh.name = self.CreateNameForObject(self.IncrementString(mesh.name, False), True, 'mesh')
			except ValueError:
				pass
		elif myType == 'armature':
			try:
				armature = Blender.Armature.Get(str(name))
				if not armature is None:
					armature.name = self.CreateNameForObject(self.IncrementString(armature.name, False), True, 'armature')
			except ValueError:
				pass
		elif myType == 'camera':
			try:
				camera = Blender.Camera.Get(str(name))
				if not camera is None:
					camera.name = self.CreateNameForObject(self.IncrementString(camera.name, False), True, 'camera')
			except NameError:
				pass
		elif myType == 'lamp':
			try:
				lamp = Blender.Lamp.Get(str(name))
				if not lamp is None:
					lamp.name = self.CreateNameForObject(self.IncrementString(lamp.name, False), True, 'lamp')
			except NameError:
				pass

		return name

	def Import(self, fileName):
		global debugMode, createNewScene
		#if debprn: print 'deb:DocumentTranslator_Import fileName', fileName #deb---------
		self.filename = fileName
		self.filePath = Blender.sys.dirname(self.filename) + Blender.sys.sep
		self.isImport = True
		Blender.Window.EditMode(0)
		Blender.Window.DrawProgressBar(0.0, 'Starting Import')

		# Keep track of the 20 layers
		self.layers = [None for x in range(20)]

		if createNewScene:
			self.currentBScene = Blender.Scene.New('Scene')
			self.currentBScene.makeCurrent()
		else:
			self.currentBScene = Blender.Scene.GetCurrent()

		# Create a new Collada document
		Blender.Window.DrawProgressBar(0.1, 'Get Collada Document')
		self.colladaDocument = collada.DaeDocument(debugMode)

		# Setup the libraries
		self.camerasLibrary.SetDaeLibrary(self.colladaDocument.camerasLibrary)
		self.lampsLibrary.SetDaeLibrary(self.colladaDocument.lightsLibrary)
		self.texturesLibrary.SetDaeLibrary(self.colladaDocument.imagesLibrary)
		self.materialsLibrary.SetDaeLibrary(self.colladaDocument.materialsLibrary)
		self.meshLibrary.SetDaeLibrary(self.colladaDocument.geometriesLibrary)
		self.animationsLibrary.SetDaeLibrary(self.colladaDocument.animationsLibrary)
		self.controllersLibrary.SetDaeLibrary(self.colladaDocument.controllersLibrary)

		# Parse the COLLADA file
		self.colladaDocument.LoadDocumentFromFile(fileName)

		self.axiss = ["X", "Y", "Z"]
		if self.colladaDocument.asset.upAxis == collada.DaeSyntax.Y_UP:
			self.tMatOLD[0][0] = 0
			self.tMatOLD[1][1] = 0
			self.tMatOLD[2][2] = 0
			self.tMatOLD[0][1] = 1
			self.tMatOLD[1][2] = 1
			self.tMatOLD[2][0] = 1
			self.axiss = ["Y", "Z", "X"]

		if self.colladaDocument.asset.upAxis == collada.DaeSyntax.Y_UP:
			self.axisTransformMatrix = Matrix(self._yUpMatrix)
			self.axiss = ["Y", "Z", "X"]

		self.inverseAxisTransformMatrix = Matrix(self.axisTransformMatrix).invert()

		self.progressStep = self.progressField/(self.colladaDocument.GetItemCount()+1)

		# Get the animation info
		#TODO: for what is this good? (migius)
		if 0:	animations = AnimationInfo.CreateAnimations(self.animationsLibrary, self.fps, self.axiss)

		# Read the COLLADA structure and build the scene in Blender.
		Blender.Window.DrawProgressBar(0.4, 'Translate Collada 2 Blender')
		self.sceneGraph.LoadFromCollada(self.colladaDocument.visualScenesLibrary.items, self.colladaDocument.scene)

		self.Progress()

	def CalcVector(self, vector):
		if self.colladaDocument.asset.upAxis == collada.DaeSyntax.Y_UP:
			return Vector(vector[2], vector[0], vector[1])
		else:
			return vector


	# Calculate the correct transform matrix dependent of the current UP axis.
	# When Up axis is Yup:
	# orgTrans * vecYup = newVecYup
	# newTrans * vecZup = newVecZup
	# vecZup = tMat * vecYUp
	# vecYup = invtMat * vecZup
	# newVecYup = orgTrans * vecYup = orgTrans * (invtMat * vecZup)
	# newVecZup = tMat * newVecYup = tMat * (orgTrans * (invtMat * vecZup))
	# newTrans = tMat * orgTrans * invtMat
	def CalcMatrix(self, matrix):
		if self.colladaDocument.asset.upAxis == collada.DaeSyntax.Y_UP:
			return self.axisTransformMatrix * matrix * self.inverseAxisTransformMatrix
		else:
			return matrix

	def CalcMatrixReverse(self, matrix):
		if self.colladaDocument.asset.upAxis == collada.DaeSyntax.Y_UP:
			return self.inverseAxisTransformMatrix * matrix * self.axisTransformMatrix
		else:
			return matrix

	def Export(self, fileName):
		global __version__, filename, usePhysics
		filename = fileName
		self.ids = []
		self.isImport = False
		Blender.Window.EditMode(0)
		Blender.Window.DrawProgressBar(0.0, 'Starting Export')

		# Create a new Collada document
		self.colladaDocument = collada.DaeDocument(debugMode)
		daeAsset = collada.DaeAsset()
		daeAsset.upAxis = 'Z_UP'
		daeAsset.created = datetime.datetime.today()
		daeAsset.modified = datetime.datetime.today()
		daeAsset.unit = collada.DaeUnit()
		daeAsset.unit.name = 'centimeter'
		daeAsset.unit.meter = '0.01'
		daeContributor = collada.DaeContributor()
		daeContributor.author = 'Illusoft Collada 1.4.0 plugin for Blender - http://colladablender.illusoft.com'
		daeContributor.authoringTool = 'Blender v:%s - Illusoft Collada Exporter v:%s' % (Blender.Get('version'), __version__)
		daeContributor.sourceData = GetValidFilename(Blender.Get('filename'))
		daeAsset.contributors.append(daeContributor)

		self.colladaDocument.asset = daeAsset

		bObjects = Blender.Object.Get()
		self.allBlenderNames = [x.getData(True) for x in bObjects] + [x.name for x in bObjects]

		daeScene = collada.DaeScene()

		#---------- Copied from export OBJ -----------------------------
		# Get Container Mesh
		# Remember: is this doesn't work, due to general problems, not especially
		# COLLADA related, OBJ export fails also too! Keep this in sync, but remember
		# that OBJ then still may use "NMesh", while this already uses "Mesh".
		temp_mesh_name = '~tmp-mesh'
		
		# Get the container mesh. - used for applying modifiers and non mesh objects.
		self.containerMesh = temp_mesh = None
		for temp_mesh in Blender.Mesh.Get():
			if temp_mesh.name.startswith(temp_mesh_name):
				if not temp_mesh.users:
					self.containerMesh = temp_mesh
		if not self.containerMesh:
			self.containerMesh = Blender.Mesh.New(temp_mesh_name)
		#------------ [end] Copied from export OBJ ------------------------
		
		# Loop through all scenes
		sceneCount = len(Blender.Scene.Get())
		self.progressStep = self.progressField / sceneCount
		Blender.Window.DrawProgressBar(0.1, \
							'Exporting ' + str(sceneCount) + ' scene(s)')
		for bScene in Blender.Scene.Get():
			if not exportCurrentScene or self.currentBScene == bScene:
				self.fps = bScene.getRenderingContext().framesPerSec()
				daeInstanceVisualScene = collada.DaeVisualSceneInstance()
				if usePhysics:
					daeInstancePhysicsScene = collada.DaePhysicsSceneInstance()

				daeVisualScene = self.colladaDocument.visualScenesLibrary.FindObject(bScene.name)
				if daeVisualScene is None:
					sceneGraph = SceneGraph(self)
					scenesList = sceneGraph.SaveToDae(bScene)
					daeVisualScene = scenesList[0]
					if usePhysics:
						daePhysicsScene = scenesList[1]

				daeInstanceVisualScene.object = daeVisualScene

				#self.colladaDocument.visualScenesLibrary.AddItem(daeIntanceVisualScene)
				if self.currentBScene == bScene:
					daeScene.iVisualScenes.append(daeInstanceVisualScene)
					if usePhysics:
						if not (daePhysicsScene is None):
							daeInstancePhysicsScene.object = daePhysicsScene
							daeScene.iPhysicsScenes.append(daeInstancePhysicsScene)

				#self.colladaDocument.visualScenesLibrary.AddItem(sceneGraph.ObjectToDae(bScene))
				#daeScene = collada.DaeScene()
				#daeScene.AddInstance()
				
			self.ProgressExport()


		self.colladaDocument.scene = daeScene
		
		Blender.Window.DrawProgressBar(0.98, 'Saving file to disk...')

		self.colladaDocument.SaveDocumentToFile(fileName)
		
		Blender.Window.DrawProgressBar(1.0, 'Export finished.')

	def Progress(self):
		self.progressPartCount = 0.0
		self.progressCount += self.progressStep
		Blender.Window.DrawProgressBar(self.progressCount, 'Creating Blender Nodes...')
		
	def ProgressPart(self, val, text):
		self.progressPartCount += val
		Blender.Window.DrawProgressBar(self.progressCount+self.progressPartCount*self.progressStep,text)
		
	def ProgressExport(self):
		self.progressPartCount = 0.0
		self.progressCount += self.progressStep
		Blender.Window.DrawProgressBar(self.progressCount, 'Exporting Blender Scenes...')

	def ProgressPartExport(self, val, text):
		self.progressPartCount += val
		Blender.Window.DrawProgressBar(self.progressCount+self.progressPartCount, text)

class SceneGraph(object):

	def __init__(self, document):
		self.document = document
		self.name = ''

		self.childNodes = dict()
		self.rootNodes = []

		self.objectNames = dict()
		# Get the current blender scene
		#self.currentBScene = Blender.Scene.getCurrent()

	def LoadFromCollada(self, visualScenes, colladaScene):
		global debugMode, newScene, clearScene, onlyMainScene
		currentBlenderSceneNames = [x.name for x in Blender.Scene.Get()]


		if visualScenes is None or len(visualScenes) == 0:
			return False

		# When in debugMode, delete everything from the scene before proceding
		if debugMode or clearScene:
			print "Delete everything from the scene.."
			bNodes = Blender.Object.Get()

			count = 0
			newName = "TEMPSCENE"
			newTempName = newName

			colladaSceneNames = [x.name for x in visualScenes]
			while newTempName in currentBlenderSceneNames+colladaSceneNames:
				count = count + 1
				newTempName = newName + str(count).zfill(3)

			tempScene = Blender.Scene.New(newTempName)
			tempScene.makeCurrent()

			if onlyMainScene: # delete only current scene
				Blender.Scene.Unlink(self.document.currentBScene)
			else: # delete all scenes except for the one just created
				for bs in Blender.Scene.Get():
					if not bs.name == newTempName:
						Blender.Scene.Unlink(bs)
			self.document.currentBScene = tempScene

		#if colladaScene == None:
		#	return False

		##//visualScenes = colladaScene.GetVisualScenes()
		##if visualScenes is None:
		##	return
		setDefaultScene = True

		for visualScene in visualScenes:
			if not onlyMainScene or (onlyMainScene and visualScene == colladaScene.GetVisualScene()):
				# Create a new Scene
				newName = visualScene.name
				count = 0
				nameCol = []
				if not clearScene:
					nameCol = nameCol+currentBlenderSceneNames
				while newName in nameCol:
					count = count + 1
					newName = visualScene.name + str(count).zfill(3)
				newScene = Blender.Scene.New(newName)
				self.document.currentBScene = newScene
				if onlyMainScene or (not onlyMainScene and clearScene and not colladaScene is None):
					newScene.makeCurrent()
					setDefaultScene = False

				Blender.Window.DrawProgressBar(0.5,'Build the items on the scene')
				# loop trough all nodes
				for daeNode in visualScene.nodes:
					sceneNode = SceneNode(self.document, None)
					ob = sceneNode.ObjectFromDae(daeNode)
					if ob != None:
						self.objectNames[daeNode.id] = ob.name
					##sceneNode.ObjectFromDae(daeNode)
			if visualScene == colladaScene.GetVisualScene():
				# Now get the physics Scene
				physicsScenes = colladaScene.GetPhysicsScenes()
				if not (physicsScenes is None) and len(physicsScenes) > 0:
					# For now, only pick the fist available physics Scene
					physicsScene = physicsScenes[0]
					for iPhysicsModel in physicsScene.iPhysicsModels:
						physicsNode = PhysicsNode(self.document)
						physicsNode.LoadFromDae(iPhysicsModel, self.objectNames)

		if setDefaultScene:
			Blender.Scene.Get()[0].makeCurrent()
		if clearScene:
			Blender.Scene.Unlink(tempScene)
			tempScene = None

		# Update the current Scene.
		self.document.currentBScene.update(1)

	def SaveToDae(self, bScene):
		global exportSelection, usePhysics
		daeVisualScene = collada.DaeVisualScene()
		daeVisualScene.id = daeVisualScene.name = self.document.CreateID(bScene.name,'-Scene')
		daePhysicsScene = collada.DaePhysicsScene()
		daePhysicsScene.id = daePhysicsScene.name = self.document.CreateID(bScene.name+'-Physics', '-Scene')

		if exportSelection:
			self.rootNodes = Blender.Object.GetSelected()
			self.childNodes = []
		else: 
			# Now loop trough all nodes in this scene and create a list with 
			# root nodes and children.
			sceneObjects = bScene.objects 
			if len(sceneObjects) > 0:
				progressStep = self.document.progressStep / len(sceneObjects) 
				for node in sceneObjects:
					pNode = node.parent
					if pNode is None:
						self.rootNodes.append(node)
					else:
						try:
							self.childNodes[pNode.name].append(node)
						except:
							self.childNodes[pNode.name] = [node]
							
					self.document.ProgressPartExport(progressStep, "Saving node(s)...")

		# Create a new Physics Model.
		daePhysicsModel = collada.DaePhysicsModel()
		daePhysicsModel.id = daePhysicsModel.name = self.document.CreateID(bScene.name,'-PhysicsModel')

		# Create a new Physics Model Instance.
		daePhysicsModelInstance = collada.DaePhysicsModelInstance()

		# Set the Physics Model of this instance.
		daePhysicsModelInstance.object = daePhysicsModel
		# add the physics model to the library.
		if usePhysics:
			self.document.colladaDocument.physicsModelsLibrary.items.append(daePhysicsModel)
		if not daePhysicsModelInstance is None:
		    daePhysicsScene.iPhysicsModels.append(daePhysicsModelInstance)

		# Begin with the rootnodes
		for rootNode in self.rootNodes:
			sceneNode = SceneNode(self.document,self)
			nodeResult = sceneNode.SaveSceneToDae(rootNode,self.childNodes, \
												  daePhysicsModel,daePhysicsModelInstance,bScene)
			daeNode = nodeResult[0]
			daeVisualScene.nodes.append(daeNode)

		self.document.colladaDocument.visualScenesLibrary.AddItem(daeVisualScene)
		if usePhysics and len(daePhysicsScene.iPhysicsModels) > 0:
			self.document.colladaDocument.physicsScenesLibrary.AddItem(daePhysicsScene)
		else:
			daePhysicsScene = None
			daePhysicsModel = None

		return (daeVisualScene, daePhysicsScene)

class PhysicsNode(object):
	dynamic = 0
	child = 1
	actor = 2
	inertiaLockX = 3
	inertiaLockY = 4
	inertiaLockZ = 5
	doFH = 6
	rotFH = 7
	anisotropicFriction = 8
	ghost = 9
	rigidBody = 10
	bounds = 11
	collisionResponse = 12

	def __init__(self, document):
		self.document = document

	def LoadFromDae(self, daeInstancePhysicsModel, objectNames):
		global usePhysics
		if not(usePhysics is None) and not usePhysics:
			return
		for iRigidBody in daeInstancePhysicsModel.iRigidBodies:
			# Get the real blender name instead of the collada name.
			realObjectName = objectNames[iRigidBody.targetString]
			# Get the Blender object with the specified name.
			bObject = Blender.Object.Get(realObjectName)
			# Check if physics is supported.
			if usePhysics is None:
				usePhysics = hasattr(bObject, "rbShapeBoundType")
				if not usePhysics:
					return

			# Get the rigid body.
			rigidBody = daeInstancePhysicsModel.object.FindRigidBody(iRigidBody.bodyString)
			##rigidBody = iRigidBody.body
			# Get the common technique of the rigid body.
			rigidBodyT = rigidBody.techniqueCommon
			# Get the physics material.
			physicsMaterial = rigidBodyT.GetPhysicsMaterial()
			# Get the shapes of the bounding volumes in this rigid body.
			shapes = rigidBodyT.shapes

			# The Rigid Body Flags
			rbFlags = 0 + rigidBodyT.dynamic + (1 << self.actor) + (1 << self.rigidBody) + (1 << self.bounds)
			rbShapeBoundType = 0
			# For now only get the first shape
			shape = shapes[0]
			# Check the type of the shape
			if isinstance(shape, collada.DaeGeometryShape):
				##print shape, shape.iGeometry.object.data
				if isinstance(shape.iGeometry.object.data, collada.DaeMesh):
					rbShapeBoundType = 4
				else:
					rbShapeBoundType = 5
			elif isinstance(shape, collada.DaeBoxShape):
				rbShapeBoundType = 0
			elif isinstance(shape, collada.DaeSphereShape):
				rbShapeBoundType = 1
			elif isinstance(shape, collada.DaeCylinderShape):
				rbShapeBoundType = 2
			elif isinstance(shape, collada.DaeTaperedCylinderShape):
				rbShapeBoundType = 3

			bObject.rbFlags = rbFlags
			if not (rigidBodyT.mass is None):
				bObject.rbMass = rigidBodyT.mass

			bObject.rbShapeBoundType = rbShapeBoundType

class Controller(object):
	# TODO: class Controller: armature Pose Mode need refactoring
	def __init__(self, document):
		if debprn: print 'deb:class Controller__init__ ---RUN---' #----------
		self.document = document
		self.bMesh = None
		self.daeController = None
		self.armatureName = None
		self.modifier = None
		self.bObject = None

	# Recursive method for setting the locations of the bones from their Bind matrices.
	def PositionBone(self, boneName, armature, bindMatrices):
		if debprn: print 'deb:class Controller_PositionBone() ---RUN---' #----------
		# Get the boneInfo for this bone.
		boneInfo = armature.boneInfos[boneName]
		# Get the jointName for this bone.
		jointName = boneInfo.GetJointName()

		##		headJointName = boneInfo.name
##		tailJointName =

##		print "deb: bone:" , boneName, "- headJoint:", jointName, "- tailJoint:", boneInfo.GetTailName(), "- isEnd:", boneInfo.IsEnd()

		# Get the BindMatrix for the head of this bone.
		headMatrix = bindMatrices[jointName]
		# Get the name of the tail joint.
		tailJointName = boneInfo.GetTailName() ##armature.boneInfos[boneName].tailJointName
		# If there is a tail joint, get the BindMatrix for that joint.

		tailMatrix = bindMatrices[tailJointName]
##		PrintTransforms(headMatrix, boneName)
##		if not boneInfo.IsEnd():
##			tailMatrix = bindMatrices[tailJointName]
		# Get the location of the armature Object.
		armatureLocation = armature.GetLocation()
		# Create a vector at [0,0,0,1]
		nullVec = Vector().resize4D()

		# Calculate the position of the head
		headVec = self.document.CalcVector(headMatrix * nullVec).resize4D() ##- armatureLocation
		headVec = Matrix(armature.GetTransformation()).transpose().invert() * headVec
		# Set the default value for the tail.
		tailVec = headVec + Vector(0,0,1,1)
		# If this bone has a Tail joint, calculate the position of the tail.
		if not boneInfo.IsEnd():
			tailVec = self.document.CalcVector(tailMatrix * nullVec).resize4D() ##- armatureLocation
			tailVec = Matrix(armature.GetTransformation()).transpose().invert() * tailVec
		else:
			parentBone = boneInfo.parent.GetBone()
			tailVec =  2 * headVec - Vector(parentBone.head).resize4D()
##			pass#PrintTransforms(headMatrix, jointName)
			##tailVec = (headMatrix * headVec)-armatureLocation
			##print jointName, headVec, headMatrix
			##print headVec, boneInfo.GetBone().tail - boneInfo.GetBone().head
			##tailVec = headVec + (boneInfo.GetBone().tail - boneInfo.GetBone().head).resize4D()
##			parentBoneInfo = boneInfo.parent
##			print "parentBoneInfo:",parentBoneInfo.name
##			if not parentBoneInfo is None:
##				parentBone = parentBoneInfo.GetBone()
##				tailVec = 2 * parentBone.tail - parentBone.head
##			else:
##				tailVec = Vector(0,0,1,1)

##		if boneInfo.IsRoot():
##			headVec -= Vector(0,0,-0.1,1)

		# Set the head and tail location.
##		if not boneInfo.IsEnd():
			##PrintTransforms(self.document.CalcMatrix(tailMatrix) * Matrix(armature.GetTransformation()).invert(), tailJointName)
##			PrintTransforms(self.document.CalcMatrix(tailMatrix), tailJointName)
			##boneInfo.GetBone().matrix = self.document.CalcMatrix(tailMatrix).transpose() * Matrix(armature.GetTransformation()).invert()
			##if boneName == "root" or boneName == "pelvis" or boneName == "spine":
##			print boneName
##			print boneInfo.GetHead()
##			print boneInfo.GetTail()
##			print headVec
##			print tailVec
##			print
		boneInfo.SetHead(headVec)
		boneInfo.SetTail(tailVec)



		##PrintTransforms(TranslationMatrix(armatureLocation).transpose(),"armature")
		##PrintTransforms(self.document.CalcMatrix(headMatrix)*
##		boneInfo.GetBone().matrix = self.document.CalcMatrix(headMatrix).transpose() ##* TranslationMatrix(armatureLocation).invert()

		# Do the same for each child.
		for childBoneName in boneInfo.childs:
			self.PositionBone(childBoneName, armature, bindMatrices)
		if debprn: print 'deb:class Controller_PositionBone() ---END---' #----------


	def PoseBone(self, boneName, armature, bindMatrices, bPose, parentBindI = Matrix()):
		if debprn: print 'deb:class Controller_PoseBone() ---RUN---' #----------
		#if debprn: print 'deb:class Controller_PoseBone() bindMatrices=', bindMatrices #----------
		boneInfo = armature.boneInfos[boneName]
		#if debprn: print 'deb:class Controller_PoseBone() boneInfo     =', boneInfo #----------
		#if debprn: print 'deb:class Controller_PoseBone() dir(boneInfo)=', dir(boneInfo) #----------
		bBone = boneInfo.GetBone()
		#if debprn: print 'deb:class Controller_PoseBone()         bBone=', bBone #----------
		jointName = boneInfo.GetJointName()
		#jointName = u'joint1'
		#if debprn: print 'deb:class Controller_PoseBone()     jointName=', jointName #----------
		bindMatrixCollada = bindMatrices[jointName]
		bindMatrixBlender = self.document.CalcMatrix(bindMatrixCollada)
##		PrintTransforms(bindMatrixBlender, "bind "+boneName)
		bindMatrixBlenderI = Matrix(bindMatrixBlender).invert()

		# calculate the local transform for the current position
		F = boneInfo.localTransformMatrix
##		PrintTransforms(F, "local transform")
		# calculate the transform in bindmode relative to its parent bind pose.
		E = parentBindI * bindMatrixBlender

		deltaBlender = Matrix()
##		if jointName != boneName:
			# calculate the difference between the two transforms
		deltaBlender = Matrix(E).invert() * F
##		PrintTransforms(deltaBlender, "delta")
		deltaBlenderT = Matrix(deltaBlender).transpose()

		# Set the transform
		bPose.bones[boneName].localMatrix = deltaBlenderT

		# Do the same for each child.
		for childBoneName in boneInfo.childs:
			self.PoseBone(childBoneName, armature, bindMatrices, bPose, bindMatrixBlenderI)
		if debprn: print 'deb:class Controller_PoseBone() ---end---' #----------


	def AnimateBone(self, boneName, armature, bPose, action):
		if debprn: print 'deb:class Controller_AnimateBone() ---RUN---' #----------
		boneInfo = armature.boneInfos[boneName]
		animationInfo = AnimationInfo.GetAnimationInfo(boneName)
		if not animationInfo is None:
			def pose_rot(anim_data):
				bone_rotation_matrix= Euler(anim_data[0], anim_data[1], anim_data[2]).toMatrix()
				bone_rotation_matrix.resize4x4()
				return tuple(bone_rotation_matrix.toQuat()) # qw,qx,qy,qz

			poseBone = bPose.bones[boneName]
			types = animationInfo.GetTypes(boneInfo.daeNode)
			for time in animationInfo.times.keys():
				poseBone.insertKey(armature.GetBlenderObject(), int(round(time)), types)
				target = animationInfo.times[time]

			actionIpos = action.getAllChannelIpos()
			ipo = actionIpos[boneName]
			for time in animationInfo.times:
				target = animationInfo.times[time]
				for value in target:
					type = animationInfo.GetType(boneInfo.daeNode, value)
					if type[0] == collada.DaeSyntax.TRANSLATE:
						pass
					elif type[0] == collada.DaeSyntax.ROTATE:
						if type[1][1] == "ANGLE":
							pass
##							axis = self.document.axiss[self.document.orgAxiss.index(type[1][0][-1])]
##							curve_wquat = ipo[Blender.Ipo.PO_QUATW]
##							curve_xquat = ipo[Blender.Ipo.PO_QUATX]
##							curve_yquat = ipo[Blender.Ipo.PO_QUATY]
##							pose_rotations = pose_rot([50+time,0,0])
##							print pose_rotations
##							curve_wquat.append((time, pose_rotations[0]))
##							curve_xquat.append((time, pose_rotations[1]))
##							curve_yquat.append((time, pose_rotations[2]))


			##for time in animationinfo.times.keys():


		for childBoneName in boneInfo.childs:
			self.AnimateBone(childBoneName,armature, bPose, action)
		if debprn: print 'deb:class Controller_AnimateBone() ---end---' #----------


	def BindMesh(self):
		if debprn: print 'deb:class Controller_BindMesh() ---RUN---' #----------
		global waitingControllers, armatures
		armature = Armature.GetArmature(self.armatureName)
		realArmatureName = armature.realName

		armatureObject = armature.GetBlenderObject()

		self.modifier[Blender.Modifier.Settings.OBJECT] = armatureObject

		bArmature = armature.GetBlenderArmature()
		daeSkin = self.daeController.skin

		# For each bone create a vertex group.
		for boneName in armatureObject.data.bones.keys():
			self.bMesh.addVertGroup(boneName)

		vertexWeights = daeSkin.vertexWeights
		daeJoints = daeSkin.joints
		if not vertexWeights.vcount is None and not vertexWeights.v is None:
			# Get the Joint Source
			jointList = daeSkin.FindSource(vertexWeights.FindInput('JOINT')).source.data
			#if debprn: print 'deb: jointList=', jointList #---------------
			#if debprn: print 'deb: jointList[0]=', jointList[0], type(jointList[0] #---------------
			# Get the weights
			weightList = 		daeSkin.FindSource(vertexWeights.FindInput('WEIGHT')).source.data
			# Get the BindMatrix values
			bindFloats = daeSkin.FindSource(daeJoints.FindInput("INV_BIND_MATRIX")).source.data
			# Create the BindMatrices
			bindMatrices = dict()
			for jointNameIndex in range(len(jointList)):
				jointName = jointList[jointNameIndex]
				bindMatrix = ToMatrix4(bindFloats[jointNameIndex*16:(jointNameIndex+1)*16]).invert()
				bindMatrices[jointName] = bindMatrix

			bPose = armatureObject.getPose()
			# Loop trough each Root bone. Those bones will position their childs.
			for rootBoneName in armature.rootBoneInfos:
				self.PoseBone(rootBoneName, armature, bindMatrices, bPose)

			# Set the bind positions of the bones (in Edit mode)
			armature.MakeEditable(True)
			# Loop trough each Root bone. Those bones will position their childs.
			for rootBoneName in armature.rootBoneInfos:
				self.PositionBone(rootBoneName, armature, bindMatrices)

			armature.MakeEditable(False)


			vIndex = 0
			vertInfos = []
			maxInputOffset = vertexWeights.GetStride()
			for vCountIndex in range(len(vertexWeights.vcount)): # loop trough each element in vcount.
				vcount = vertexWeights.vcount[vCountIndex]
				vertInfo = [vCountIndex,[]]
				for jointIndex in range(vcount): # Get all the info for this vertice.
					vertInfo[1].append([0,0])
					vertJointInfo = vertInfo[1][jointIndex]
					for input in vertexWeights.inputs:
						inputVal = vertexWeights.v[vIndex+input.offset]
						if input.semantic == "JOINT":
							vertJointInfo[0] = jointList[inputVal]
						elif input.semantic == "WEIGHT":
							vertJointInfo[1] = weightList[inputVal]
					vIndex += maxInputOffset
				vertInfos.append(vertInfo)


			bonesList = bArmature.bones.keys()


			jointBoneInfoList = armature.GetJointList()

			for vertInfo in vertInfos:
				for vertJointInfo in vertInfo[1]:
					groupName = vertJointInfo[0]
##					if  groupName in jointBoneInfoList:
						##boneName = jointBoneInfoList[groupName].name
					boneName = groupName
					vertIndex = vertInfo[0]
					vertWeight = vertJointInfo[1]
					self.bMesh.assignVertsToGroup(boneName,[vertIndex],vertWeight,'add')

			action = Blender.Armature.NLA.NewAction("Action")
			action.setActive(armature.GetBlenderObject())



			# Apply the animations for the bones (if available)
			for rootBoneName in armature.rootBoneInfos:
				pass#self.AnimateBone(rootBoneName, armature, bPose, action)
		if debprn: print 'deb:class Controller_BindMesh() ---end---' #----------

	def LoadFromDae(self, daeController, daeControllerInstance, bObject):
		if debprn: print 'deb:class Controller_LoadFromDae() ---RUN---' #----------
		global waitingControllers
		# Check if this controller is a SKIN controller
		if not (daeController.skin is None):
			# Create a new GeometryInstance for getting the mesh for this controller.
			daeGeoInstance = collada.DaeGeometryInstance()
			daeGeoInstance.url = daeController.skin.source
			daeGeoInstance.bindMaterials = daeControllerInstance.bindMaterials
			# Get the Blender Mesh
			self.bMesh = self.document.meshLibrary.FindObject(daeGeoInstance, True)
			self.daeController = daeController
			# Link the mesh to the blenderObject.
			bObject.link(self.bMesh)

			# Set the bObject to the right place.
			bindShapeMatrix =  daeController.skin.bindShapeMatrix
			bindShapeMatrix = self.document.CalcMatrix(bindShapeMatrix)
			bObject.setMatrix(bindShapeMatrix)

			# Create an armature modifier for the Blender Object.
			self.modifier = bObject.modifiers.append(Blender.Modifier.Type.ARMATURE)

			# Set the object skinned to this armature
			self.bObject = bObject

			# Find the armature for this controller
			armature = None
			for jointName in daeControllerInstance.skeletons:
				armature = Armature.FindArmatureWithJoint(jointName[1:])
				if not armature is None:
					self.armatureName = armature.name
					break

			# Check if the armature is already parsed. If not, wait until then.
			if not armature is None:
				# make the armature the parent of the blender object
				armature.GetBlenderObject().makeParent([bObject], 0 , 1)
				self.BindMesh()
			else:
				# Add this controller to a to-do list ;) and wait until the armature is parsed.
				##waitingControllers[self.armatureName] = self
				waitingControllers[jointName[1:]] = self



			#return the mesh.
			return self.bMesh
		else: # It's a morph controller. do nothing.
			print "WARNING: Morph is not supported"
			return None
		if debprn: print 'deb:class Controller_LoadFromDae() ---end---' #----------

	def SaveToDae(self, bModifier, bMeshObject, meshName):
		bMesh = bMeshObject.getData()
		daeController = collada.DaeController()
		daeController.id = self.document.CreateID(bMesh.name,"-skin")
		# Create a skin
		daeController.skin = daeSkin = collada.DaeSkin()
		daeSkin.source = meshName

		# Set the bindshapematrix
		daeSkin.bindShapeMatrix = Matrix(bMeshObject.matrix).transpose()##bMeshObject.getMatrix('localspace').transpose()

		bArmatureObject = bModifier[Blender.Modifier.Settings.OBJECT]
		if (bArmatureObject is None):
			HandleError(ERROR_MESH_ARMATURE_PARENT, meshName)
		bArmature = bArmatureObject.data

		# Create the joints elements
		daeSkin.joints = collada.DaeJoints()
		# Create the vertexWeights element
		daeSkin.vertexWeights = collada.DaeVertexWeights()

		# Create the source for the joints
		jointSource = collada.DaeSource()
		jointSource.id = self.document.CreateID(daeController.id, "-joints")
		jointSource.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		jointSource.source = jointSourceArray = collada.DaeIDREFArray()
		jointSourceArray.id = self.document.CreateID(jointSource.id, "-array")
		jointSource.techniqueCommon.accessor = jointAccessor = collada.DaeAccessor()
		jointAccessor.AddParam("JOINT",collada.DaeSyntax.IDREF)
		jointAccessor.source = jointSourceArray.id
		daeSkin.sources.append(jointSource)
		# And the input for the joints
		jointInput = collada.DaeInput()
		jointInput.semantic = "JOINT"
		jointInput.source = jointSource.id
		jointInput.offset = 0

		jointInput2 = collada.DaeInput()
		jointInput2.semantic = "JOINT"
		jointInput2.source = jointSource.id


		# Create the source for the weights
		weightSource = collada.DaeSource()
		weightSource.id = self.document.CreateID(daeController.id, "-weights")
		weightSource.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		weightSource.source = weightSourceArray = collada.DaeFloatArray()
		weightSourceArray.id = self.document.CreateID(weightSource.id, "-array")
		weightSource.techniqueCommon.accessor = weightAccessor = collada.DaeAccessor()
		weightAccessor.AddParam("WEIGHT","float")
		weightAccessor.source = weightSourceArray.id
		daeSkin.sources.append(weightSource)
		# And the input for the weights
		weightInput = collada.DaeInput()
		weightInput.semantic = "WEIGHT"
		weightInput.source = weightSource.id
		weightInput.offset = 1

		daeSkin.joints.inputs.append(jointInput2)
		daeSkin.vertexWeights.inputs.append(jointInput)
		daeSkin.vertexWeights.inputs.append(weightInput)

		poseSource = collada.DaeSource()
		poseSource.id = self.document.CreateID(daeController.id,"-poses")
		poseSource.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		poseSource.source = poseSourceArray = collada.DaeFloatArray()
		poseSourceArray.id = self.document.CreateID(poseSource.id,"-array")
		poseSource.techniqueCommon.accessor = poseAccessor = collada.DaeAccessor()
		poseAccessor.AddParam("","float4x4")
		poseAccessor.stride = 16
		poseAccessor.source = poseSourceArray.id
		daeSkin.sources.append(poseSource)
		# Add the input for the poses
		poseInput = collada.DaeInput()
		poseInput.semantic = "INV_BIND_MATRIX"
		poseInput.source = poseSource.id

		# Add this input to the joints
		daeSkin.joints.inputs.append(poseInput)


		# Get all vertextGroups
		vGroups = dict()
		for vertexGroupName in bMesh.getVertGroupNames():
			vwsdict = vGroups[vertexGroupName] = dict()
			try:
				vws = bMesh.getVertsFromGroup(vertexGroupName,True)
				for vw in vws:
					vwsdict[vw[0]] = vw[1]
			except:
				print("Vertex group '%s' couldn't be handled.\n" \
					"Maybe the group is empty.\n" % vertexGroupName)

		# Loop through each vertex group.
		for vertexGroupName in bMesh.getVertGroupNames():
			# Check if this vertexgroup has the same name as a bone in the armature.
			if vertexGroupName in bArmature.bones.keys():
				jointAccessor.count += 1
				adjustedName = "" + vertexGroupName
				# Jan: If we rename the bones/joints/armatures, we must rename the
				#		(corresponding) vertex groups also.
				adjustedName = AdjustName(adjustedName)
				jointSourceArray.data.append(adjustedName)
				# Get the vertices in this vertexGroup
				verts = bMesh.getVertsFromGroup(vertexGroupName)

##				print
##				PrintTransforms(Matrix(bArmature.bones[vertexGroupName].matrix['ARMATURESPACE']).transpose().invert(), vertexGroupName)
				if 0:
					bindMatrix = Matrix(bArmature.bones[vertexGroupName].matrix['ARMATURESPACE']).transpose()
					bindMatrix = Matrix(bMeshObject.matrix).transpose() * bindMatrix
				elif dmitri:	#by dmitri: Use ARAMATURE matrix for a global position/orientation
					bindMatrix = Matrix(bArmature.bones[vertexGroupName].matrix["ARMATURESPACE"]).resize4x4().transpose()
					bindMatrix = Matrix(bArmatureObject.getMatrix('localspace')).transpose() * bindMatrix
				else:
					headPos = bArmature.bones[vertexGroupName].head["ARMATURESPACE"]
					bindMatrix = Matrix([1,0,0,headPos.x], [0,1,0,headPos.y], [0,0,1,headPos.z],[0,0,0,1])
					bindMatrix = Matrix(bArmatureObject.getMatrix('localspace')).transpose() * bindMatrix

				invBindMatrix = Matrix(bindMatrix).invert()
				poseSourceArray.data.extend(MatrixToList(invBindMatrix))
				poseAccessor.count += 1
				for vert in verts:
					weightAccessor.count += 1


		vertJointCount = dict()
		weightIndex = 0
		for vert in bMesh.verts:
			jointCount = 0
			vertTotalWeight = 0.0
			jointVertexWeight = dict()

			# Jan: Keep this bone/joints renaming always in snyc with the vertex group
			# renaming!

			#count up the number of joints to get an equal weight
			for vGroup in vGroups:
				if vert.index in vGroups[vGroup]:
					adjustedName = "" + vGroup
					adjustedName = AdjustName(adjustedName)
					found = False
					weight = 0.0
					try :
						jointSourceArray.data.index(adjustedName)
						vi = vGroups[vGroup]
						weight = vi[vert.index]
						found = True
					except:
						found = False
					if found :
						jointCount += 1
						vertJointCount[vert.index] = jointCount
						jointVertexWeight[adjustedName] = weight
						vertTotalWeight+= weight

			#now we know how many, so make an even weight:
			# !!! Update cast3d !!! - even weight distribution only used if vertex total weight == 0
			# otherwise used each joint weight normalised by total weight
			for vGroup in vGroups:
				if vert.index in vGroups[vGroup]:
					adjustedName = "" + vGroup
					adjustedName = AdjustName(adjustedName)
					found = False
					try :
						jointSourceArray.data.index(adjustedName)
						found = True
					except:
						found = False
					if found :
						daeSkin.vertexWeights.v.append(jointSourceArray.data.index(adjustedName))
						daeSkin.vertexWeights.v.append(weightIndex)
						if vertTotalWeight != 0.0:
							jw = jointVertexWeight[adjustedName]
							vw = jw / vertTotalWeight
							weightSourceArray.data.append(vw)
							#print "Joint ", adjustedName, " weight=", jw, "Total weight=",vertTotalWeight, " Final weight=", vw
						else:
							weightSourceArray.data.append( 1.0 / vertJointCount[vert.index])
						weightIndex += 1

			#update the counts for this vertex
			if jointCount > 0:
				daeSkin.vertexWeights.count += 1
				daeSkin.vertexWeights.vcount.append(jointCount)


		self.document.colladaDocument.controllersLibrary.AddItem(daeController)
		return daeController

class Animation(object):
	def __init__(self, document):
		self.document = document

	def LoadFromDae(self, daeAnimation, daeNode, bObject):
		if debprn: print 'deb:class Animation_LoadFromDae RUN-------:' #------
		# Loop trough all channels
		for channel in daeAnimation.channels:
			if debprn: print 'deb: channel.target=',channel.target #------
			if debprn: print 'deb:     daeNode.id=',daeNode.id #------
			ca = channel.target.split("/",1)
			#if it targets to this daeNode
			if ca[0] == daeNode.id:
				for s in daeAnimation.samplers:
					if debprn: print 'deb:           s.id=',s.id #------
					#if debprn: print 'deb: channel.source=',channel.source #------
#org				if s.id == channel.source[1:]:
					if s.id == channel.source:
						if debprn: print 'deb: BINGO ---> channel.source=s.id' #------
						sampler = s

				if s!=None: #TODO: Animation_bObject sometimes is None
					if bObject.ipo is None:
						ipo = Blender.Ipo.New("Object",daeAnimation.id)
						bObject.setIpo(ipo)
					else:
						ipo = bObject.ipo
					type = self.FindType(ca[1], daeNode)
					input = sampler.GetInput("INPUT")
					inputSource = daeAnimation.GetSource(input.source)
					# Check if the input has a TIME parameter and is the only one.
					if not (type is None) and inputSource.techniqueCommon.accessor.HasParam("TIME") and len(inputSource.techniqueCommon.accessor.params) == 1:
						output = sampler.GetInput("OUTPUT")
						outputSource = daeAnimation.GetSource(output.source)
						accessorCount = outputSource.techniqueCommon.accessor.count
						accessorStride = outputSource.techniqueCommon.accessor.stride
						interpolations = sampler.GetInput("INTERPOLATION")
						interpolationsSource = daeAnimation.GetSource(interpolations.source)
						times = [x * self.document.fps for x in inputSource.source.data]
						if type[0] == "translate" or type[0] == "scale" or (type[0] == "rotate" and type[1][1] == "ANGLE"):
							axiss = []
							if len(type[1]) == 1:
								axiss = ["X", "Y", "Z"]
							elif type[0] == "rotate":
								axiss = [type[1][0][-1]]
							elif len(type[1]) == 2 and type[1][1] in ["X", "Y", "Z"]:
								axiss = [type[1][1]]
							for axis in axiss:
								if type[0] == "translate":
									cname = "Loc"
								elif type[0] == "scale":
									cname = "Scale"
								elif type[0] == "rotate":
									cname = "Rot"
								cname += self.document.axiss[self.document.orgAxiss.index(axis)]
								curve = ipo.addCurve(cname)
								curve.interpolation = 1 #LINEAR
								for time in times:
									value = outputSource.source.data[times.index(time) * accessorStride + axiss.index(axis)]
									if type[0] == "rotate":
										value /= 10
#old								curve.addBezier((time,value))
									inter = interpolationsSource.source.data[times.index(time) * accessorStride + axiss.index(axis)]
									if inter=='BEZIER':
										point=Blender.BezTriple.New()
										point.pt=(time, value)
										point.handleTypes=[1,1]
									else: #if inter=='LINEAR': inter=1
										point=(time, value)
									curve.append(point)
								if 1: #TODO: need individual support interpolation type for each point
									#if debprn: print 'deb: dir(curve)=', dir(curve) #--------
									if inter=='LINEAR': inter = 1
									elif inter=='BEZIER': inter = 2
									else: inter = 1
									curve.interpolation = inter
								#if debprn: print 'deb: inter=', inter #--------
								#if debprn: print 'deb: curve.interpolation=', curve.interpolation #--------

	def FindType(self, target, daeNode):
		ta = target.split(".",1)
		for t in daeNode.transforms:
			if t[2] == ta[0]:
				return [t[0],ta]
		return None

	def GetEulerAnimations(self, ipo, targetDaeNode, joint=None, bPose=None, bParentMatrix=None, bArmatureObject=None):
		curves = ipo.getCurves()
		if not curves is None:
			quatXList = dict()
			quatYList = dict()
			quatZList = dict()
			quatWList = dict()

			#collect quats
			quatKey = dict()
			for cur in curves:
				curName = cur.getName()
				if curName.startswith("Quat"):
					quatKey[curName] = []
					curNameIndex = curName[-1]
					if curNameIndex == 'X':
						for point in cur.bezierPoints:
							quatXList[point.pt[0]] = point.pt[1]
					elif curNameIndex == 'Y':
						for point in cur.bezierPoints:
							quatYList[point.pt[0]] = point.pt[1]
					elif curNameIndex == 'Z':
						for point in cur.bezierPoints:
							quatZList[point.pt[0]] = point.pt[1]
					elif curNameIndex == 'W':
						for point in cur.bezierPoints:
							quatWList[point.pt[0]] = point.pt[1]

			quats = dict()
			eulers = dict()

			xKeyList = quatXList.keys()
			yKeyList = quatYList.keys()
			zKeyList = quatZList.keys()
			wKeyList = quatWList.keys()

			#Assumption: All the keys are the same!!
			for xKey in xKeyList:
				if not quats.has_key(xKey):
					quats[xKey] = Quaternion()

			#assign value
			for key in xKeyList:
				quats[key].x = quatXList[key]
			for key in yKeyList:
				quats[key].y = quatYList[key]
			for key in zKeyList:
				quats[key].z = quatZList[key]
			for key in wKeyList:
				quats[key].w = quatWList[key]

			for key in quats:
				euler = quats[key].toEuler()

				if joint is not None:
					if dmitri:
						bindMatrix = Matrix(joint.matrix["ARMATURESPACE"]).resize4x4().transpose()
					else:
						headPos = joint.head["ARMATURESPACE"]
						bindMatrix = Matrix([1,0,0,headPos.x], [0,1,0,headPos.y], [0,0,1,headPos.z],[0,0,0,1])
					armMatrix = Matrix(bindMatrix)
					if not joint.hasParent():
						armMatrix = Matrix(bArmatureObject.getMatrix('localspace')).transpose().invert()
						armMatrix *= bindMatrix

					if 1: #migius
						swap = euler.y
						euler.y = - euler.z
						euler.z = swap

					else:
						poseMatrix = Matrix(bParentMatrix).invert() * armMatrix
						poseMatrix.transpose()

						poseEuler = poseMatrix.toEuler()
						euler.x += poseEuler.x
						euler.y += poseEuler.y
						euler.z += poseEuler.z
					#if debprn: print 'deb: getEuler: ', joint.name , poseEuler, euler

				eulers[key] = euler

			# this nodes list of euler angles:
			return eulers
		return None

	def SaveToDae(self, ipo, targetDaeNode, joint=None, bPose=None, bParentMatrix=None, bArmatureObject=None):
		global sampleAnimation
		animations = None
		curves = ipo.getCurves()
		if not curves is None:
			animations = dict()

			for curve in curves:
				cName = curve.getName()
				interpolation = curve.getInterpolation()
				#interpolation = curve.interpolation
				if debprn: print 'deb: interpolation=', interpolation #--------
				if cName.startswith("Loc") or cName.startswith("Rot") or cName.startswith("Scale"):
					if cName.startswith("Loc"):
						n = collada.DaeSyntax.TRANSLATE
					elif cName.startswith("Scale"):
						n = collada.DaeSyntax.SCALE
					else:
						n = collada.DaeSyntax.ROTATE+cName[-1]
					ani = animations.setdefault(n,{})

					# Get all the framenumbers for the current curve.
					frames = [bp.pt[0] for bp in curve.bezierPoints]
					if sampleAnimation: ## if the users wants to sample the animation each frame..
						# .. generate a sequence of frames, starting at the first(smallest) framenumber of the current curve
						#  and ending at the last(largest)framenumber.
						frames = range(min(frames), max(frames)+1)
					##print cName, frames
					# Now get the values for each frame
					for frameNumber in frames:
						anit = ani.setdefault(float(frameNumber),{'X':None, 'Y':None, 'Z':None, 'interpolation':interpolation})
						# calculate the value at the current frame.
						timeVal = float(curve[float(frameNumber)])
						##print cName, frameNumber, timeVal
						anit[cName[-1]] = timeVal

						if not joint is None:
							if dmitri:
								bindMatrix = Matrix(joint.matrix["ARMATURESPACE"]).resize4x4().transpose()
							else:
								headPos = joint.head["ARMATURESPACE"]
								bindMatrix = Matrix([1,0,0,headPos.x], [0,1,0,headPos.y], [0,0,1,headPos.z],[0,0,0,1])
							armMatrix = bindMatrix
							if ( not joint.hasParent() ):
								armMatrix = Matrix(bArmatureObject.getMatrix('localspace')).transpose()
								armMatrix *= bindMatrix

							poseMatrix = Matrix(bParentMatrix).invert() * armMatrix

							if cName.startswith("Loc"):
								poseMatrix.transpose()
								jointPosition = poseMatrix.translationPart()

								if cName[-1] == 'X':
									anit['X'] += jointPosition.x
								if cName[-1] == 'Y':
									anit['Y'] += jointPosition.y
								if cName[-1] == 'Z':
									anit['Z'] += jointPosition.z
							if cName.startswith("Scale"):
								poseMatrix.transpose()
								jointPosition = poseMatrix.scalePart()
								if cName[-1] == 'X':
									anit['X'] *= jointPosition.x
								if cName[-1] == 'Y':
									anit['Y'] *= jointPosition.y
								if cName[-1] == 'Z':
									anit['Z'] *= jointPosition.z

						if cName.startswith("Rot"):
							anit[cName[-1]] = anit[cName[-1]]*10 # Multiply the angle times 10 (Blender uses angle/10)
				else:
					pass

		eulers = self.GetEulerAnimations(ipo, targetDaeNode, joint, bPose, bParentMatrix, bArmatureObject)
		eulerKeys = eulers.keys()


		for key in eulerKeys:
			euler = eulers[key]

			aniX = animations.setdefault(str(collada.DaeSyntax.ROTATE) + 'X',{})
			aniY = animations.setdefault(str(collada.DaeSyntax.ROTATE) + 'Y',{})
			aniZ = animations.setdefault(str(collada.DaeSyntax.ROTATE) + 'Z',{})

			anitx = aniX.setdefault(key,{'X':0, 'Y':0, 'Z':0, 'interpolation':interpolation})
			anitx['X'] = euler.x
			anitx['Y'] = euler.y
			anitx['Z'] = euler.z

			anity = aniY.setdefault(key,{'X':0, 'Y':0, 'Z':0, 'interpolation':interpolation})
			anity['X'] = euler.x
			anity['Y'] = euler.y
			anity['Z'] = euler.z

			anitz = aniZ.setdefault(key,{'X':0, 'Y':0, 'Z':0, 'interpolation':interpolation})
			anitz['X'] = euler.x
			anitz['Y'] = euler.y
			anitz['Z'] = euler.z

		# add animations to collada
		for name, animation in animations.iteritems():
			daeAnimation = collada.DaeAnimation()
			daeAnimation.id = daeAnimation.name = self.document.CreateID(targetDaeNode.id+'-'+name,'-Animation')

			daeSourceInput = collada.DaeSource()
			daeSourceInput.id = self.document.CreateID(daeAnimation.id,'-input')
			daeFloatArrayInput = collada.DaeFloatArray()
			daeFloatArrayInput.id = self.document.CreateID(daeSourceInput.id,'-array')
			daeSourceInput.source = daeFloatArrayInput
			daeSourceInput.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
			accessorInput = collada.DaeAccessor()
			daeSourceInput.techniqueCommon.accessor = accessorInput
			accessorInput.source = daeFloatArrayInput.id
			accessorInput.count = len(animation)
			accessorInput.AddParam('TIME','float')

			if name == collada.DaeSyntax.TRANSLATE or name == collada.DaeSyntax.SCALE:
				vals = self.CreateSourceOutput(daeAnimation, len(animation),  "xyz")
			elif name.startswith(collada.DaeSyntax.ROTATE):
				vals = self.CreateSourceOutput(daeAnimation, len(animation),  "angle")

			daeSourceOutput = vals[0]
			outputArray = vals[1]
			daeSourceInterpolation = vals[2]
			interpolationArray = vals[3]

			daeAnimation.sources.append(daeSourceInput)
			daeAnimation.sources.append(daeSourceOutput)
			daeAnimation.sources.append(daeSourceInterpolation)

			animationKeys = animation.keys()
			animationKeys.sort()

			prevX = prevY = prevZ = 0

			for key in animationKeys:
				value = animation[key]

				daeFloatArrayInput.data.append(key/self.document.fps)
				interpolation = value['interpolation']
				if interpolation == 'Constant':
					cInterpolation = 'STEP'
				elif interpolation == 'Linear':
					cInterpolation = 'LINEAR'
				else:
					cInterpolation = 'BEZIER'

				if name == collada.DaeSyntax.TRANSLATE or name == collada.DaeSyntax.SCALE:
					if value['X'] is None:
						value['X'] = prevX
					else:
						prevX = value['X']

					if value['Y'] is None:
						value['Y'] = prevY
					else:
						prevY = value['Y']

					if value['Z'] is None:
						value['Z'] = prevZ
					else:
						prevZ = value['Z']

					outputArray.data.append(value['X'])
					outputArray.data.append(value['Y'])
					outputArray.data.append(value['Z'])

					interpolationArray.data.append(cInterpolation)
					interpolationArray.data.append(cInterpolation)
					interpolationArray.data.append(cInterpolation)
				elif name.startswith(collada.DaeSyntax.ROTATE):
					outputArray.data.append(value[name[-1]])
					interpolationArray.data.append(cInterpolation)

			#if not name.startswith(collada.DaeSyntax.ROTATE) or sum(outputArray.data) != 0:
			#dimitr: rotation could be full circle, can not use  sum(outputArray.data) != 0:
			if not name.startswith(collada.DaeSyntax.ROTATE) or len(animation) > 0:

				daeSampler = collada.DaeSampler()
				daeSampler.id = self.document.CreateID(daeAnimation.id,"-sampler")
				daeAnimation.samplers.append(daeSampler)

				daeInputInput = collada.DaeInput()
				daeInputInput.semantic = 'INPUT'
				daeInputInput.source = daeSourceInput.id
				daeSampler.inputs.append(daeInputInput)

				daeInputOutput = collada.DaeInput()
				daeInputOutput.semantic = 'OUTPUT'
				daeInputOutput.source = daeSourceOutput.id
				daeSampler.inputs.append(daeInputOutput)

				daeInputInterpolation = collada.DaeInput()
				daeInputInterpolation.semantic = 'INTERPOLATION'
				daeInputInterpolation.source = daeSourceInterpolation.id
				daeSampler.inputs.append(daeInputInterpolation)

				daeChannel = collada.DaeChannel()
				daeChannel.source = daeSampler

				daeChannel.target = collada.StripString(targetDaeNode.id) +'/'+name
				if name.startswith(collada.DaeSyntax.ROTATE):
					daeChannel.target = daeChannel.target + ".ANGLE"
				daeAnimation.channels.append(daeChannel)

				self.document.colladaDocument.animationsLibrary.AddItem(daeAnimation)

	def CreateSourceOutput(self, daeAnimation, count, type):
		daeSourceOutput = collada.DaeSource()
		daeSourceOutput.id = self.document.CreateID(daeAnimation.id,'-output')
		if type == "xyz" or "angle":
			outputArray = collada.DaeFloatArray()
		else:
			pass

		##daeFloatArrayOutput = collada.DaeFloatArray()
		outputArray.id = self.document.CreateID(daeSourceOutput.id,'-array')
		daeSourceOutput.source = outputArray
		daeSourceOutput.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		accessorOutput = collada.DaeAccessor()
		daeSourceOutput.techniqueCommon.accessor = accessorOutput
		accessorOutput.source = outputArray.id
		accessorOutput.count = count


		daeSourceInterpolation = collada.DaeSource()
		daeSourceInterpolation.id = self.document.CreateID(daeAnimation.id,'-interpolation')
		interpolationArray = collada.DaeNameArray()
		interpolationArray.id = self.document.CreateID(daeSourceInterpolation.id,'-array')
		daeSourceInterpolation.source = interpolationArray
		daeSourceInterpolation.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		accessorInterpolation = collada.DaeAccessor()
		daeSourceInterpolation.techniqueCommon.accessor = accessorInterpolation
		accessorInterpolation.source = interpolationArray.id
		accessorInterpolation.count = count

		if type == "xyz":
			accessorOutput.AddParam('X','float')
			accessorOutput.AddParam('Y','float')
			accessorOutput.AddParam('Z','float')
			accessorInterpolation.AddParam('X','Name')
			accessorInterpolation.AddParam('Y','Name')
			accessorInterpolation.AddParam('Z','Name')

		elif type == "angle":
			accessorOutput.AddParam('ANGLE','float')
			accessorInterpolation.AddParam('ANGLE','Name')

		return [daeSourceOutput, outputArray, daeSourceInterpolation, interpolationArray]


class SceneNode(object):

	def __init__(self, document, sceneNode):
		self.id = ''
		self.type = ''
		self.document = document
		self.transformMatrix = None
		self.localTransformMatrix = None
		self.parentNode = sceneNode

		self.armature = None
		self.isJoint = False

		self.createRootBone = False
		self.createLastBone = False

	def ObjectFromDae(self,daeNode):
		if debprn: print 'deb:class SceneNode_ObjectFromDae ---RUN---' #------------
		global replaceNames, objectList, waitingControllers, armatures
		self.document.Progress()
		self.id = daeNode.id
		self.name = daeNode.name
		self.type = daeNode.type
		if debprn: print 'deb: daeNode.id  =', daeNode.id #------------
		if debprn: print 'deb: daeNode.name=', daeNode.name #------------
		if debprn: print 'deb: daeNode.type=', daeNode.type #------------

		editBone = None
		newObject = None
		dataObject = None
		armature = None
		parentBone = None
		daeInstance = None
		boneName = None
		noninverse = 0

		#Get the transformation
		# TODO: replace all the self.document.tMatOLD and calculate the transformmatrices the correct way using CalcMatrix()
		mat = Matrix().resize4x4()
		for i in range(len(daeNode.transforms)):
			transform = daeNode.transforms[len(daeNode.transforms)-(i+1)]
			type = transform[0]
			data = transform[1]

			if type == collada.DaeSyntax.TRANSLATE:
				mat = mat*TranslationMatrix(Vector(data)* self.document.tMatOLD)
			elif type == collada.DaeSyntax.ROTATE:
				mat = mat*RotationMatrix(data[3] % 360,4,'r',Vector([data[0],data[1],data[2]])* self.document.tMatOLD)
			elif type == collada.DaeSyntax.SCALE:
				skewVec = Vector(data[0],data[1], data[2])*self.document.tMatOLD
				mat = mat * Matrix([skewVec.x,0,0,0],[0,skewVec.y,0,0],[0,0,skewVec.z,0],[0,0,0,1])
			elif type == collada.DaeSyntax.SKEW:
				s = math.tan(data[0]*angleToRadian)
				rotVec = Vector(data[1],data[2],data[3])*self.document.tMatOLD
				transVec = Vector(data[4],data[5],data[6])*self.document.tMatOLD
				fac1 = s*transVec.x
				fac2 = s*transVec.y
				fac3 = s*transVec.z

				mat = mat * Matrix([1+fac1*rotVec.x,fac1*rotVec.y,fac1*rotVec.z,0],[fac2*rotVec.x,1+fac2*rotVec.y,fac2*rotVec.z,0],[fac3*rotVec.x,fac3*rotVec.y,1+fac3*rotVec.z,0],[0,0,0,1])
			elif type == collada.DaeSyntax.LOOKAT:
				# TODO: LOOKAT Transform: use the correct up-axis
				position = Vector([data[0],data[1], data[2]])
				target = Vector([data[3],data[4], data[5]])
				up = Vector([data[6],data[7], data[8]]).normalize()
				front = (position-target).normalize()
				side = -1*CrossVecs(front, up).normalize()
				m = Matrix().resize4x4()

				m[0][0] = side.x
				m[0][1] = side.y
				m[0][2] = side.z

				m[1][0] = up.x
				m[1][1] = up.y
				m[1][2] = up.z

				m[2][0] = front.x
				m[2][1] = front.y
				m[2][2] = front.z

				m[3][0] = position.x
				m[3][1] = position.y
				m[3][2] = position.z

				mat = mat*m

			elif type == collada.DaeSyntax.MATRIX:
				mat = mat * self.document.CalcMatrix(data)## * self.document.tMatOLD

		self.localTransformMatrix = Matrix(mat)
		#if debprn: print 'deb: localTransformMatrix:\n' , self.localTransformMatrix #---------
		self.transformMatrix = mat


		if daeNode.IsJoint():#---daeNode is Joint
			if isinstance(self.parentNode, SceneNode):
				self.transformMatrix = self.transformMatrix * self.parentNode.transformMatrix
				#if debprn: print 'deb: transformMatrix:\n' , self.transformMatrix #--------
			currentBoneExists = False
			self.isJoint = True
			# it's a Joint, so check if we have to create a new armature or a bone inside an existing armature.
			if daeNode.parentNode == None or not daeNode.parentNode.IsJoint():
				# Create a unique name for the armature object
#old			objectName = self.document.CreateNameForObject(self.id, replaceNames, 'object')
				objectName = self.document.CreateNameForObject(self.name, replaceNames, 'object')
				# Create a unique name for the armature data
				armatureName = self.document.CreateNameForObject(objectName, replaceNames, 'armature')
				# Create a new armature
#old			self.armature = Armature.CreateArmature(objectName, self.id, armatureName, daeNode)
				self.armature = Armature.CreateArmature(objectName, self.name, armatureName, daeNode)

				##print "create armature", armatureName
				# Get the new created Blender Object
				newObject = self.armature.GetBlenderObject()
				# Link the new object to the current scene.
				self.document.currentBScene.link(newObject)

				# Set the position of the armature.
				newObject.setMatrix(self.transformMatrix)
			else:
				self.armature = self.parentNode.armature
				# Make the armature editable, so we can create a new bone.
				self.armature.MakeEditable(True)
				# Get the name of the parent bone of this bone.
				parentBoneName = None
				if isinstance(self.parentNode.parentNode, SceneNode):
#old				parentBoneName = str(self.parentNode.parentNode.id)
					parentBoneName = str(self.parentNode.parentNode.name)

				# Create the name for the new bone
#old			boneName = str(self.parentNode.id)
				boneName = str(self.parentNode.name)

				if not self.armature.HasBone(boneName):
					# Add a new bone to the armature.
					boneInfo = self.armature.AddNewBone(boneName, parentBoneName, daeNode)

					# Get the location of the armature.
					armatureLoc = self.armature.GetLocation()

					# Set the correct head and tail positions of this bone.
					headLoc = Vector(0,0,0,1)
					if not boneInfo.parent is None: # The head of this bone starts at the end of it's parent.
##						headLoc = Matrix(self.parentNode.transformMatrix).transpose() * Vector(0,0,0,1) - armatureLoc
						headLoc = Matrix(self.parentNode.transformMatrix).transpose() * Vector(0,0,0,1)
						# Check if the head of this bone is at the same position as the tail of it's parent.


					# Get the location of this node.
					nodeLoc = Matrix(self.transformMatrix).transpose() * Vector(0,0,0,1)
##					tailLoc = (nodeLoc - armatureLoc).resize3D()
					if headLoc == nodeLoc:
						print "zero bone:", boneName
					##print headLoc, nodeLoc
##					if (headLoc - nodeLoc).length < 0.001:
						nodeLoc += Vector(0,0,0.001,1)
					tailLoc = Matrix(self.armature.GetTransformation()).transpose().invert() * nodeLoc.resize4D()


##					print boneName
##					print "headLoc PRE" ,headLoc
					# Undo the armature transformation
					if not boneInfo.parent is None:
						headLoc = Matrix(self.armature.GetTransformation()).transpose().invert() * headLoc.resize4D()
						if (Vector(headLoc).resize3D() - boneInfo.parent.GetTail()).length < 0.001:
							boneInfo.SetConnected()

##					print "headLoc POST", headLoc
##					PrintTransforms(Matrix(self.armature.GetTransformation()).transpose().invert(),"armature")
##					print "invert origin", Matrix(self.armature.GetTransformation()).transpose().invert() * Vector(0,0,0,1)
##					print "nodeLoc PRE:",nodeLoc
##					print "tailLoc POST:", tailLoc

##					print
					# Set the location of the tail to the difference between the NodeLoc and armatureLoc
					boneInfo.SetHead(headLoc)
					boneInfo.SetTail(tailLoc)
					# Store the localTransformMatrix of this joint
					boneInfo.localTransformMatrix = Matrix(self.parentNode.localTransformMatrix).transpose()
					boneInfo.worldTransformMatrix = Matrix(self.parentNode.transformMatrix).transpose()

				else:
					currentBoneExists = True

				self.armature.MakeEditable(False)

			# Check if this is the last Joint
			hasJointChilds = False
			for daeChild in daeNode.nodes:
				if daeChild.IsJoint():
					hasJointChilds = True
					break

#old		if not hasJointChilds:
			if not hasJointChilds and not self.parentNode==None:

				# This is the last joint of the armature.
				# Create one last bone for the last joint.
				# (otherwise pivoting the end point of the last created bone is not possible)
				self.armature.MakeEditable(True)

				# Get the name of the parent bone of this bone.
#old				parentBoneName = str(self.parentNode.id)
				parentBoneName = str(self.parentNode.name)
				if debprn: print 'deb: i have parentBoneName=', parentBoneName #-----------
				# Create the name for the new bone
#old			boneName = str(self.id)
				boneName = str(self.name)
				if debprn: print 'deb:  add me boneName=', boneName #----------
				# Add a new bone to the armature.
				boneInfo = self.armature.AddNewBone(boneName, parentBoneName, daeNode)

				# Get the location of the armature.
				armatureLoc = self.armature.GetLocation().resize3D()
				if debprn: print 'deb:  armatureLoc=', armatureLoc #----------

				# Set the correct head and tail positons of this bone.
				headLoc = Vector(0,0,0)
				tailLoc = Vector(0,0,1)
				if not parentBoneName is None:
					# The head of this bone starts at the end of it's parent.
					parentBone = boneInfo.parent.GetBone()

					if currentBoneExists:
						headLoc = self.transformMatrix.translationPart() - armatureLoc
					else:
						headLoc = parentBone.tail
						boneInfo.SetConnected()

					tailLoc = 2 * headLoc - parentBone.head
					boneInfo.localTransformMatrix = Matrix(self.localTransformMatrix).transpose()

				boneInfo.SetHead(headLoc)
				boneInfo.SetTail(tailLoc)

				self.armature.MakeEditable(False)

		else : #---daeNode is not Joint
			if isinstance(self.parentNode, SceneNode):
				self.transformMatrix *= self.parentNode.localTransformMatrix
				#if debprn: print 'deb: transformMatrix:\n' , self.transformMatrix #--------
			daeInstances = daeNode.GetInstances()
			if debprn: print 'deb: daeInstances:\n' , daeInstances #--------
			isController = False
			newObjects = []
			if len(daeInstances):
				for daeInstance in daeInstances:
					# Check which type the instance is
					if isinstance(daeInstance,collada.DaeAnimationInstance): # Animation
						newObject = Blender.Object.New('Empty',self.document.CreateNameForObject(self.id,replaceNames, 'empty'))
						newObjects.append(newObject)

					elif isinstance(daeInstance,collada.DaeCameraInstance): # Camera
						newObject = Blender.Object.New('Camera',self.document.CreateNameForObject(self.id,replaceNames, 'camera'))
						dataObject = self.document.camerasLibrary.FindObject(daeInstance,True)
						newObject.link(dataObject)
						newObjects.append(newObject)

					elif isinstance(daeInstance,collada.DaeControllerInstance): # Controller
						newObject = Blender.Object.New('Mesh',self.document.CreateNameForObject(self.id,replaceNames, 'object'))
						dataObject = self.document.controllersLibrary.FindObject(daeInstance, True, newObject)
						newObject.link(dataObject)
						isController = True
						newObjects.append(newObject)
						if debprn: print 'deb: M E S H isController -------O O O O O:' #--------

					elif isinstance(daeInstance,collada.DaeGeometryInstance): # Geometry
						newObject = Blender.Object.New('Mesh',self.document.CreateNameForObject(self.id,replaceNames, 'object'))
						dataObject = self.document.meshLibrary.FindObject(daeInstance,True)
						newObject.link(dataObject)
						newObjects.append(newObject)
						if debprn: print 'deb: M E S H daeInstance -------ooooooooo:' #--------

					elif isinstance(daeInstance,collada.DaeLightInstance): # Light
						newObject = Blender.Object.New('Lamp',self.document.CreateNameForObject(self.id,replaceNames, 'lamp'))
						dataObject = self.document.lampsLibrary.FindObject(daeInstance,True)
						newObject.link(dataObject)
						newObjects.append(newObject)

					elif isinstance(daeInstance,collada.DaeNodeInstance): # Node
						newObject = Blender.Object.New('Empty',self.document.CreateNameForObject(self.id,replaceNames, 'empty'))
						newObjects.append(newObject)

					elif isinstance(daeInstance,collada.DaeVisualSceneInstance): # Visual Scene
						newObject = Blender.Object.New('Empty',self.document.CreateNameForObject(self.id,replaceNames, 'empty'))
						newObjects.append(newObject)

					else:
						print "???   Loading Unknown Instance:", daeInstance
#old					return

				# If there were multiple instance, create an empty. We add the instances to it later.
				#if not isController or len (daeInstances) > 1:
					#if debprn: print 'deb: PROBLEM >>>>>>> make newObject as Empty cointainer: ' #--------
					#newObject = Blender.Object.New('Empty',self.id)

				if newObjects:
					if len(newObjects)==1:
						newObject = newObjects[0]
						if debprn: print 'deb: >>>>>>>bObject: ' , newObject #--------
						self.bObject = newObject
						self.document.currentBScene.link(newObject)
					else:
						newObject = Blender.Object.New('Empty',self.id)
						self.document.currentBScene.link(newObject)
						# If there were more instances in this node,
						# also add all those instances to the Blender Scene
						#  and make them childs of the Empty.
						if debprn: print 'deb: >>>>>>>newObjects: ' , newObjects #--------
						for newObjectChild in newObjects:
							self.document.currentBScene.link(newObjectChild)
						newObject.makeParent(newObjects, noninverse , 1)

			else:
				if debprn: print 'deb: no daeInstances ? ? ? ?:' #--------
				newObject = Blender.Object.New('Empty',self.id)
				#newObjects.append(newObject)

			#reload properties:
			if len(daeNode.extras) > 0:
				for extra in daeNode.extras:
					for tech in extra.techniques:
						for prop in tech.params:
							self.bObject.addProperty(prop.name, prop.value, prop.type)
							if prop.value is not None:
								if prop.type == 'STRING':
									realValue = prop.value.encode('utf-8')
									self.bObject.getProperty(prop.name).setData(realValue)
								elif prop.type == 'INT':
									realValue = int(prop.value.encode('utf-8'))
									self.bObject.getProperty(prop.name).setData(realValue)
								elif prop.type == 'FLOAT':
									realValue = float(prop.value.encode('utf-8'))
									self.bObject.getProperty(prop.name).setData(realValue)
								elif prop.type == 'BOOL':
									realValue = int(prop.value.encode('utf-8'))
									self.bObject.getProperty(prop.name).setData(realValue)
								elif prop.type == 'TIME':
									realValue = float(prop.value.encode('utf-8'))
									self.bObject.getProperty(prop.name).setData(realValue)


			# TODO: Vertex Colors: MAYBE CHANGE THIS LATER update the mesh..
			if newObject.getType() == 'Mesh':
				newObject.getData().update(1,1,1)
				# Set the vertex colors.
				try:
					for f in newObject.getData(mesh=1).faces:
						for c in f.col:
							c.r = 255
							c.g = 255
							c.b = 255
							c.a = 255
				except ValueError:
					pass

#old	if not self.isJoint and self.armature is None:
#old2	if self.armature is None:

		if 1:	#---Check if the daeNode has some Layer information.
			if len(daeNode.layer):
				layers = self.document.layers
				# Keep track of the blender layers to which this Node belongs.
				myLayers = []
				# Loop through all layers of this node.
				for layer in daeNode.layer:
					# Check if this layer is used before.
					if layer in layers:
						# If so, the layer to add is the index of that layer + 1.
						layerNo = layers.index(layer)+1
					else:
						# else, create a new layer.
						addLayer = True
						# When the layer is a digit, try to use the same digit in Blender.
						if layer.isdigit():
							# If So, check if this digit is between 1 and 20 AND if this layer is not used before
							digit = int(layer)
							if digit >= 1 and digit <= len(layers) and layers[digit-1] is None:
								# Add the new layer to the list.
								layers[digit-1] = layer
								layerNo = digit
								# Set this flag to false, so further checking is skipped.
								addLayer = False
						# If the layer was not a digit, Create a new one.
						if addLayer:
							layerNo = 1
							# Get the first free spot.
							if None in layers:
								index = layers.index(None)
								layers[index] = layer
								layerNo = index+1
					# When this layerNo is not in myLayers yet, add it to the list.
					if not (layerNo in myLayers):
						myLayers.append(layerNo)
			else:
				# Use the current selected layers.
				myLayers = self.document.currentBScene.layers

		childlist = []
		for daeChild in daeNode.nodes:
			try:
				childSceneNode = SceneNode(self.document,self)
				object = childSceneNode.ObjectFromDae(daeChild)
				if object: childlist.append(object)
			except NameError:
				if debprn: print "a child of node " + daeNode.id + " has no id ? ?"
		for iDaeChild in daeNode.iNodes:
			try:
				childSceneNode = SceneNode(self.document,self)
				object = childSceneNode.ObjectFromDae(iDaeChild.object)
				if object:
					newObject.makeParent([object], noninverse , 1)
					childlist.append(object)
			except NoneType:
				if debprn: print "a child instance of node " + daeNode.id + " has no id ? ?"

		if newObject:
			if not self.isJoint and not self.armature:
				newObject.setMatrix(Matrix().resize4x4())
				newObject.makeParent(childlist, noninverse,1)
				# Set the layers of the new Object.
				newObject.layers = myLayers
				newObject.setMatrix(self.localTransformMatrix)
			elif self.isJoint and self.armature:
				for jointName in waitingControllers:
					armature = Armature.FindArmatureWithJoint(jointName)
					if armature and armature.name == self.armature.name:
						controller = waitingControllers[jointName]
						controller.armatureName = armature.name
						armature.GetBlenderObject().makeParent([controller.bObject], noninverse , 1)
						controller.BindMesh()
						break
			# Check if this node has an animation.
			daeAnimations = self.document.animationsLibrary.GetDaeAnimations(self.id)
			if debprn: print 'deb: O ------- O animation check' #------------
			for daeAnimation in daeAnimations:
				a = Animation(self.document)
				a.LoadFromDae(daeAnimation, daeNode, newObject)
				if debprn: print 'deb: X -------- X an animation is loaded= \n', a #------------

		if debprn: print 'deb:class SceneNode_ObjectFromDae ---end---' #------------
		return newObject

	def SaveSceneToDae(self, bNode, childNodes, daeGlobalPhysicsModel, \
					   daeGlobalPhysicsModelInstance, bScene):
		global bakeMatrices, exportSelection, applyModifiers, debprn
		daeNode = collada.DaeNode()
		daeNode.id = daeNode.name = self.document.CreateID(bNode.name,'-Node')# +'-node'

		if len(bNode.getAllProperties()) > 0 :
			daeExtra = collada.DaeExtra()
			daeNode.extras.append(daeExtra)

			tech = collada.DaeTechnique()
			daeExtra.techniques.append(tech)

			for property in bNode.getAllProperties():
				tech.AddParam( property.getName(), property.getType(), property.getData())

		# Get the transformations
		mat = bNode.getMatrix('localspace')
		if bakeMatrices :
			mat = Matrix(mat).transpose()
			daeNode.transforms.append([collada.DaeSyntax.MATRIX, mat])
		else:
			loc = mat.translationPart()
			daeNode.transforms.append([collada.DaeSyntax.TRANSLATE, loc])

			euler = mat.toEuler()
			rotxVec = [1,0,0,euler.x]
			rotyVec = [0,1,0,euler.y]
			rotzVec = [0,0,1,euler.z]
			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotzVec])
			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotyVec])
			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotxVec])

			scale = mat.scalePart()
			daeNode.transforms.append([collada.DaeSyntax.SCALE, scale])

		# Get the instance
		type = bNode.getType()
		instance  = None
		meshID = None
		if type == 'Mesh':
			instance = collada.DaeGeometryInstance()
			daeGeometry = self.document.colladaDocument.geometriesLibrary.FindObject(bNode.getData(True))
			meshNode = MeshNode(self.document)
			if daeGeometry is None:							   
				derivedObjsMatrices = BPyObject.getDerivedObjects(bNode)
				for derivedObject, matrix in derivedObjsMatrices:
					virtualMesh = BPyMesh.getMeshFromObject(derivedObject, \
									self.document.containerMesh, applyModifiers, False, bScene)					
					if debprn:
						print("Virtual mesh: " + str(virtualMesh) )
						# + "; type: " + str(type(virtualMesh)))
					if not virtualMesh:
						# Fallback!
						# Should never happen!
						# (The "mesh=1" param on the getData() method ensures that
						# it gets a "Mesh" instead of a "NMesh".) 
						print("Error while trying to save derived object / apply modifiers. Try saving " \
							  + "more direct, all modifiers will be ignored.")
						daeGeometry = meshNode.SaveToDae(bNode.getData(mesh=1))
					else:
						# Apply original name from untransformed object
						# to transformed object (name is later copied 1:1 to id).
						virtualMesh.name = derivedObject.name
						# _Don't_ apply transformation matrix "matrix"!
						# The transformation will get instead written in the file itself
						# seperately!
						daeGeometry = meshNode.SaveToDae(virtualMesh)
					
			meshID = daeGeometry.id
			bindMaterials = meshNode.GetBindMaterials(bNode.getData(),\
													daeGeometry.uvTextures, daeGeometry.uvIndex)
			instance.object = daeGeometry
			instance.bindMaterials = bindMaterials


			instanceController = None
			# Check if this mesh is skinned to an amarture.
			isSkinned = False
			for bModifier in bNode.modifiers:
				if bModifier.type == Blender.Modifier.Type.ARMATURE:
					isSkinned = True
					# An Armature modifier exists, so create a Controller.
					controller = Controller(self.document)
					daeController = controller.SaveToDae(bModifier, bNode, meshID)

					#Get the root bone
					bArmatureObject = bModifier[Blender.Modifier.Settings.OBJECT]
					if (bArmatureObject is None):
						HandleError(ERROR_MESH_ARMATURE_PARENT, meshID)
					bArmature = bArmatureObject.getData()
					rootBones = []
					for boneName in bArmature.bones.keys():
						bone = bArmature.bones[boneName]
						if not bone.hasParent():
							rootBones.append(bone)

					instanceController = collada.DaeControllerInstance()
					instanceController.object = daeController
					daeNode.transforms = []
					loc = [0,0,0]
					daeNode.transforms.append([collada.DaeSyntax.TRANSLATE, loc])

					rotxVec = [1,0,0,0]
					rotyVec = [0,1,0,0]
					rotzVec = [0,0,1,0]
					daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotzVec])
					daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotyVec])
					daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotxVec])

					scale = [1,1,1]
					daeNode.transforms.append([collada.DaeSyntax.SCALE, scale])
					#use the rootBone's name for consistancy with vert groups
					instanceController.skeletons.append(rootBones[0].name)
					instanceController.bindMaterials = bindMaterials

			if not isSkinned:  # only add an instance geometry if it is not skinned
				daeNode.iGeometries.append(instance)
			else:
				daeNode.iControllers.append(instanceController)

		elif type == 'Camera':
			instance = collada.DaeCameraInstance()
			daeCamera = self.document.colladaDocument.camerasLibrary.FindObject(bNode.getData(True))
			if daeCamera is None:
				cameraNode = CameraNode(self.document)
				daeCamera = cameraNode.SaveToDae(bNode.getData())
			instance.object = daeCamera
			daeNode.iCameras.append(instance)
		elif type == 'Lamp':
			instance = collada.DaeLightInstance()
			daeLight = self.document.colladaDocument.lightsLibrary.FindObject(bNode.getData(True))
			if daeLight is None:
				lampNode = LampNode(self.document)
				daeLight = lampNode.SaveToDae(bNode.getData())
			instance.object = daeLight
			daeNode.iLights.append(instance)
		elif type == 'Armature':
			daeNode.type = collada.DaeSyntax.TYPE_JOINT
			armatureNode = ArmatureNode(self.document)
			daeArmature = armatureNode.SaveToDae(bNode.getData(),bNode, bNode.getPose())
			daeNode = daeArmature[0]

		# Check if the object has an IPO curve. if so, export animation.
		if not (bNode.ipo is None):
			ipo = bNode.ipo
			animation = Animation(self.document)
			animation.SaveToDae(ipo, daeNode)

		# Export layer information.
		daeNode.layer = ['L'+str(layer) for layer in bNode.layers]

		if not exportSelection:
			myChildNodes = []
			if childNodes.has_key(bNode.name):
				myChildNodes = childNodes[bNode.name]

			for bNode in myChildNodes:
				sceneNode = SceneNode(self.document, self)
				daeNode.nodes.append(sceneNode.SaveSceneToDae(bNode, childNodes, daeGlobalPhysicsModel, \
															daeGlobalPhysicsModelInstance, bScene)[0])

		daePhysicsInstance = self.SavePhysicsToDae(bNode, meshID, daeNode,daeGlobalPhysicsModel,daeGlobalPhysicsModelInstance)
		return (daeNode, daePhysicsInstance)

	def SavePhysicsToDae(self, bNode, meshID, daeNode,daeGlobalPhysicsModel,daeGlobalPhysicsModelInstance):
		global usePhysics
		if meshID is None or (not(usePhysics is None) and not usePhysics):
			return None

		# Check if physics is supported.
		if usePhysics is None:
			usePhysics = hasattr(bNode, "rbShapeBoundType")
			if not usePhysics:
				return None

		rbFlags = [0]*32
		rbF = bNode.rbFlags
		lastIndex = 0
		# Get the bit flags.
		while rbF > 0:
			val = rbF >> 1
			if val << 1 == rbF:
				##rbFlags.append(0)
				rbFlags[lastIndex] = 0
			else:
				##rbFlags.append(1)
				rbFlags[lastIndex] = 1
			lastIndex += 1
			rbF = val


		daeRigidBody = collada.DaeRigidBody()
		#print "daeNode.id",daeNode.id
		daeRigidBody.id = daeRigidBody.name = daeRigidBody.sid = self.document.CreateID(daeNode.id,'-RigidBody')
		#print "daeRigidBody.sid",daeRigidBody.sid
		daeRigidBodyTechniqueCommon = collada.DaeRigidBody.DaeTechniqueCommon()
		daeRigidBodyTechniqueCommon.dynamic = bool(rbFlags[0])
		if daeRigidBodyTechniqueCommon.dynamic:
				daeRigidBodyTechniqueCommon.mass = bNode.rbMass
		else:
				daeRigidBodyTechniqueCommon.mass = 0 #not dynamic means mass == 0!

		# Check the shape of the rigid body.
		if bNode.rbShapeBoundType == 0 and rbFlags[PhysicsNode.bounds]: # Box
			shape = collada.DaeBoxShape()
			shape.halfExtents = list(bNode.rbHalfExtents)
		elif bNode.rbShapeBoundType == 1  and rbFlags[PhysicsNode.bounds]: # Sphere
			shape = collada.DaeSphereShape()
			shape.radius = bNode.rbRadius
		elif bNode.rbShapeBoundType == 2  and rbFlags[PhysicsNode.bounds]: # Cylinder
			shape = collada.DaeCylinderShape()
			shape.radius = [[bNode.rbRadius],[bNode.rbRadius]]
			shape.height = bNode.rbHalfExtents[2]
		elif bNode.rbShapeBoundType == 3  and rbFlags[PhysicsNode.bounds]: # Cone
			shape = collada.DaeTaperedCylinderShape()
			shape.radius1 = [[bNode.rbRadius],[bNode.rbRadius]]
			shape.radius2 = [0 , 0]
			shape.height = bNode.rbHalfExtents[2]
		else: # Convex hull or # Static Triangle Mesh for static, and # Sphere if dynamic
			if daeRigidBodyTechniqueCommon.dynamic and not rbFlags[PhysicsNode.bounds]:
				shape = collada.DaeSphereShape()
				shape.radius = bNode.rbRadius
			else:
				shape = collada.DaeGeometryShape()
				iGeometry = collada.DaeGeometryInstance()
				if bNode.rbShapeBoundType == 5:
					object = self.document.colladaDocument.geometriesLibrary.FindObject(daeNode.id+'-Convex')
				else:
					object = self.document.colladaDocument.geometriesLibrary.FindObject(meshID)
					if not object.HasOnlyTriangles(): # The geometry contains no triangles
						pass

				if object is None:
					object = collada.DaeGeometry()
					object.id = object.name = self.document.CreateID(daeNode.id,'-ConvexGeom')
					convexMesh = collada.DaeConvexMesh()
					convexMesh.convexHullOf = meshID
					object.data = convexMesh
					self.document.colladaDocument.geometriesLibrary.AddItem(object)
				iGeometry.object = object
				shape.iGeometry = iGeometry

		# Create a physics material.
		daePhysicsMaterial = self.document.colladaDocument.physicsMaterialsLibrary.FindObject(daeNode.id+'-PxMaterial')
		if daePhysicsMaterial is None:
			daePhysicsMaterial = collada.DaePhysicsMaterial()

			physicsMaterials = bNode.getData().getMaterials()
			if (not (physicsMaterials is None)) and len(physicsMaterials) > 0:
				physicsMaterial = physicsMaterials[0]
				if not (physicsMaterial is None):
					daePhysicsMaterial.techniqueCommon.restitution = physicsMaterial.rbRestitution

					# Usually dynamic friction is less than static friction. However, as of Blender 2.42,
					# only a single friction coefficient can be specified. This is used for both
					# static and dynamic friction coefficients.

					daePhysicsMaterial.techniqueCommon.staticFriction = physicsMaterial.rbFriction
					daePhysicsMaterial.techniqueCommon.dynamicFriction = physicsMaterial.rbFriction

			daePhysicsMaterial.id = daePhysicsMaterial.name = self.document.CreateID(daeNode.id, '-PhysicsMaterial')
			self.document.colladaDocument.physicsMaterialsLibrary.AddItem(daePhysicsMaterial)

		# Create a physics material instance.
		daePhysicsMaterialInstance = collada.DaePhysicsMaterialInstance()
		# Set the physics material of this instance
		daePhysicsMaterialInstance.object = daePhysicsMaterial


		# add the shape to the commom technique.
		daeRigidBodyTechniqueCommon.shapes.append(shape)
		# Set the material of the common technique.
		daeRigidBodyTechniqueCommon.iPhysicsMaterial = daePhysicsMaterialInstance
		# Set the common technique of this rigid body.
		daeRigidBody.techniqueCommon = daeRigidBodyTechniqueCommon

		# Add the rigid body to the physics model.
		daeGlobalPhysicsModel.rigidBodies.append(daeRigidBody)
		# Create a new RigidBody instance
		daeRigidBodyInstance = collada.DaeRigidBodyInstance()
		# Set the rigid body of this instance
		daeRigidBodyInstance.body = daeRigidBody
		# Set the node of this instance
		daeRigidBodyInstance.target = daeNode

		# add the rigidbody instance to this physics model instance.
		daeGlobalPhysicsModelInstance.iRigidBodies.append(daeRigidBodyInstance)

		#handle constraints
		for rbconstraint in bNode.constraints:
			if rbconstraint.type == Blender.Constraint.Type.RIGIDBODYJOINT:
				daeRigidConstraint = collada.DaeRigidConstraint()
				daeRigidConstraint.id = daeRigidConstraint.name = daeRigidConstraint.sid = rbconstraint.name

				daeRigidConstraint.attachment = collada.DaeRigidConstraint.DaeAttachment()
				daeRigidConstraint.attachment.rigid_body = daeRigidBody
				daeRigidConstraint.attachment.blenderobject = bNode
				daeRigidConstraint.attachment.pivX = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_PIVX]
				daeRigidConstraint.attachment.pivY = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_PIVY]
				daeRigidConstraint.attachment.pivZ = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_PIVZ]
				daeRigidConstraint.attachment.axX = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_AXX]
				daeRigidConstraint.attachment.axY = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_AXY]
				daeRigidConstraint.attachment.axZ = rbconstraint[Blender.Constraint.Settings.CONSTR_RB_AXZ]

				if not (rbconstraint[Blender.Constraint.Settings.TARGET] is None):
					#find rigidbody that goes with TARGET...
					blendertargetob = rbconstraint[Blender.Constraint.Settings.TARGET]
					targetRbId = blendertargetob.name+'-RigidBody'
					targetrigidbody = daeGlobalPhysicsModel.FindRigidBody(targetRbId)
					if not (targetrigidbody is None):
						daeRigidConstraint.ref_attachment = collada.DaeRigidConstraint.DaeRefAttachment()
						daeRigidConstraint.ref_attachment.rigid_body = targetrigidbody
						daeRigidConstraint.ref_attachment.blenderobject = blendertargetob

						#calculate pivX/pivY/pivZ and axX,axY,axZ for this reference frame
						refObjectWorldMatrix = blendertargetob.matrix
						attachObjectWorldMatrix = bNode.matrix
						euler = Euler(daeRigidConstraint.attachment.axX,daeRigidConstraint.attachment.axY,daeRigidConstraint.attachment.axZ)
						matRot= euler.toMatrix()
						matT1 = TranslationMatrix(Vector(daeRigidConstraint.attachment.pivX,daeRigidConstraint.attachment.pivY,daeRigidConstraint.attachment.pivZ))
						matT = TranslationMatrix(Vector(0,0,0))
						matT[0][0]=matRot[0][0]
						matT[0][1]=matRot[0][1]
						matT[0][2]=matRot[0][2]
						matT[1][0]=matRot[1][0]
						matT[1][1]=matRot[1][1]
						matT[1][2]=matRot[1][2]
						matT[2][0]=matRot[2][0]
						matT[2][1]=matRot[2][1]
						matT[2][2]=matRot[2][2]
						copyMat = Matrix(refObjectWorldMatrix)
						attachLocalMatrix = matT * matT1
						invertedMat = copyMat.invert()
						globalFrameA = attachLocalMatrix * attachObjectWorldMatrix
						refAttachLocalMatrix = globalFrameA * invertedMat
						refPivot = refAttachLocalMatrix.translationPart()
						eulerAngles = refAttachLocalMatrix.toEuler()
						daeRigidConstraint.ref_attachment.pivX = refPivot[0]
						daeRigidConstraint.ref_attachment.pivY = refPivot[1]
						daeRigidConstraint.ref_attachment.pivZ = refPivot[2]
						daeRigidConstraint.ref_attachment.axX = eulerAngles[0]
						daeRigidConstraint.ref_attachment.axY = eulerAngles[1]
						daeRigidConstraint.ref_attachment.axZ = eulerAngles[2]


				#print "constraint type = ",rbconstraint[Blender.Constraint.Settings.CONSTR_RB_TYPE]

				daeRigidConstraintTechniqueCommon = collada.DaeRigidConstraint.DaeTechniqueCommon()

				daeRigidConstraintTechniqueCommon.limits.linearLimits.min = [0,0,0]
				daeRigidConstraintTechniqueCommon.limits.linearLimits.max = [0,0,0]
				daeRigidConstraintTechniqueCommon.limits.angularLimits.min = [0,0,0]
				daeRigidConstraintTechniqueCommon.limits.angularLimits.max = [0,0,0]

				#generic 6 degree-of-freedom joint
				if rbconstraint[Blender.Constraint.Settings.CONSTR_RB_TYPE] == 12:
					daeRigidConstraintTechniqueCommon.limits.linearLimits.min = [rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT0],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT1],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT2]]
					daeRigidConstraintTechniqueCommon.limits.linearLimits.max = [rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT0],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT1],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT2]]
					daeRigidConstraintTechniqueCommon.limits.angularLimits.min = [rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT3],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT4],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MINLIMIT5]]
					daeRigidConstraintTechniqueCommon.limits.angularLimits.max = [rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT3],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT4],rbconstraint[Blender.Constraint.Settings.CONSTR_RB_MAXLIMIT5]]
				#ball-socket joint
				if rbconstraint[Blender.Constraint.Settings.CONSTR_RB_TYPE] == 1:
					daeRigidConstraintTechniqueCommon.limits.linearLimits.min = [0,0,0]
					daeRigidConstraintTechniqueCommon.limits.linearLimits.max = [0,0,0]
					daeRigidConstraintTechniqueCommon.limits.angularLimits.min = ['-INF','-INF','-INF']
					daeRigidConstraintTechniqueCommon.limits.angularLimits.max = ['INF','INF','INF']
				#hinge
				if rbconstraint[Blender.Constraint.Settings.CONSTR_RB_TYPE] == 2:
					daeRigidConstraintTechniqueCommon.limits.linearLimits.min = [0,0,0]
					daeRigidConstraintTechniqueCommon.limits.linearLimits.max = [0,0,0]
					daeRigidConstraintTechniqueCommon.limits.angularLimits.min = ['-INF',0,0]
					daeRigidConstraintTechniqueCommon.limits.angularLimits.max = ['INF',0,0]


				#daeRigidConstraintTechniqueCommon.enabled = True
				daeRigidConstraint.techniqueCommon = daeRigidConstraintTechniqueCommon
				daeGlobalPhysicsModel.constraints.append(daeRigidConstraint)

				# Create a new RigidConstraint instance
				daeRigidConstraintInstance = collada.DaeRigidConstraintInstance()
				# Set the rigid body of this instance
				daeRigidConstraintInstance.constraint = daeRigidConstraint

				#add constraint instance
				daeGlobalPhysicsModelInstance.iConstraints.append(daeRigidConstraintInstance)

		return daeRigidBodyInstance

class ArmatureNode(object):
	def __init__(self, document):
		self.document = document

	def SaveToDae(self, bArmature, bArmatureObject, bPose):
		daeNodes = []
		# Get the root bones
		rootBones = []

		for boneName in bArmature.bones.keys():
			bone = bArmature.bones[boneName]
			if not bone.hasParent():
				rootBones.append(bone)

		# Only take the first root and ignore it.
		if len(rootBones) > 0 and not rootBones[0].children is None:
			daeNodes.append(self.BoneToDae(rootBones[0], bPose,Matrix(), bArmatureObject))

			##for childBone in rootBones[0].children:
##				print PrintTransforms(Matrix(bPose.bones[rootBones[0].name].poseMatrix).transpose(),rootBones[0].name)
			##	poseMatrix = Matrix(bPose.bones[rootBones[0].name].poseMatrix).transpose()
			##	poseMatrixRotInv = poseMatrix.rotationPart().invert().resize4x4()
				##poseMatrixNoRot = poseMatrix * poseMatrixRotInv

			##	print rootBones[0]
			##	print Matrix(bArmatureObject.matrix).transpose().invert()
				##daeNodes.append(self.BoneToDae(rootBones[0], bPose, Matrix(bArmatureObject.matrix).transpose().invert(),poseMatrixRotInv, bArmatureObject))

		if len(rootBones) > 1:
			print("Please use only a single root for proper export.\n" \
				"Nr. of root bones: %i.\n" \
				"List of root bones:" % len(rootBones))
			for rootBone in rootBones:
				print("\t%s" % rootBone.__str__())

		return daeNodes

	def BoneToDae(self, bBone, bPose, parentMatrix, bArmatureObject, bParentBind=None):
		daeNode = collada.DaeNode()
		daeNode.id = daeNode.name = daeNode.sid = self.document.CreateID(bBone.name, '-Joint')
		daeNode.type = collada.DaeSyntax.TYPE_JOINT

		# Get the transformations
		if dmitri:
			bindMatrix = Matrix(bBone.matrix["ARMATURESPACE"]).resize4x4().transpose()
		else:
			headPos = bBone.head["ARMATURESPACE"]
			bindMatrix = Matrix([1,0,0,headPos.x], [0,1,0,headPos.y], [0,0,1,headPos.z],[0,0,0,1])
		armMatrix = bindMatrix

		if ( not bBone.hasParent() ):
			armMatrix = Matrix(bArmatureObject.getMatrix('localspace')).transpose()
			armMatrix *= bindMatrix

		try :
			armAction = bArmatureObject.getAction()
			ipList = armAction.getAllChannelIpos()

			ip_bone_channel = ipList[bBone.name]
			ip_bone_name = ip_bone_channel.getName()
			ipo = Blender.Ipo.Get(ip_bone_name)

			if not (ipo is None):
				animation = Animation(self.document)
				if bParentBind is None:
					animation.SaveToDae(ipo, daeNode, bBone, bPose, parentMatrix, bArmatureObject)
				else:
					animation.SaveToDae(ipo, daeNode, bBone, bPose, bParentBind, bArmatureObject)
		except:
			pass

		mat = Matrix(parentMatrix).invert() * armMatrix
		boneMatrix = Matrix(bindMatrix)
		mat.transpose()
		#PrintTransforms(mat, "ARMATURESPACE: " + bBone.name)

		if bakeMatrices :
			mat = Matrix(mat).transpose()
			daeNode.transforms.append([collada.DaeSyntax.MATRIX, mat])
		else:
			translation = mat.translationPart()

			daeNode.transforms.append([collada.DaeSyntax.TRANSLATE, translation])

			euler = mat.toEuler()
##			print euler
			rotxVec = [1,0,0,euler.x]
			rotyVec = [0,1,0,euler.y]
			rotzVec = [0,0,1,euler.z]

##			print rotxVec[3], rotyVec[3], rotzVec[3]

			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotzVec])
			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotyVec])
			daeNode.transforms.append([collada.DaeSyntax.ROTATE, rotxVec])

			scale = mat.scalePart()
			daeNode.transforms.append([collada.DaeSyntax.SCALE, scale])


		if not bBone.children is None:
			for childBone in bBone.children:
				daeNode.nodes.append(self.BoneToDae(childBone, bPose, boneMatrix, bArmatureObject, boneMatrix))
		return daeNode



class MeshNode(object):
	def __init__(self,document):
		self.document = document
		self.materials = []
		self.verts = []

	def FindMaterial(self, bMaterials,name):
		for i in range(len(bMaterials)):
			if bMaterials[i].name == name:
				return i
		return -1

	def LoadFromDae(self, daeGeometry):
		global replaceNames
		meshID = daeGeometry.id
		meshName = daeGeometry.name

		if isinstance(daeGeometry.data,collada.DaeMesh): # check if it's a mesh
			# Create a new meshObject
			bMesh2 = Blender.NMesh.New(self.document.CreateNameForObject(meshID,replaceNames,'mesh'))

			materials = []
			for matName, material in self.materials.iteritems():
				materials.append(material)
			bMesh2.materials = materials
			if len(materials)>16:
				print 'Warning: Mesh-Object:"%s" has more than 16 materials:' %meshName

			faces = []
			daeMesh = daeGeometry.data
			daeVertices = daeMesh.vertices

			# Get all the sources
			sources = dict()

			for source in daeMesh.sources:
				sources[source.id] = source.vectors

			# Get the Position Input
			vPosInput = daeVertices.FindInput('POSITION')
			if not (daeMesh.FindSource(vPosInput).techniqueCommon is None):

				# Get the Normal Input
				vNorInput = daeVertices.FindInput('NORMAL')

				# Keep track of the Blender Vertices
				pVertices = []
				self.document.ProgressPart(0.0,'Create Vertices')
				vertIndex = 0
				self.verts = range(len(sources[vPosInput.source]))

				# Create all the vertices.
				for i in sources[vPosInput.source]:
					vPosVector = Vector(i[0], i[1], i[2]) * self.document.tMatOLD
					bVert = Blender.NMesh.Vert(vPosVector.x, vPosVector.y, vPosVector.z)
					self.verts[vertIndex] = bVert
					pVertices.append(bVert)
					vertIndex += 1
				bMesh2.verts = pVertices

				faceVerts = [] # The list of vertices for each face to add
				edgeVerts = [] # The list of vertices for each edge to add

				# Now create the primitives
				self.document.ProgressPart(0.5,'Create Primitives')
				daePrimitives = daeMesh.primitives
				for primitive in daePrimitives:
					# Get the UV Input
					vUvInput = primitive.FindInput('TEXCOORD')
					uvs = []
					if not (vUvInput is None):
						for i in sources[vUvInput.source]:
							vUvVector = Vector(i[0],i[1])
							uvs.append(vUvVector)
					inputs = primitive.inputs
					maxOffset = primitive.GetStride() # The number of values for one vertice
					vertCount = 0 # The number of vertices for one primitive
					realVertCount = 0 # The number of vertices for one primitive
					plist = [] # The list with all primitives
					if isinstance(primitive, collada.DaePolygons): # Polygons
						plist = primitive.polygons
						vertCount = 4
					elif isinstance(primitive, collada.DaeTriangles): # Triangles
						plist.append(primitive.triangles)
						vertCount = 3
					elif isinstance(primitive, collada.DaeLines): # Lines
						plist.append(primitive.lines)
						vertCount = 2
					elif isinstance(primitive, collada.DaePolylist): # Polylist
						plist.append(primitive.polygons)
						vertCount = 5
					elif isinstance(primitive, collada.DaeTriFans): #TriFan
						plist = primitive.trifans
						vertCount = 3
					elif isinstance(primitive, collada.DaeTriStrips): #TriStrip
						plist = primitive.tristrips
						vertCount = 3

					realVertCount = vertCount
					# Loop through each P (primitive)
					for p in plist:
						# for PolyList: Keep track of the index of the polygon
						polyListIndex = -1
						if isinstance(primitive, collada.DaeTriFans) or isinstance(primitive, collada.DaeTriStrips):
							pIndex = maxOffset * 2
							vertexInput = primitive.FindInput("VERTEX")
							uvInput = primitive.FindInput("TEXCOORD")
							norInput = primitive.FindInput("NORMAL")

							if not vertexInput is None:
								firstVertIndex = p[vertexInput.offset]
								prevVertIndex = p[maxOffset + vertexInput.offset]

							if not uvInput is None:
								firstUVIndex = p[uvInput.offset]
								prevUVIndex = p[maxOffset + uvInput.offset]
							realVertCount = 1
						else:
							pIndex = 0
						if isinstance(primitive, collada.DaePolygons): #patch from tera_api
							vertCount = len(p)/maxOffset
							realVertCount = vertCount
						# A list with edges in this face
						faceEdges = []
						# a list to store all the created faces in to add them to the mesh afterwards.
						allFaceVerts = []
						# loop through all the values in this 'p'
						while pIndex < len(p):
							# update the realVertCount
							if vertCount == 5:
								polyListIndex += 1
								realVertCount = primitive.vcount[polyListIndex]

							# Keep track of the verts in this face
							curFaceVerts2 = []
							uvList	= []
							# for every vertice for this primitive
							for i in range(realVertCount):
								# Check all the inputs and do the right thing
								for input in inputs:
									inputVal = p[pIndex+(i*maxOffset)+input.offset]
									if input.semantic == "VERTEX":
										vert2 = pVertices[inputVal]
										if isinstance(primitive, collada.DaeTriFans):
											firstVert = pVertices[firstVertIndex]
											prevVert = pVertices[prevVertIndex]
											curFaceVerts2.append(firstVert)
											curFaceVerts2.append(prevVert)
											curFaceVerts2.append(vert2)
											prevVertIndex = inputVal
										elif isinstance(primitive, collada.DaeTriStrips):
											firstVert = pVertices[firstVertIndex]
											prevVert = pVertices[prevVertIndex]
											curFaceVerts2.append(firstVert)
											curFaceVerts2.append(prevVert)
											curFaceVerts2.append(vert2)
											firstVertIndex = prevVertIndex
											prevVertIndex = inputVal
										else:
											curFaceVerts2.append(vert2)
									elif input.semantic == "TEXCOORD":
										uv2 = uvs[inputVal]
										if isinstance(primitive, collada.DaeTriFans):
											firstUV = uvs[firstUVIndex]
											prevUV = uvs[prevUVIndex]
											uvList.append((firstUV[0],firstUV[1]))
											uvList.append((prevUV[0],prevUV[1]))
											uvList.append((uv2[0],uv2[1]))
											prevUVIndex = inputVal
										elif isinstance(primitive, collada.DaeTriStrips):
											firstUV = uvs[firstUVIndex]
											prevUV = uvs[prevUVIndex]
											uvList.append((firstUV[0],firstUV[1]))
											uvList.append((prevUV[0],prevUV[1]))
											uvList.append((uv2[0],uv2[1]))
											firstUVIndex = prevUVIndex
											prevUVIndex = inputVal
										else:
											uvList.append((uvs[inputVal][0],uvs[inputVal][1]))
									elif input.semantic == "NORMAL":
										pass #TODO: support for normals

							if vertCount > 2:
								if isinstance(primitive, collada.DaeTriFans):
									faceCount = 1
								else:
									faceCount = 1 + (realVertCount-4) / 2 + (realVertCount-4) % 2
									#print 'deb: VertCount/realVertCount/faceCount:',VertCount,' / ',realVertCount,' / ',faceCount # ---------
								firstIndex = 2
								lastIndex = 1
								for a in range(faceCount):
									if isinstance(primitive, collada.DaeTriFans):
										newFirstIndex = (firstIndex + 1) % vertCount
										newLastIndex = (lastIndex -1) % vertCount
									else:
										newFirstIndex = (firstIndex + 1) % realVertCount
										newLastIndex = (lastIndex -1) % realVertCount
									fuv = []
									if newFirstIndex != newLastIndex:
										fv = [curFaceVerts2[firstIndex]] + [curFaceVerts2[newFirstIndex]] + [curFaceVerts2[newLastIndex]] +  [curFaceVerts2[lastIndex]]
										if len(uvList) == realVertCount:
											fuv = [uvList[firstIndex]] + [uvList[newFirstIndex]] + [uvList[newLastIndex]] + [uvList[lastIndex]]
									else:
										fv = [curFaceVerts2[firstIndex]] + [curFaceVerts2[newFirstIndex]] + [curFaceVerts2[lastIndex]]
										if len(uvList) == realVertCount:
											fuv = [uvList[firstIndex]] + [uvList[newFirstIndex]] + [uvList[lastIndex]]
									firstIndex = newFirstIndex
									lastIndex = newLastIndex
									# Create a new Face.
									newFace = Blender.NMesh.Face(fv)
									# Set the UV Coordinates
									newFace.uv = fuv
									# Add the new face to the list
									#bMesh2.addFace(newFace)
									faces.append(newFace)
									# Add the material to this face
									if primitive.material != '':
										if self.materials.has_key(primitive.material):
											#print 'deb: primitive.material:', primitive.material # ---------
											# Find the material index.
											matIndex = self.FindMaterial(bMesh2.materials, self.materials[primitive.material].name)
											# Set the material index for the new face.
											if 15 > matIndex > -1:
												newFace.materialIndex = matIndex
											else:
												newFace.materialIndex = 15 #TODO: fake for all material index bigger than 15
												print 'material Index:%s, outside specification! set to 15 ----' %matIndex # ---------
											textures = self.materials[primitive.material].getTextures()
											#if len(textures) > 0 and textures[0]!=None:
											for texture in textures: #searching for any texture-image
												if texture is not None:
													image = texture.tex.getImage()
													if image is not None:
														newFace.image = image
														break
										else:
											print "Warning: Cannot find material:", primitive.material
							elif vertCount == 2:
								bMesh2.addEdge(curFaceVerts2[0], curFaceVerts2[1])
							else: pass
							# update the index
							if isinstance(primitive, collada.DaeTriFans) or isinstance(primitive, collada.DaeTriStrips):
								pIndex += maxOffset
							else:
								pIndex += realVertCount * maxOffset

				bMesh2.faces = faces
			return bMesh2
		return

	def SaveToDae(self, mesh):
		global useTriangles, usePolygons, useUV

		uvTextures = dict()
		uvIndex = dict()

		daeGeometry = collada.DaeGeometry()
		daeGeometry.id = daeGeometry.name = self.document.CreateID(mesh.name,'-Geometry')

		daeMesh = collada.DaeMesh()

		# Keep track of the edges in faces
		faceEdges = []

		daeSource = collada.DaeSource()
		daeSource.id = self.document.CreateID(daeGeometry.id,'-Position')
		daeFloatArray = collada.DaeFloatArray()
		daeFloatArray.id = self.document.CreateID(daeSource.id,'-array')
		daeSource.source = daeFloatArray
		daeSource.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		accessor = collada.DaeAccessor()
		daeSource.techniqueCommon.accessor = accessor
		accessor.source = daeFloatArray.id
		accessor.count = len(mesh.verts)
		accessor.AddParam('X','float')
		accessor.AddParam('Y','float')
		accessor.AddParam('Z','float')

		daeSourceNormals = collada.DaeSource()
		daeSourceNormals.id = self.document.CreateID(daeGeometry.id,'-Normals')
		daeFloatArrayNormals = collada.DaeFloatArray()
		daeFloatArrayNormals.id = self.document.CreateID(daeSourceNormals.id,'-array')
		daeSourceNormals.source = daeFloatArrayNormals
		daeSourceNormals.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		accessorNormals = collada.DaeAccessor()
		daeSourceNormals.techniqueCommon.accessor = accessorNormals
		accessorNormals.source = daeFloatArrayNormals.id
		accessorNormals.count = 0
		accessorNormals.AddParam('X','float')
		accessorNormals.AddParam('Y','float')
		accessorNormals.AddParam('Z','float')

		daeSourceTextures = collada.DaeSource()
		daeSourceTextures.id = self.document.CreateID(daeGeometry.id , '-UV')
		daeFloatArrayTextures = collada.DaeFloatArray()
		daeFloatArrayTextures.id = self.document.CreateID(daeSourceTextures.id,'-array')
		daeSourceTextures.source = daeFloatArrayTextures
		daeSourceTextures.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
		accessorTextures = collada.DaeAccessor()
		daeSourceTextures.techniqueCommon.accessor = accessorTextures
		accessorTextures.source = daeFloatArrayTextures.id
		accessorTextures.AddParam('S','float')
		accessorTextures.AddParam('T','float')


		daeVertices = collada.DaeVertices()
		daeVertices.id = self.document.CreateID(daeGeometry.id,'-Vertex')
		daeInput = collada.DaeInput()
		daeInput.semantic = 'POSITION'
		daeInput.source = daeSource.id
		daeVertices.inputs.append(daeInput)
		daeInputNormals = collada.DaeInput()
		daeInputNormals.semantic = 'NORMAL'
		daeInputNormals.source = daeSourceNormals.id
		daeInputNormals.offset = 1
		## daeVertices.inputs.append(daeInputNormals)
		daeMesh.vertices = daeVertices

		hasColor = False
		#Vertex colors:
		if ( mesh.vertexColors ) :
			hasColor = True
			daeSourceColors = collada.DaeSource()
			daeSourceColors.id = self.document.CreateID(daeGeometry.id , '-color')
			daeFloatArrayColors = collada.DaeFloatArray()
			daeFloatArrayColors.id = self.document.CreateID(daeSourceColors.id,'-array')
			daeSourceColors.source = daeFloatArrayColors
			daeSourceColors.techniqueCommon = collada.DaeSource.DaeTechniqueCommon()
			accessorColors = collada.DaeAccessor()
			daeSourceColors.techniqueCommon.accessor = accessorColors
			accessorColors.source = daeFloatArrayColors.id
			accessorColors.AddParam('R','float')
			accessorColors.AddParam('G','float')
			accessorColors.AddParam('B','float')
			accessorColors.AddParam('A','float')

		for vert in mesh.verts:
			#print vert
			for co in vert.co:
				daeFloatArray.data.append(co)
			## for no in vert.no:
			##	daeFloatArrayNormals.data.append(no)


		daePolygonsDict = dict()
		daeTrianglesDict = dict()
		daeLines = None


		# Loop trough all the faces
		for face in mesh.faces:
			matIndex = -1
			if not useUV and mesh.materials and len(mesh.materials) > 0:
				matIndex = face.mat
			elif mesh.faceUV and (useUV or mesh.materials is None or len(mesh.materials) == 0):
				if not face.image is None:
					matIndex = face.image.name

			vertCount = len(face.verts)

			# Create a new function which adds vertices to a list.
			def AddVerts(verts,prlist,smooth,secondTriangle=False):
				secondTriangleIndex = [2,3,0]

				prevVert = None
				lastVert = verts[-1]
				for v_index in range(len(verts)):
					# Get the vertice
					vert = verts[v_index]
					vert_index = v_index
					# Add the vertice to the end of the list.
					prlist.append(vert.index)

					# Add the normals of these verts
					if smooth:
						for no in vert.no:
							daeFloatArrayNormals.data.append(no)
						accessorNormals.count = accessorNormals.count + 1

					#prlist.append(len(daeFloatArrayNormals.data)/3-1)
					prlist.append(accessorNormals.count - 1)


					if hasColor:
						if secondTriangle:
							vert_index = secondTriangleIndex[vert_index]
						daeFloatArrayColors.data.append(face.col[vert_index].r / 255.0)
						daeFloatArrayColors.data.append(face.col[vert_index].g / 255.0)
						daeFloatArrayColors.data.append(face.col[vert_index].b / 255.0)
						daeFloatArrayColors.data.append(face.col[vert_index].a / 255.0)
						prlist.append(len(daeFloatArrayColors.data)/4-1)
						accessorColors.count = accessorColors.count + 1
						vert_index = v_index

					# If the mesh has UV-coords, add these to the textures array.
					if mesh.faceUV:
						if secondTriangle:
							vert_index = secondTriangleIndex[vert_index]
						daeFloatArrayTextures.data.append(face.uv[vert_index][0])
						daeFloatArrayTextures.data.append(face.uv[vert_index][1])
						prlist.append(len(daeFloatArrayTextures.data)/2-1)
						accessorTextures.count = accessorTextures.count + 1

					# Add the edge to the faceEdges list.
					if prevVert != None:
						faceEdges.append(mesh.findEdges(prevVert, vert))
					else:
						faceEdges.append(mesh.findEdges([(vert, lastVert)]))
					# Update the prevVert vertice.
					prevVert = vert

			# Now use that AddVerts(...) function exactly in this "if"-block
			if (vertCount == 3 and not usePolygons) or (useTriangles and vertCount == 4): # triangle
				# Iff a Triangle Item for the current material not exists, create one.
				daeTrianglesDict.setdefault(matIndex, collada.DaeTriangles())
				if not face.smooth:
					# Add the face normal to the normals source
					for no in face.no:
						daeFloatArrayNormals.data.append(no)
					accessorNormals.count = accessorNormals.count + 1
				if vertCount == 3:
					# Add all the vertices to the triangle list.
					AddVerts(face.verts,daeTrianglesDict[matIndex].triangles, face.smooth )
					# Update the vertice count for the trianglelist.
					daeTrianglesDict[matIndex].count += 1
				else: # Convert polygon to triangles
					verts1 = face.verts[:3]
					verts2 = face.verts[2:] + tuple([face.verts[0]])
					# Add all the vertices to the triangle list.
					AddVerts(verts1,daeTrianglesDict[matIndex].triangles, face.smooth)
					AddVerts(verts2, daeTrianglesDict[matIndex].triangles,face.smooth,True)
					# Update the vertice count for the trianglelist.
					daeTrianglesDict[matIndex].count += 2
			else: # polygon
				# Iff a Polygon Item for the current material not exists, create one.
				daePolygonsDict.setdefault(matIndex, collada.DaePolygons())
				# for each polygon, create a new P element
				pverts = []
				if not face.smooth:
					# Add the face normal to the normals source
					for no in face.no:
						daeFloatArrayNormals.data.append(no)
					accessorNormals.count = accessorNormals.count + 1
				# Add all the vertices to the pverts list.
				AddVerts(face.verts,pverts, face.smooth)
				# Add the pverts list to the polygons list.
				daePolygonsDict[matIndex].polygons.append(pverts)
				# Update the vertice count for the polygonlist.
				daePolygonsDict[matIndex].count += 1
			if mesh.faceUV:
				if not face.image is None:
					if not face.image.name in uvTextures:
						uvIndex[face.image.name] = face.mat
						uvTextures[face.image.name] = self.document.CreateID(face.image.name, "-Material")
		# Loop through all the edges
		for edge in mesh.edges:
			if not edge.index in faceEdges:
				if daeLines == None: daeLines = collada.DaeLines()
				daeLines.count += 1
				daeLines.lines.append(edge.v1.index)
				daeLines.lines.append(edge.v2.index)

		daeInput = collada.DaeInput()
		daeInput.semantic = 'VERTEX'
		daeInput.offset = 0
		daeInput.source = daeVertices.id

		daeInputUV = collada.DaeInput()
		daeInputUV.semantic = 'TEXCOORD'
		daeInputUV.offset = 2
		daeInputUV.source = daeSourceTextures.id

		if hasColor:
			daeInputColor = collada.DaeInput()
			daeInputColor.semantic = 'COLOR'
			daeInputColor.offset = 3
			daeInputColor.source = daeSourceColors.id

		materialName = ''

		for k, daeTriangles in daeTrianglesDict.iteritems():
			##print k
			if k != -1:
				if not useUV and not mesh.materials is None and len(mesh.materials) > 0 and k >= 0:
					daeTriangles.material = mesh.materials[k].name
				elif mesh.faceUV and (useUV or mesh.materials is None or len(mesh.materials) == 0):
					daeTriangles.material = uvTextures[k]
			offsetCount = 0
			daeInput.offset = offsetCount
			daeTriangles.inputs.append(daeInput)
			offsetCount += 1

			daeInputNormals.offset = offsetCount
			daeTriangles.inputs.append(daeInputNormals)
			offsetCount += 1

			if mesh.faceUV:
				daeInputUV.offset = offsetCount
				daeTriangles.inputs.append(daeInputUV)
				offsetCount += 1
			if hasColor:
				daeInputColor.offset = offsetCount
				daeTriangles.inputs.append(daeInputColor)
				offsetCount += 1
			daeMesh.primitives.append(daeTriangles)
		for k, daePolygons in daePolygonsDict.iteritems():
			if k != -1:
				if not useUV and not mesh.materials is None and len(mesh.materials) > 0 and k >= 0:
					daePolygons.material = getattr(mesh.materials[k], 'name', "")
				elif mesh.faceUV and (useUV or mesh.materials is None or len(mesh.materials) == 0):
					daePolygons.material = uvTextures[k]
			offsetCount = 0
			daeInput.offset = offsetCount
			daePolygons.inputs.append(daeInput)
			offsetCount += 1

			daeInputNormals.offset = offsetCount
			daePolygons.inputs.append(daeInputNormals)
			offsetCount += 1

			if mesh.faceUV:
				daeInputUV.offset = offsetCount
				daePolygons.inputs.append(daeInputUV)
				offsetCount += 1
			if hasColor:
				daeInputColor.offset = offsetCount
				daePolygons.inputs.append(daeInputColor)
				offsetCount += 1
			daeMesh.primitives.append(daePolygons)

		if daeLines != None:
			daeLines.inputs.append(daeInput)
			daeMesh.primitives.append(daeLines)

		daeMesh.sources.append(daeSource)
		daeMesh.sources.append(daeSourceNormals)
		if mesh.faceUV:
			daeMesh.sources.append(daeSourceTextures)
		if hasColor:
			daeMesh.sources.append(daeSourceColors)

		daeGeometry.data = daeMesh

		self.document.colladaDocument.geometriesLibrary.AddItem(daeGeometry)
		daeGeometry.uvTextures = uvTextures
		daeGeometry.uvIndex = uvIndex
		return daeGeometry

	def GetBindMaterials(self, bMesh, uvTextures, uvIndex):
		global useUV

		bindMaterials = []
		mesh = Blender.Mesh.Get(bMesh.name)
		# now check the materials


		if (not useUV) and bMesh.materials and len(bMesh.materials) > 0:
			daeBindMaterial = collada.DaeFxBindMaterial()
			for bMaterial in bMesh.materials:
				instance = collada.DaeFxMaterialInstance()
				daeMaterial = self.document.colladaDocument.materialsLibrary.FindObject(bMaterial.name)
				if daeMaterial is None:
					materialNode = MaterialNode(self.document)
					daeMaterial = materialNode.SaveToDae(bMaterial)
				instance.object = daeMaterial
				daeBindMaterial.techniqueCommon.iMaterials.append(instance)
			# now we have to add this bindmaterial to the intance of this geometry
			bindMaterials.append(daeBindMaterial)
		elif mesh.faceUV and (useUV or bMesh.materials is None or len(bMesh.materials) == 0):
			daeBindMaterial = collada.DaeFxBindMaterial()
			for imageName, imageNameUnique in uvTextures.iteritems():
				image = Blender.Image.Get(imageName)
				instance = collada.DaeFxMaterialInstance()
				daeMaterial = self.document.colladaDocument.materialsLibrary.FindObject(imageNameUnique)
				if daeMaterial is None:
					textureNode = TextureNode(self.document)
					daeMaterial = textureNode.SaveToDae(image)
					daeMaterial.id = daeMaterial.name = imageNameUnique

					if len(bMesh.materials) > 0 :
						materialIndex = uvIndex[imageName]
						bMaterial = bMesh.materials[materialIndex]

						materialNode = MaterialNode(self.document)
						materialShader = materialNode.GenerateShader(bMaterial, False)
						textureDiffuse = daeMaterial.iEffects[0].object.profileCommon.technique.shader.diffuse
						materialShader.diffuse = textureDiffuse
						daeMaterial.iEffects[0].object.AddShader(materialShader)


				instance.object = daeMaterial
				daeBindMaterial.techniqueCommon.iMaterials.append(instance)
			# now we have to add this bindmaterial to the intance of this geometry
			bindMaterials.append(daeBindMaterial)
		return bindMaterials

class TextureNode(object):
	def __init__(self, document):
		self.document = document

	def LoadFromDae(self, daeImage):
		imageID = daeImage.id
		imageName = daeImage.name

		bTexture = Blender.Texture.New(imageID)

		filename = ''
		if isinstance(daeImage.initFrom, str):
			if Blender.sys.exists(daeImage.initFrom):
				filename = daeImage.initFrom
			elif Blender.sys.exists(self.document.filePath + daeImage.initFrom):
				filename = self.document.filePath + daeImage.initFrom

		if filename <> '':
			bTexture.setType('Image')
			img = Blender.Image.Load(filename)
			bTexture.setImage(img)
		else:
			print 'image not found: %s'%(daeImage.initFrom)

		return bTexture


	def AddImageTexture(self, daeEffect, bImage):
		#creating surface
		daeSurfaceParam = collada.DaeFxNewParam()
		surfaceId = self.document.CreateID(bImage.name,'-surface')
		daeSurfaceParam.sid = surfaceId

		daeSurface = collada.DaeFxSurface()
		daeSurfaceParam.surface = daeSurface
		daeSurface.initfrom = bImage.name + "-img"
		daeEffect.profileCommon.newParams.append( daeSurfaceParam )

		#creating Sampler
		daeSamplerParam = collada.DaeFxNewParam()
		samplerId = self.document.CreateID(bImage.name,'-sampler')
		daeSamplerParam.sid = samplerId

		daeSampler = collada.DaeFxSampler2D()

		daeSamplerParam.sampler = daeSampler
		daeSampler.source.value = surfaceId
		daeEffect.profileCommon.newParams.append( daeSamplerParam )

		shader = collada.DaeFxShadeLambert()
		daeImage = self.document.colladaDocument.imagesLibrary.FindObject(bImage.name)
		if daeImage is None: # Create the image
			daeImage = collada.DaeImage()
			daeImage.id = daeImage.name = bImage.name + "-img"

			daeImage.initFrom = Blender.sys.expandpath(bImage.filename)
			if useRelativePaths:
				daeImage.initFrom = CreateRelativePath(filename, daeImage.initFrom)
			self.document.colladaDocument.imagesLibrary.AddItem(daeImage)
			daeTexture = collada.DaeFxTexture()
			daeTexture.texture = samplerId
			shader.AddValue(collada.DaeFxSyntax.DIFFUSE, daeTexture)

		daeEffect.AddShader(shader)
		return

	def SaveToDae(self, bImage):
		daeMaterial = collada.DaeFxMaterial()
		##daeMaterial.id = daeMaterial.name = self.document.CreateID(bImage.name, '-Material')
		self.document.colladaDocument.materialsLibrary.AddItem(daeMaterial)

		instance = collada.DaeFxEffectInstance()
		daeEffect = self.document.colladaDocument.effectsLibrary.FindObject(bImage.name+'-fx')

		if daeEffect is None:
			daeEffect = collada.DaeFxEffect()
			daeEffect.id = daeEffect.name = self.document.CreateID(bImage.name , '-fx')

			#Add the texture stuff!!
			self.AddImageTexture(daeEffect, bImage)

			self.document.colladaDocument.effectsLibrary.AddItem(daeEffect)
		instance.object = daeEffect

		daeMaterial.iEffects.append(instance)

		return daeMaterial

class MaterialNode(object):
	def __init__(self, document):
		self.document = document

	def LoadFromDae(self, daeMaterial):
		materialID = daeMaterial.id
		materialName = daeMaterial.name

		bMat = Blender.Material.New(materialID)
		for i in daeMaterial.iEffects:
			daeEffect = self.document.colladaDocument.effectsLibrary.FindObject(i.url)
			if not (daeEffect.profileCommon is None):
				shader = daeEffect.profileCommon.technique.shader
				if shader.transparent:
					if shader.transparent.color!=None:
						tcol = shader.transparent.color.rgba
						tkey = 1
						if shader.transparency:
							tkey = shader.transparency.float
						alpha = 1 - tkey * (tcol[0]*0.21 + tcol[1]*0.71 + tcol[2]*0.08)
						bMat.setAlpha(alpha)
					if shader.transparent.texture!=None: # Texture
						textureSampler = shader.transparent.texture.texture
						if debprn: print "shader:" #---------
						if debprn: print shader.transparent.texture #---------
						if debprn: print "shader end" #---------
						if not(textureSampler is None):
							#support 1.4.0:
							texture = textureSampler
							#support 1.4.1
							for newParam in daeEffect.profileCommon.newParams:
								if newParam.sid == textureSampler:
									surfaceID = newParam.sampler.source
									for newSurface in daeEffect.profileCommon.newParams:
										if newSurface.sid == surfaceID:
											texture = newSurface.surface.initfrom

							texture = self.document.colladaDocument.imagesLibrary.FindObject(texture)
							bTexture = self.document.texturesLibrary.FindObject(texture, True)
							if not bTexture is None:
								bMat.setTexture(0, bTexture, Blender.Texture.TexCo.UV, Blender.Texture.MapTo.ALPHA)
				elif shader.transparency:
					alpha = 1 - shader.transparency.float
				if shader.reflective:
					if shader.reflective.color:
						# TODO: bug: shader.reflective.color.rgba raise error NoneType with file F40.DAE
						if debprn: print 'deb: shader.reflective.color=',shader.reflective.color #-------
						if debprn: print 'deb: shader.reflective.color.rgba=',shader.reflective.color.rgba #-------
						color = shader.reflective.color.rgba
						bMat.setMirCol(color[0], color[1], color[2])
				if shader.reflectivity:
					bMat.setRayMirr(shader.reflectivity.float)

				if isinstance(shader,collada.DaeFxShadeLambert) or isinstance(shader, collada.DaeFxShadeBlinn) or isinstance(shader, collada.DaeFxShadePhong):
					bMat.setDiffuseShader(Blender.Material.Shaders.DIFFUSE_LAMBERT)
					if shader.diffuse:
						##print shader.diffuse.color.rgba, shader.diffuse.color
						if not (shader.diffuse.color is None):
							color = shader.diffuse.color.rgba
							bMat.setRGBCol(color[0],color[1], color[2])
						if not (shader.diffuse.texture is None): # Texture
							textureSampler = shader.diffuse.texture.texture
							if not (textureSampler is None):
								#support 1.4.0:
								texture = textureSampler

								#support 1.4.1
								for newParam in daeEffect.profileCommon.newParams:
									if newParam.sid == textureSampler:
										surfaceID = newParam.sampler.source
										for newSurface in daeEffect.profileCommon.newParams:
											if newSurface.sid == surfaceID:
												texture = newSurface.surface.initfrom

								texture = self.document.colladaDocument.imagesLibrary.FindObject(texture)
								bTexture = self.document.texturesLibrary.FindObject(texture, True)
								if not bTexture is None:
									bMat.setTexture(0, bTexture, Blender.Texture.TexCo.UV)

					bMat.setSpec(0)

				if isinstance(shader, collada.DaeFxShadeBlinn) or isinstance(shader, collada.DaeFxShadePhong):
					if not isinstance(shader, collada.DaeFxShadePhong):
						bMat.setSpecShader(Blender.Material.Shaders.SPEC_BLINN)
						if shader.indexOfRefraction:
							shader.indexOfRefraction.float
							bMat.setRefracIndex(shader.indexOfRefraction.float)
					else:
						bMat.setSpecShader(Blender.Material.Shaders.SPEC_PHONG)
					if shader.specular:
						if not (shader.specular.color is None): #it's a color and not a texture
							specColor = shader.specular.color.rgba
							bMat.setSpecCol(specColor[0], specColor[1], specColor[2])
					bMat.setSpec(1)
					if shader.shininess:
						bMat.setHardness(int(shader.shininess.float) * 4)

		return bMat

	def GenerateShader(self, bMaterial, updateShader):
		shader = updateShader
		previousDiffuse = None

		if shader != None:
			try:
				previousDiffuse = shader.diffuse
			except:
				pass

		if bMaterial.getSpec() > 0.0:
			if bMaterial.getSpecShader() == Blender.Material.Shaders.SPEC_BLINN:
				shader = collada.DaeFxShadeBlinn()
				shader.AddValue(collada.DaeFxSyntax.INDEXOFREFRACTION, bMaterial.getRefracIndex())
			else:
				shader = collada.DaeFxShadePhong()
			shader.AddValue(collada.DaeFxSyntax.SPECULAR, [col * bMaterial.getSpec() for col in bMaterial.getSpecCol()]+[1])
			shader.AddValue(collada.DaeFxSyntax.SHININESS, bMaterial.getHardness() * 0.25)
		else:
			shader = collada.DaeFxShadeLambert()

		shader.AddValue(collada.DaeFxSyntax.DIFFUSE,bMaterial.getRGBCol()+[1])
		shader.AddValue(collada.DaeFxSyntax.TRANSPARENCY, 1 - bMaterial.alpha)
		shader.AddValue(collada.DaeFxSyntax.TRANSPARENT, [1,1,1,1])
		mainColor = bMaterial.getRGBCol()
		white = [1.0,1.0,1.0]

		shader.AddValue(collada.DaeFxSyntax.EMISSION, [col * bMaterial.getEmit() for col in white] + [1])
		shader.AddValue(collada.DaeFxSyntax.AMBIENT, [col * bMaterial.getAmb() for col in mainColor] + [1])
		shader.AddValue(collada.DaeFxSyntax.REFLECTIVE, bMaterial.getMirCol() + [1])
		shader.AddValue(collada.DaeFxSyntax.REFLECTIVITY, bMaterial.getRayMirr())

		if previousDiffuse != None:
			shader.diffuse = previousDiffuse

		return shader

	def SaveToDae(self, bMaterial):
		global useRelativePaths, filename
		daeMaterial = collada.DaeFxMaterial()
		daeMaterial.id = daeMaterial.name = self.document.CreateID(bMaterial.name, '-Material')

		instance = collada.DaeFxEffectInstance()
		daeEffect = self.document.colladaDocument.effectsLibrary.FindObject(bMaterial.name+'-fx')
		meshNode = MeshNode(self.document)
		if daeEffect is None:
			daeEffect = collada.DaeFxEffect()
			daeEffect.id = daeEffect.name = self.document.CreateID(bMaterial.name , '-fx')

			# Check if a texture is used for color
			textures = bMaterial.getTextures()
			for mTex in textures:
				# Check if this texture is mapped to Color
				if not mTex is None and mTex.mapto == Blender.Texture.MapTo.COL and mTex.tex.image != None:
					texture = mTex.tex
					textureNode = TextureNode(self.document)
					textureNode.AddImageTexture(daeEffect, texture.image)

			shader = self.GenerateShader(bMaterial, daeEffect.profileCommon.technique.shader)
			daeEffect.AddShader(shader)

			self.document.colladaDocument.effectsLibrary.AddItem(daeEffect)
		instance.object = daeEffect

		daeMaterial.iEffects.append(instance)

		self.document.colladaDocument.materialsLibrary.AddItem(daeMaterial)
		return daeMaterial

class CameraNode(object):
	def __init__(self,document):
		self.document = document

	def LoadFromDae(self, daeCamera):
		camID = daeCamera.id
		camName = daeCamera.name
		camType = 'persp'
		clipStart = daeCamera.optics.techniqueCommon.znear
		clipEnd = daeCamera.optics.techniqueCommon.zfar

		if daeCamera.optics.techniqueCommon.GetType() == collada.DaeSyntax.ORTHOGRAPHIC:
			camType = 'ortho'

		camera = Blender.Camera.New(camType,camID)
		camera.clipStart = clipStart
		camera.clipEnd = clipEnd
		return camera


	def SaveToDae(self, bCamera):
		daeCamera = collada.DaeCamera()
		daeCamera.id = daeCamera.name = self.document.CreateID(bCamera.name,'-Camera')
		daeOptics = collada.DaeOptics()
		daeTechniqueCommon = None
		if bCamera.type == 1: # orthographic
			daeTechniqueCommon = collada.DaeOptics.DaeOrthoGraphic()
		else: # perspective
			daeTechniqueCommon = collada.DaeOptics.DaePerspective()
			lens = bCamera.getLens( )
			daeTechniqueCommon.yfov = 2 * ( math.atan( 16.0 / lens ) ) * radianToAngle

		daeTechniqueCommon.znear = bCamera.clipStart
		daeTechniqueCommon.zfar = bCamera.clipEnd

		daeOptics.techniqueCommon = daeTechniqueCommon
		daeCamera.optics = daeOptics

		self.document.colladaDocument.camerasLibrary.AddItem(daeCamera)
		return daeCamera

class LampNode(object):
	def __init__(self,document):
		self.document = document

	def LoadFromDae(self, daeLight):

		lampID = daeLight.id
		lampName = daeLight.name

		# Create a new lampObject
		lamp = Blender.Lamp.New('Sun',lampID)

		if daeLight.techniqueCommon.GetType() == collada.DaeSyntax.DIRECTIONAL:
			lamp.type = Blender.Lamp.Types.Sun
		elif daeLight.techniqueCommon.GetType() == collada.DaeSyntax.POINT:
			lamp.type = Blender.Lamp.Types.Lamp
			# Get the attenuation
			constAtt = daeLight.techniqueCommon.constantAttenuation
			linAtt = daeLight.techniqueCommon.linearAttenuation
			# set the attenuation
			lamp.energy = 1-constAtt
			if linAtt > 0.0:
				lamp.dist = (lamp.energy/2)/ linAtt
			else:
				lamp.dist = 5000.0
		elif daeLight.techniqueCommon.GetType() == collada.DaeSyntax.SPOT:
			lamp.type = Blender.Lamp.Types.Spot
			# Get the attenuation
			constAtt = daeLight.techniqueCommon.constantAttenuation
			linAtt = daeLight.techniqueCommon.linearAttenuation
			# set the attenuation
			lamp.energy = 1-constAtt
			if linAtt > 0.0:
				lamp.dist = (0.5 - constAtt)/ linAtt
			else:
				lamp.dist = 5000.0
			lamp.spotSize = daeLight.techniqueCommon.falloffAngle
			lamp.spotBlend = daeLight.techniqueCommon.falloffExponent
		elif daeLight.techniqueCommon.GetType() == collada.DaeSyntax.AMBIENT:
			lamp.type = Blender.Lamp.Types.Hemi


		# Set the color
		lamp.col = daeLight.techniqueCommon.color

		return lamp

	def SaveToDae(self, bLamp):
		daeLight = collada.DaeLight()
		daeLight.id = daeLight.name = self.document.CreateID(bLamp.name,'-Light')

		daeTechniqueCommon = None
		if bLamp.type == Blender.Lamp.Types.Hemi: # Ambient
			daeTechniqueCommon = collada.DaeLight.DaeAmbient()
		elif bLamp.type == Blender.Lamp.Types.Lamp: # Point light
			daeTechniqueCommon = collada.DaeLight.DaePoint()
		elif bLamp.type == Blender.Lamp.Types.Spot: # Spot
			daeTechniqueCommon = collada.DaeLight.DaeSpot()
			daeTechniqueCommon.constantAttenuation = 1-bLamp.energy
			daeTechniqueCommon.linearAttenuation = (0.5 - daeTechniqueCommon.constantAttenuation)/bLamp.dist
			# Export the falloff Angle.
			daeTechniqueCommon.falloffAngle = bLamp.getSpotSize()
		elif bLamp.type == Blender.Lamp.Types.Sun: # Directional
			daeTechniqueCommon = collada.DaeLight.DaeDirectional()
		else: # area
			daeTechniqueCommon = collada.DaeOptics.DaeTechniqueCommon()

		daeTechniqueCommon.color = bLamp.col
		daeLight.techniqueCommon = daeTechniqueCommon

		self.document.colladaDocument.lightsLibrary.AddItem(daeLight)
		return daeLight

class Library(object):
	def __init__(self, document):
		self.objects = dict()
		self.document = document
		self.daeLibrary = None

	def SetDaeLibrary(self, daeLibrary):
		self.daeLibrary = daeLibrary

	def FindObject(self, daeInstance, fromDae, bObject = None):
		for k in self.objects:

			if 'url' in dir(daeInstance) and k == daeInstance.url:
				return self.objects[k][0]
			elif 'target' in dir(daeInstance) and k == daeInstance.target:
				return self.objects[k][0]
			elif isinstance(daeInstance, str):
				return self.objects[k][0]

		if fromDae:
			# dataObject not in library, so add it
			return self.LoadFromDae(daeInstance, bObject)
		else:
			return self.SaveToDae(daeIntance)

	def FindObjectTotal(self, daeInstance):
		for k in self.objects:
			if 'url' in dir(daeInstance) and k == daeInstance.url:
				return self.objects[k]
			elif 'target' in dir(daeInstance) and k == daeInstance.target:
				return self.objects[k]
			elif isinstance(daeInstance, str):
				return self.objects[k]
		return None

	def FindObjectEx(self, bObject):
		for k in self.objects:
			if k == id:
				return self.objects[k][0]

		return self.SaveToDae(bObject)


	def LoadFromDae(self, daeInstance):
		Debug.Debug('Library: Please override this method','WARNING')

	def SaveToDae(self, bScene):
		Debug.Debug('Library: Please override this method','WARNING')

class ScenesLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeVisualScene = self.daeLibrary.FindObject(daeInstance.url)
		# TODO: Scene: implement multiple scenes
		return None

	def SaveToDae(self, id):
		pass
		#print bScene

class CamerasLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeCamera = self.daeLibrary.FindObject(daeInstance.url)
		if daeCamera is None:
			Debug.Debug('CamerasLibrary: Object with this ID does not exist','ERROR')
			return

		camID = daeCamera.id
		camName = daeCamera.name

		cameraNode = CameraNode(self.document)
		camera = cameraNode.LoadFromDae(daeCamera)

		self.objects[camID] = [camera,camera.name]
		return camera

	def SaveToDae(self, id):
		pass

class LampsLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeLight = self.daeLibrary.FindObject(daeInstance.url)
		if daeLight is None:
			Debug.Debug('LightsLibrary: Object with this ID does not exist','ERROR')
			return
		lampID = daeLight.id
		lampName = daeLight.name

		lampNode = LampNode(self.document)
		lamp = lampNode.LoadFromDae(daeLight)

		self.objects[lampID] = [lamp,lamp.name]
		return lamp

	def SaveToDae(self, id):
		pass

class MeshLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeGeometry = None
		if isinstance(daeInstance, collada.DaeInstance):
			daeGeometry = self.daeLibrary.FindObject(daeInstance.url)
		else:
			##print daeInstance
			daeGeometry = self.daeLibrary.FindObject(daeInstance)
		if daeGeometry is None:
			Debug.Debug('MeshLibrary: Object with this ID does not exist: %s'%(daeInstance.url),'ERROR')
			return
		meshID = daeGeometry.id
		meshName = daeGeometry.name

		meshNode = MeshNode(self.document)

		# Get the materials ( only get the first one right now)
		bMaterials = daeInstance.bindMaterials
		meshNode.materials = dict()
		if bMaterials:
			for bMaterial in bMaterials:
				for iMaterial in bMaterial.techniqueCommon.iMaterials:
					Material = self.document.materialsLibrary.FindObject(iMaterial,True)
					meshNode.materials[iMaterial.symbol] = Material

		bMesh = meshNode.LoadFromDae(daeGeometry)

		# Add this mesh in this library, under it's real name
		self.objects[meshID] = [bMesh,bMesh.name, meshNode]
		return bMesh

	def SaveToDae(self, id):
		pass

class MaterialsLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeMaterial = self.daeLibrary.FindObject(daeInstance.target)
		if daeMaterial is None:
			Debug.Debug('MaterialLibrary: Object with this TARGET:%s does not exist'%(daeInstance.target),'ERROR')
			return
		materialID = daeMaterial.id
		materialName = daeMaterial.name

		materialNode = MaterialNode(self.document)

		bMaterial = materialNode.LoadFromDae(daeMaterial)

		# Add this mesh in this library, under it's real name
		self.objects[materialID] = [bMaterial,bMaterial.name]
		return bMaterial

	def SaveToDae(self, id):
		pass

class TexturesLibrary(Library):

	def LoadFromDae(self, daeImage, bObject):
		if daeImage is None:
			Debug.Debug('TexturesLibrary: Object with this TARGET:%s does not exist'%(daeImage),'ERROR')
			return
		imageID = daeImage.id
		imageName = daeImage.name

		textureNode = TextureNode(self.document)
		bTexture = textureNode.LoadFromDae(daeImage)
		bTexture.setType('Image')

		# Add this texture in this library, under it's real name
		self.objects[imageID] = [bTexture,bTexture.name]
		return bTexture

	def SaveToDae(self, id):
		pass

class AnimationsLibrary(Library):

	def LoadFromDae(self, animationName, bObject):
		if debprn: print 'deb:class AnimationsLibrary <<    <<LoadFromDae' #------------
		daeAnimation = self.daeLibrary.FindObject(animationName)
		if daeAnimation is None:
			return
		animationID = daeAnimation.id
		animationName = daeAnimation.name

		animation = Animation(self.document)
		animation.LoadFromDae(daeAnimation)
		##self.objects[animationID] = [animation, animation.name]
		return animation

	def GetDaeAnimations(self, daeNodeId):
		#if debprn: print 'deb:class AnimationsLibrary -------- GetDaeAnimations:' #------------
		#if debprn: print 'deb:----> daeNodeId=',daeNodeId #---------
		daeAnimations = []
		for daeAnimation in self.daeLibrary.items:
			#if debprn: print 'deb: daeAnimation=', daeAnimation #-----------
			for channel in daeAnimation.channels:
				ta = channel.target.split("/", 1)
				#if debprn: print 'deb:------> ta=', ta #---------
				if ta[0] == daeNodeId:
					daeAnimations.append(daeAnimation)
		#if debprn: print 'deb:--> daeAnimations=', daeAnimations #------------
		return daeAnimations

class ControllersLibrary(Library):

	def LoadFromDae(self, daeInstance, bObject):
		daeController = self.daeLibrary.FindObject(daeInstance.url)
		if daeController is None:
			Debug.Debug('ControllersLibrary: Object with this TARGET:%s does not exist'%(daeInstance.target),'ERROR')
			return
		controllerID = daeController.id
		controllerName = daeController.name

		controller = Controller(self.document)
		bMesh = controller.LoadFromDae(daeController, daeInstance, bObject)

		# Add this mesh in this library, under it's real name
		self.objects[controllerID] = [bMesh, bMesh.name]

		return bMesh
