# --------------------------------------------------------------------------
# Illusoft Collada 1.4 plugin for Blender
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2006: Illusoft - colladablender@illusoft.com
# 2008.05.08 modif. for debug mode by migius (AKA Remigiusz Fiedler)
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

debug = False #--- debug mode
debprn = False #--- print debug "print 'deb: ..."
_ERROR = False
_PERROR = False

import sys

try:
	import Blender
except NameError:
	print "Error! Could not find Blender modules!"
	_ERROR = True

__version__ = '0.3.162'

# Show the wait cursor in blender
Blender.Window.WaitCursor(1)

# indicates if the user can choose a file to import
useDefaultFile = True

defaultFileUrl = 'animation.DAE'
#defaultFileUrl = 'animation.DAE'
#defaultFileUrl = 'animated_letters.DAE'
#defaultFileUrl = 'animation_robot.DAE'
defaultExportUrl = ''

# Check if full version of python is installed.
try:
	import os
	import xml
except ImportError:
	print"Error! Could not find full version of Python..."
	_ERROR = True

if _ERROR:
	from sys import version_info
	version = '%s.%s' % version_info[0:2]
	print """
To run the collada importer and exporter you need to have
Python version %s installed in your system. It can be downloaded from:

http://www.python.org

Notes:
- The minor (third) version number doesn't matter, you can have either
Python %s.1 or %s.2 or higher.
- If you do have Python %s installed and still can't run the scripts, then
make sure Blender's Python interpreter is finding the standalone modules
(run 'System Information' from Blender's Help -> System menu).
""" % (version, version, version, version)
	Blender.Draw.PupMenu("Please install full version of python %t | Check the console for more info")
	raise StandardError()
else:

	# Try to load the Plugin modules
	try:
		import cutils
	except NameError:
		print "\nError! Could not find Collada Utils (cutils) module!"
		_ERROR = True

	try:
		import xmlUtils
	except NameError:
		print "\nError! Could not find XML module!"
		_PERROR = True

	try:
		import collada
	except NameError:
		print "Error! Could not find Collada(collada.py) module"
		_PERROR = True

	try:
		import translator
	except NameError:
		print "Error! Could not find Collada Translator (translator.py) module"
		_PERROR = True

	try:
		import helperObjects
	except NameError:
		print "Error! Could not find Collada helperObjects (helperObjects.py) module"
		_PERROR = True

	# Try to load extra python modules
	try:
		import math
	except NameError:
		print "Error! Could not find math module"
		_PERROR = True

	if _PERROR:
		Blender.Draw.PupMenu("Cannot load plugin modules.")
	else:
		# A List with al the modules (in the scriptsdir) to be reloaded
		modules = [cutils, xmlUtils, collada, helperObjects, translator]


def Main(doImp, scriptsLoc):
	global debug, __version__, doImport, scriptsLocation, defaultFilename, valsLoaded
	if _ERROR or _PERROR:
		return
	valsLoaded = False

	doImport = doImp
	if scriptsLoc == "":
		scriptsLoc = Blender.Get('scriptsdir')
		if not scriptsLoc: scriptsLoc = Blender.Get('uscriptsdir')
		if scriptsLoc:
			scriptsLocation = scriptsLoc+Blender.sys.sep+'bpymodules'+Blender.sys.sep+'colladaImEx'+Blender.sys.sep
		else:
			print 'Could not find a scripts path'
	else:
		scriptsLocation = scriptsLoc


	if not ReloadModules():
		print 'cannot reload all modules'
		return False
	# Clear the console
	cutils.ClearConsole()

	# set the debuglevel
	if debug:
		cutils.Debug.SetLevel('DEBUG')
	else:
		cutils.Debug.SetLevel('FEEDBACK')

	cutils.Debug.Debug('Illusoft Collada 1.4 Plugin v%s started'%(__version__),'FEEDBACK')
	# Create a Collada <-> Blender Translator
	if debug:
		print 'keep track of the time to execute this script' #---------
		startTime = Blender.sys.time()

		##fileurl = scriptsDir
		fileurl = ''
		if doImport:
			fileurl+= defaultFileUrl
		else :
			fileurl += defaultExportUrl
		if debprn: print 'deb: fileurl=',fileurl #-------

		useTriangles = False
		usePolygons = False
		bakeMatrices = False
		exportSelection = False
		newScene = True
		clearScene = False
		lookAt = False
		usePhysics = True
		exportCurrentScene = False
		exportRelativePaths = False
		useUV = False
		sampleAnimation = False
		onlyMainScene = False

		transl = translator.Translator(doImport,__version__,debug,fileurl, useTriangles, usePolygons, bakeMatrices, exportSelection, newScene, clearScene, lookAt, usePhysics, exportCurrentScene, exportRelativePaths, useUV, sampleAnimation, onlyMainScene)

		##transl = translator.Translator(doImport,__version__,debug,fileurl)
		##translator = Translator(False,__version__,debug,scriptsDir+defaultExportUrl)
		##translator = Translator(True,__version__,debug,scriptsDir+defaultExportUrl)
		# Redraw al 3D windows.
		if debprn: print 'deb: ---- the end ----' #-----
		Blender.Window.RedrawAll()

		# calculate the elapsed time
		endTime = Blender.sys.time()
		elapsedTime = endTime - startTime
		cutils.Debug.Debug('FINISHED - time elapsed: %.1f'%(elapsedTime),'FEEDBACK')

		# Hide the wait cursor in blender
		Blender.Window.WaitCursor(0)
	else:
		defFilename = Blender.sys.dirname(Blender.sys.progname)+Blender.sys.sep
		colladaReg = Blender.Registry.GetKey('collada',True)

		if not (colladaReg is None) and 'path' in colladaReg and Blender.sys.exists(colladaReg['path']):
			defFilename = colladaReg['path']
		elif not (doImport):
			defFilename += 'untitled.dae'

		defaultFilename = defFilename

		Blender.Draw.Register(Gui, Event, ButtonEvent)	# registering the 3 callbacks

def ReloadModules():
	# Loop through all the modules and try to reload them
	for module in modules:
		try:
			reload(module)
		except NameError:
			cutils.Debug.Debug('cannot reload module %s' %(module),'ERROR')
			return False
	return True

def FileSelected(fileName):
	global doImport, fileButton

	if fileName != '':
		# check if file exists
		if Blender.sys.exists(fileName) != 1 and doImport:
			cutils.Debug.Debug('File(%s) does not exist' % (fileName),'ERROR')
			return False

		# must the file to import end with .dae or .xml?
##		  if doImport:
##			  # Check if the file has a valid extension .DAE or .XML
##			  extension = fileName.rsplit('.',1)[1].lower()
##			  if extension != 'xml' and extension != 'dae':
##				  cutils.Debug.Debug('File(%s) is not a .dae or .xml file' % (fileName),'ERROR')
		fileButton.val = fileName
		##transl = translator.Translator(doImport,__version__,debug,fileName)
	else:
		cutils.Debug.Debug('ERROR: filename is empty','ERROR')

toggle = 0
fileButton = None
toggleTriangles = None
togglePolygons = None
toggleExportSelection = None
toggleClearScene = None
toggleNewScene = None
toggleBakeMatrix = None
toggleSampleAnimation = None
toggleLookAt = None
togglePhysics = None
toggleExportCurrentScene = None
toggleExportRelativePaths = None
toggleUseUV = None
toggleSampleAnimation = None
toggleOnlyMainScene = None
toggleApplyModifiers = None

def LoadDefaultVals():
	global toggleLookAt, toggleBakeMatrix, toggleSampleAnimation, toggleNewScene, \
	toggleClearScene, toggleTriangles, togglePolygons, toggleExportSelection, \
	scriptsLocation, doImport, defaultFilename, fileButton, valsLoaded, \
	togglePhysics, toggleExportCurrentScene, toggleExportRelativePaths, \
	toggleUseUV, toggleOnlyMainScene, toggleApplyModifiers

	if valsLoaded:
		return None

	colladaReg = Blender.Registry.GetKey('collada',True)
	if not (colladaReg is None):
		fileButton.val = colladaReg.get('path', '')
		fileParts = []
		filePath = "";
		fileParts = fileButton.val.split("\\");
		partCount = len(fileParts);
		if partCount > 0 :
			for i in range(partCount):
				if i == 0:
					filePath = fileParts[i];
				else :
					if i != partCount - 1:
						filePath = filePath + "\\" + fileParts[i];
					else:
						filePath = filePath + "\\";

		blenderFilename = Blender.Get('filename');
		fileParts = []
		fileParts = blenderFilename.split("\\");
		partCount = len(fileParts);
		if partCount > 0 :
			blenderFileOnlyName = fileParts[partCount -1];
			blenderFileOnlyName = blenderFileOnlyName.replace(".blend", ".dae");
			filePath = filePath + blenderFileOnlyName;
		else :
			filePath = filePath + "untitled.dae";


		if len(filePath) > 0 :
			fileButton.val = filePath;

		if doImport:
			toggleOnlyMainScene.val = colladaReg.get('onlyMainScene', False)
			toggleNewScene.val = colladaReg.get('newScene', False)
			toggleClearScene.val = colladaReg.get('clearScene', False)
		else:
			##toggleLookAt.val = colladaReg.get('lookAt', False)
			toggleBakeMatrix.val = colladaReg.get('bakeMatrices', False)
			toggleTriangles.val = colladaReg.get('useTriangles', False)
			togglePolygons.val = colladaReg.get('usePolygons', False)
			toggleExportSelection.val = colladaReg.get('exportSelection', False)
			togglePhysics.val = not colladaReg.get('usePhysics', True)
			toggleExportCurrentScene.val = colladaReg.get('exportCurrentScene', False)
			toggleExportRelativePaths.val = colladaReg.get('exportRelativePaths', True)
			toggleSampleAnimation.val = colladaReg.get('sampleAnimation', False)
			toggleUseUV.val = colladaReg.get('useUV', False)
			#TODO: "toggleOnlyMainScene" left out intentionally by the original plugin author?
			toggleApplyModifiers.val = colladaReg.get('applyModifiers', True)
	valsLoaded = True

def Gui():
	global toggleLookAt, toggleBakeMatrix, toggleSampleAnimation, toggleNewScene, \
	toggleClearScene, toggleTriangles, togglePolygons, toggleExportSelection, \
	scriptsLocation, doImport, defaultFilename, fileButton, togglePhysics, \
	toggleExportCurrentScene, toggleExportRelativePaths, toggleUseUV, \
	toggleOnlyMainScene, toggleApplyModifiers
	
	Blender.BGL.glClearColor(0.898,0.910,0.808,1) # Set BG Color1
	Blender.BGL.glClear(Blender.BGL.GL_COLOR_BUFFER_BIT)
	Blender.BGL.glColor3f(0.835,0.848,0.745) # BG Color 2
	size = Blender.Window.GetAreaSize()
	Blender.BGL.glRectd(40,0,200,size[1])
	try:
		logoImage = Blender.Image.Load(scriptsLocation + 'logo.png')
		Blender.BGL.glEnable(Blender.BGL.GL_BLEND ) # Only needed for alpha blending images with background.
		Blender.BGL.glBlendFunc(Blender.BGL.GL_SRC_ALPHA, Blender.BGL.GL_ONE_MINUS_SRC_ALPHA)
		try:		Blender.Draw.Image(logoImage, 45, size[1]-30)
		except:		pass
		Blender.BGL.glDisable(Blender.BGL.GL_BLEND)
	except IOError: # image not found
		Blender.BGL.glColor3i(0.255,0.255,0.2)
		Blender.BGL.glRasterPos2i(45, size[1]-30)
		Blender.Draw.Text("Collada 1.4.0 Plugin for Blender", "large")

	Blender.BGL.glColor3f(0.255,0.255,0.2)
	Blender.BGL.glRasterPos2i(45, size[1]-40)
	Blender.Draw.Text("Version: %s"%(__version__),"small")

	# Write donation text
	donateText1 = "If this plugin is valuable to you or your company, please consider a donation at"
	donateText2 = "http://colladablender.illusoft.com to support this plugin. Thanks a lot!"
	Blender.BGL.glRasterPos2i(45, size[1]-60)
	Blender.Draw.Text(donateText1, "small")
	Blender.BGL.glRasterPos2i(45, size[1]-70)
	Blender.Draw.Text(donateText2, "small")

	# Write import / export text
	Blender.BGL.glColor3f(0.9,0.08,0.08)
	Blender.BGL.glRasterPos2i(45, size[1]-95)
	if doImport:
		importExportText = "Import"
	else:
		importExportText = "Export"
	Blender.Draw.Text(importExportText, "normal")

	Blender.BGL.glColor3f(0.255,0.255,0.2)

	# Create File path input
	yval = size[1]-130
	Blender.BGL.glRasterPos2i(45, yval)
	if fileButton is None or fileButton.val == '':
		fileName = defaultFilename
	else:
		fileName = fileButton.val
	Blender.Draw.Text('%s file:'%(importExportText),"normal")
	maxWidth = 400
	if size[0] - (105 + 35) > maxWidth:
		fileWidth = maxWidth
	else:
		fileWidth = size[0] - (105 + 35)
	fileButton = Blender.Draw.String('', 5, 105, yval-5, fileWidth, 20, fileName, 255)
	Blender.Draw.PushButton('...', 2, 105 + fileWidth, yval-5, 30, 20, 'browse file')

	Blender.Draw.PushButton("Cancel", 3, 45, 10, 55, 20, "Cancel")
	Blender.Draw.PushButton(importExportText + ' and Close', 4, 45+55+35, 10, 100, 20, importExportText + ' and close this screen')

	# Create Export Options:
	if not doImport:
		yval = yval - 50
		# Create Triangle / Polygons Options
		if not (toggleTriangles is None):
			toggleTrianglesVal = toggleTriangles.val
		else:
			toggleTrianglesVal = 0

		if not (togglePolygons is None):
			togglePolygonsVal = togglePolygons.val
		else:
			togglePolygonsVal = 0

		toggleTriangles = Blender.Draw.Toggle('Triangles',6,45, yval, 60, 20, toggleTrianglesVal, 'Export all geometry as triangles')
		togglePolygons = Blender.Draw.Toggle('Polygons',7,45+60 + 30, yval, 60, 20, togglePolygonsVal, 'Export all geometry as polygons')

		yval = yval - 40
		# Create Export Selection Option
		if not (toggleExportSelection is None):
			toggleExportSelectionVal = toggleExportSelection.val
		else:
			toggleExportSelectionVal = 0

		toggleExportSelection = Blender.Draw.Toggle('Only Export Selection',8,45, yval, 150, 20, toggleExportSelectionVal, 'Only export selected objects')

		yval = yval - 40
		# Create Bake Matrix Option
		if not (toggleBakeMatrix is None):
			toggleBakeMatrixVal = toggleBakeMatrix.val
		else:
			toggleBakeMatrixVal = 0

		toggleBakeMatrix = Blender.Draw.Toggle('Bake Matrices',11,45, yval, 150, 20, toggleBakeMatrixVal, 'Put all transformations in a single matrix')

		yval = yval - 40
		# Create Sample Anim
		if not (toggleSampleAnimation is None):
			toggleSampleAnimationVal = toggleSampleAnimation.val
		else:
			toggleSampleAnimationVal = 0

		toggleSampleAnimation = Blender.Draw.Toggle('Sample Animation',16,45, yval, 150, 20, toggleSampleAnimationVal, 'Export information for every frame of animation.')


		yval = yval - 40
		#Create Physics Option
		if not (togglePhysics is None):
			togglePhysicsVal = togglePhysics.val
		else:
			togglePhysicsVal = 0

		togglePhysics = Blender.Draw.Toggle('Disable Physics',13,45, yval, 150, 20, togglePhysicsVal, 'Disable Export physics information')


		yval = yval - 40
		#Create Physics Option
		if not (toggleExportCurrentScene is None):
			toggleExportCurrentSceneVal = toggleExportCurrentScene.val
		else:
			toggleExportCurrentSceneVal = 0

		toggleExportCurrentScene = Blender.Draw.Toggle('Only Current Scene',14,45, yval, 150, 20, toggleExportCurrentSceneVal, 'Only Export the current scene')

		yval = yval - 40
		#Create Relative Path's Option
		if not (toggleExportRelativePaths is None):
			toggleExportRelativePathsVal = toggleExportRelativePaths.val
		else:
			toggleExportRelativePathsVal = 0

		toggleExportRelativePaths = Blender.Draw.Toggle('Use Relative Paths',15,45, yval, 150, 20, toggleExportRelativePathsVal, 'Export paths relative to the collada file')

		yval = yval - 40
		#Create Relative Path's Option
		if not (toggleUseUV is None):
			toggleUseUVVal = toggleUseUV.val
		else:
			toggleUseUVVal = 0

		toggleUseUV = Blender.Draw.Toggle('Use UV Image Mats',15,45, yval, 150, 20, toggleUseUVVal, 'Use UV Image instead of the material textures. Use this if you did not use the Material Textures window. Note: If you reimport this file, they will have moved to the materials section!!')

		##yval = yval - 40
		# Create Lookat  Option
		if not (toggleLookAt is None):
			toggleLookAtVal = toggleLookAt.val
		else:
			toggleLookAtVal = 0

		##toggleLookAt = Blender.Draw.Toggle('Camera as Lookat',14,45, yval, 150, 20, toggleLookAtVal, 'Export the transformation of camera\'s as lookat')
		
		yval = yval - 40
		if not (toggleApplyModifiers is None):
			toggleApplyModifiersVal = toggleApplyModifiers.val
		else:
			toggleApplyModifiersVal = 0
		
		toggleApplyModifiers = Blender.Draw.Toggle('Apply modifiers',14,45, yval, 150, 20, toggleApplyModifiersVal, 'Apply modifiers, like mirroring, transformations, etc.')

		Blender.Draw.PushButton(importExportText, 12, 45+55+35+100+35, 10, 55, 20, importExportText)
	else: # IMPORT GUI
		yval = yval - 50
		# Create Import To new Scene Options
		if not (toggleNewScene is None):
			toggleNewSceneVal = toggleNewScene.val
		else:
			toggleNewSceneVal = 0

		if not (toggleClearScene is None):
			toggleClearSceneVal = toggleClearScene.val
		else:
			toggleClearSceneVal = 0

		if not (toggleOnlyMainScene is None):
			toggleOnlyMainSceneVal = toggleOnlyMainScene.val
		else:
			toggleOnlyMainSceneVal = 0

		if toggleOnlyMainSceneVal == 0:
			if toggleClearSceneVal == 0 and toggleNewSceneVal == 0:
				toggleNewSceneVal = 1

		newSceneText = 'Import file into a new Scene';
		newSceneTitle = 'New Scene'
		clearSceneText = 'Clear everything on the current scene'
		clearSceneTitle = 'Clear Scene'
		if toggleOnlyMainSceneVal == 0:
			newSceneText = 'Import file into a new Scenes'
			newSceneTitle = 'New Scenes'
			clearSceneText = 'Delete all the Blender Scenes'
			clearSceneTitle = 'Delete Scenes'
		toggleOnlyMainScene = Blender.Draw.Toggle('Only Import Main Scene',17,40, yval, 190, 20, toggleOnlyMainSceneVal, 'Only import the main scene from Collada')
		yval = yval - 40
		toggleNewScene = Blender.Draw.Toggle(newSceneTitle,9,40, yval, 90, 20, toggleNewSceneVal, newSceneText)
		toggleClearScene = Blender.Draw.Toggle(clearSceneTitle,10,40+90 + 10, yval, 90, 20, toggleClearSceneVal, clearSceneText)

	LoadDefaultVals()

def CalcElapsedTime(startTime):
	'''
	Calc elapsed time between now and start time.
	'''	
	return Blender.sys.time() - startTime	

def Event(evt, val):
	pass

def ButtonEvent(evt):
	global toggleLookAt, toggleBakeMatrix, toggleExportSelection,toggleNewScene, \
	toggleClearScene, toggleTriangles, togglePolygons, doImport, defaultFilename, \
	fileSelectorShown, fileButton, valsLoaded, togglePhysics, \
	toggleExportCurrentScene, toggleExportRelativePaths, toggleUseUV, \
	toggleSampleAnimation, toggleOnlyMainScene
	
	checkImportButtons = False
	if evt == 1:
		toggle = 1 - toggle
		Blender.Draw.Redraw(1)
	elif evt == 2: # browse file
		browseText = ''
		if doImport:
			browseText = 'Import .dae'
		else:
			browseText = 'Export .dae'
		Blender.Window.FileSelector(FileSelected,browseText,defaultFilename)
		Blender.Draw.Redraw(1)
	elif evt == 3:
		Blender.Draw.Exit()
	elif evt == 4 or evt == 12: # Ok, time to export/import
		#keep track of the time to execute this script
		startTime = Blender.sys.time()
		fileName = fileButton.val
		dirName = Blender.sys.dirname(fileName) + Blender.sys.sep
		exists = Blender.sys.exists(fileName)
		if exists == 1 and not doImport:
			overwrite = Blender.Draw.PupMenu( "File Already Exists, Overwrite?%t|Yes%x1|No%x0" )
			if not overwrite == 1:
				return False
		elif exists != 1 and doImport:
			Blender.Draw.PupMenu("File does not exist: %t|"+fileName)
			cutils.Debug.Debug('File(%s) does not exist' % (fileName),'ERROR')
			return False
		elif not Blender.sys.exists(dirName):
			Blender.Draw.PupMenu("Path is not valid: %t|"+dirName)
			cutils.Debug.Debug('Path is not valid: %s' % (dirName),'ERROR')
			return False

		if toggleTriangles is None:
			useTriangles = False
		else:
			useTriangles = bool(toggleTriangles.val)

		if togglePolygons is None:
			usePolygons = False
		else:
			usePolygons = bool(togglePolygons.val)

		if toggleBakeMatrix is None:
			bakeMatrices = False
		else:
			bakeMatrices = bool(toggleBakeMatrix.val)

		if toggleExportSelection is None:
			exportSelection = False
		else:
			exportSelection = bool(toggleExportSelection.val)

		if toggleNewScene is None:
			newScene = False
		else:
			newScene = bool(toggleNewScene.val)

		if toggleClearScene is None:
			clearScene = False
		else:
			clearScene = bool(toggleClearScene.val)

		if toggleOnlyMainScene is None:
			onlyMainScene = False
		else:
			onlyMainScene = bool(toggleOnlyMainScene.val)

		if toggleLookAt is None:
			lookAt = False
		else:
			lookAt = bool(toggleLookAt.val)

		if togglePhysics is None:
			usePhysics = True
		else:
			usePhysics = not bool(togglePhysics.val)

		if toggleExportCurrentScene is None:
			exportCurrentScene = False
		else:
			exportCurrentScene = bool(toggleExportCurrentScene.val)

		if toggleExportRelativePaths is None:
			exportRelativePaths = False
		else:
			exportRelativePaths = bool(toggleExportRelativePaths.val)

		if toggleUseUV is None:
			useUV = False
		else:
			useUV = bool(toggleUseUV.val)

		if toggleSampleAnimation is None:
			sampleAnimation = False
		else:
			sampleAnimation = bool(toggleSampleAnimation.val)
			
		if toggleApplyModifiers is None:
			applyModifiers = False
		else:
			applyModifiers = bool(toggleApplyModifiers.val)


		d = Blender.Registry.GetKey('collada',True)
		if d is None:
			d = dict()
		d['path'] = fileName

		if doImport:
			d['newScene'] = newScene
			d['clearScene'] = clearScene
			d['onlyMainScene'] = onlyMainScene
		else:
			d['useTriangles'] = useTriangles
			d['usePolygons'] = usePolygons
			d['bakeMatrices'] = bakeMatrices
			d['exportSelection'] = exportSelection
			d['lookAt'] = lookAt
			d['usePhysics'] = usePhysics
			d['exportCurrentScene'] = exportCurrentScene
			d['exportRelativePaths'] = exportRelativePaths
			d['useUV'] = useUV
			d['sampleAnimation'] = sampleAnimation
			d['applyModifiers'] = applyModifiers

		Blender.Registry.SetKey('collada',d, True)

		if doImport:
			importExportText = "Import"
		else:
			importExportText = "Export"

		try:
			transl = translator.Translator(doImport,__version__,debug,fileName, \
										useTriangles, usePolygons, bakeMatrices,\
										exportSelection, newScene, clearScene,  \
										lookAt, usePhysics, exportCurrentScene, \
										exportRelativePaths, useUV, sampleAnimation, \
										onlyMainScene, applyModifiers)
			
			cutils.Debug.Debug('Time to process and save data: %.1f' \
							% CalcElapsedTime(startTime), 'FEEDBACK')
			
			# Redraw all 3D windows.
			Blender.Window.RedrawAll()
			
			Blender.Draw.PupMenu(importExportText + " Successful %t")
		except:						
			Blender.Draw.PupMenu(importExportText + "ing failed%t | Check the console for more info")
			raise # throw the exception

		cutils.Debug.Debug('FINISHED - time elapsed: %.1f' % CalcElapsedTime(startTime), \
						'FEEDBACK')

		# Hide the wait cursor in blender
		Blender.Window.WaitCursor(0)
		if evt == 4:
			Blender.Draw.Exit()
		valsLoaded = False
	elif evt == 6: # Toggle Triangles
		if toggleTriangles.val:
			togglePolygons.val = 0
		Blender.Draw.Redraw(1)
	elif evt == 7: # Toggle Polygons
		if togglePolygons.val:
			toggleTriangles.val = 0
		Blender.Draw.Redraw(1)
	elif evt == 9: # Toggle Create new Scene
		if toggleNewScene.val:
			toggleClearScene.val = 0
		checkImportButtons = True
	elif evt == 10: # Toggle Clear current Scene
		if toggleClearScene.val:
			toggleNewScene.val = 0
		checkImportButtons = True
	elif evt == 17: # Toggle Only Main Scene
		checkImportButtons = True


	if checkImportButtons:
		if not toggleOnlyMainScene.val:
			if not toggleClearScene.val and not toggleNewScene.val:
				toggleNewScene.val = True
		Blender.Draw.Redraw(1)
