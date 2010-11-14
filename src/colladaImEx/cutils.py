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

import sys
import os
import Blender
from Blender.Mathutils import *
import string

'''
Translation map.
Used to translate every COLLADA id to a valid id, no matter what "wrong" letters may be
included. Look at the IDREF XSD declaration for more.
Follows strictly the COLLADA XSD declaration which explicitly allows non-english chars,
like special chars (e.g. micro sign), umlauts and so on.
The COLLADA spec also allows additional chars for member access ('.'), these
must obviously be removed too, otherwise they would be heavily misinterpreted.
''' 
translateMap = "" + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(45) + chr(95) + chr(95) + \
	chr(48) + chr(49) + chr(50) + chr(51) + chr(52) + chr(53) + chr(54) + chr(55) + \
	chr(56) + chr(57) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(65) + chr(66) + chr(67) + chr(68) + chr(69) + chr(70) + chr(71) + \
	chr(72) + chr(73) + chr(74) + chr(75) + chr(76) + chr(77) + chr(78) + chr(79) + \
	chr(80) + chr(81) + chr(82) + chr(83) + chr(84) + chr(85) + chr(86) + chr(87) + \
	chr(88) + chr(89) + chr(90) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(97) + chr(98) + chr(99) + chr(100) + chr(101) + chr(102) + chr(103) + \
	chr(104) + chr(105) + chr(106) + chr(107) + chr(108) + chr(109) + chr(110) + chr(111) + \
	chr(112) + chr(113) + chr(114) + chr(115) + chr(116) + chr(117) + chr(118) + chr(119) + \
	chr(120) + chr(121) + chr(122) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(183) + \
	chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + chr(95) + \
	chr(192) + chr(193) + chr(194) + chr(195) + chr(196) + chr(197) + chr(198) + chr(199) + \
	chr(200) + chr(201) + chr(202) + chr(203) + chr(204) + chr(205) + chr(206) + chr(207) + \
	chr(208) + chr(209) + chr(210) + chr(211) + chr(212) + chr(213) + chr(214) + chr(95) + \
	chr(216) + chr(217) + chr(218) + chr(219) + chr(220) + chr(221) + chr(222) + chr(223) + \
	chr(224) + chr(225) + chr(226) + chr(227) + chr(228) + chr(229) + chr(230) + chr(231) + \
	chr(232) + chr(233) + chr(234) + chr(235) + chr(236) + chr(237) + chr(238) + chr(239) + \
	chr(240) + chr(241) + chr(242) + chr(243) + chr(244) + chr(245) + chr(246) + chr(95) + \
	chr(248) + chr(249) + chr(250) + chr(251) + chr(252) + chr(253) + chr(254) + chr(255)


#---Classes---

class Debug(object):

	__debugLevels = ['ALL','DEBUG','FEEDBACK','WARNING','ERROR','NONE']

	# the current debugLevel
	debugLevel = 'ALL'

	# Method: Debug a message
	def Debug(message, level):
		#print 'deb:', level, message #deb--------
		currentLevelIndex = Debug.__debugLevels.index(Debug.debugLevel)
		if str(level).isdigit() and level >= currentLevelIndex:
			print Debug.__debugLevels[level] + ': ' + message
		else:
			try:
				i = Debug.__debugLevels.index(level)
				if i >= currentLevelIndex :
					print level + ': ' + str(message)
			except:
				print 'exception in cutils.py-Debug()'
				pass
	# make Debug a static method
	Debug = staticmethod(Debug)

	# Method: Set the Debug Level
	def SetLevel(level):
		try:
			Debug.__debugLevels.index(level)
			Debug.debugLevel = level
			return True
		except:
			Debug('Debuglevel not available','WARNING')
			return False
	# Make SetLevel a static method
	SetLevel = staticmethod(SetLevel)




#---Functions----

angleToRadian = 3.1415926 / 180.0
radianToAngle = 180.0 / 3.1415926

# Convert a string to a float if the value exists
def ToFloat(val):
	if val is None or val == '':
		return None
	else:
		return float(val)

# Convert a string to a int if the value exists
def ToInt(val):
	if val is None or val == '':
		return None
	else:
		return int(val)

# Convert a string to a list of 3 floats e.g '1.0 2.0 3.0' -> [1.0, 2.0, 3.0]
def ToFloat3(stringValue):
	if stringValue is None:
		return None
	split = stringValue.split( )
	return [ float( split[ 0 ] ), float( split[ 1 ] ), float( split[ 2 ] ) ]

# Convert a string to a list of 2 floats e.g '1.0 2.0' -> [1.0, 2.0]
def ToFloat2(stringValue, errorText=''):
	if stringValue is None:
		return None
	split = stringValue.split( )
	try:
		return [ float( split[ 0 ] ), float( split[ 1 ] )]
	except IndexError:
		print 'Error: ' + errorText
		raise


def CreateRelativePath(basePath, filePath):
	if basePath is None or basePath is '' or filePath is None or filePath is '':
		return ''
	try:
		if not Blender.sys.exists(filePath):
			return ''
	except TypeError:
		return ''

	basePathAr = basePath.lower().split(Blender.sys.sep)
	filePathAr = filePath.lower().split(Blender.sys.sep)
	filePathLength = len(filePathAr)

	if not basePathAr[0] == filePathAr[0]: # Files must have the same root.
		return filePath

	result =  ''
	equalIndex = -1
	for i in range(len(basePathAr)-1):
		if basePathAr[i] == filePathAr[i]:
			pass
		else:
			if equalIndex == -1:
				equalIndex = i
			result += '..'+ Blender.sys.sep
	if equalIndex == -1:
		equalIndex = len(basePathAr)-1
	for i in range(equalIndex, filePathLength):
		result += filePathAr[i]
		if not i == filePathLength-1:
			result += Blender.sys.sep

	return result

def ToList(var):
	result = []
	if var is None:
		return result

	split = var.split( )
	for i in split:
		result.append(i)
	return result
# Convert a string or list to a list of floats
def ToFloatList(var):
	result = []
	if var is None:
		return result

	if type(var) == list:
		for i in var:
			result.append(float(i))
	else:
		split = var.split( )
		for i in split:
			result.append(float(i))
	return result

def ToIntList(lst):
	result = []
	if lst is None:
		return result
	if type(lst) == list:
		for i in lst:
			result.append(int(i))
	else:
		split = lst.split( )
		for i in split:
			result.append(int(i))
	return result

def ToBoolList(lst):
	result = []
	if lst is None:
		return result
	for i in lst:
		result.append(bool(i))
	return result

# Convert a string to a list of 4 floats e.g '1.0 2.0 3.0 4.0' -> [1.0, 2.0, 3.0, 4.0]
def ToFloat4(stringValue):
	split = stringValue.split( )
	return [ float( split[ 0 ] ), float( split[ 1 ] ), float( split[ 2 ] ) , float( split[3])]

def ToFloat7(stringValue):
	data = stringValue.split( )
	return [ float(data[0]), float(data[1]), float(data[2]), float(data[3]), float(data[4]), float(data[5]), float(data[6])]

def AddVec3( vector1, vector2 ):
	vector1.x += vector2.x
	vector1.y += vector2.y
	vector1.z += vector2.z

def ToMatrix4( matrixElement ):
	if matrixElement is None:
		return None
	if not isinstance(matrixElement,list):
		data = matrixElement.split( )

		vec1 = [ float(data[0]), float(data[4]), float(data[8]), float(data[12]) ]
		vec2 = [ float(data[1]), float(data[5]), float(data[9]), float(data[13]) ]
		vec3 = [ float(data[2]), float(data[6]), float(data[10]), float(data[14]) ]
		vec4 = [ float(data[3]), float(data[7]), float(data[11]), float(data[15]) ]
	else:
		vec1 = matrixElement[0:4]
		vec2 = matrixElement[4:8]
		vec3 = matrixElement[8:12]
		vec4 =  matrixElement[12:16]

	return Blender.Mathutils.Matrix( vec1, vec2, vec3, vec4 )

def ToMatrix3(matrixElement):
	data = matrixElement.split( )

	vec1 = [ float(data[0]), float(data[3]), float(data[6]) ]
	vec2 = [ float(data[1]), float(data[4]), float(data[7])]
	vec3 = [ float(data[2]), float(data[5]), float(data[8])]

	return Blender.Mathutils.Matrix( vec1, vec2, vec3)

def GetVector3( element ):
	value = [ float( element[ 0 ] ), float( element[ 1 ] ), float( element[ 2 ] ) ]
	return Blender.Mathutils.Vector( value )

def GetEuler( rotateElement ):
	euler = [ float( rotateElement[ 0 ] ) * float( rotateElement[ 3 ] ) * angleToRadian,
			  float( rotateElement[ 1 ] ) * float( rotateElement[ 3 ] ) * angleToRadian,
			  float( rotateElement[ 2 ] ) * float( rotateElement[ 3 ] ) * angleToRadian ]
	return Blender.Mathutils.Euler( euler )

def AddEuler(euler1, euler2):
	euler1.x += euler2.x
	euler1.y += euler2.y
	euler1.z += euler2.z

# Clear the console
def ClearConsole():
	if sys.platform == 'linux-i386' or sys.platform == 'linux2':
		sysCommand = 'clear'
	elif sys.platform == 'win32' or sys.platform == 'dos' or sys.platform[0:5] == 'ms-dos' :
		sysCommand = 'cls'
	else :
		sysCommand = 'unknown'

	if sysCommand != 'unknown':
		os.system(sysCommand)

def MatrixToString(mat, nDigits):
	result = ''
	if mat is None:
		return result

	for vec in mat:
		result += '\n\t'
		for i in vec:
			result += str(round(i, nDigits))+' '

	return result+'\n'

def MatrixToList(mat):
	result = []
	for vec in mat:
		result.extend(list(vec))
	return result

def RoundList(lst, nDigits):
	result = []
	for i in lst:
		val = round(i, nDigits)
		if val < (1.0 / 10**nDigits):
			val = 0
		result.append(round(i, nDigits))
	return result


def ListToString(lst, nDigits = 5):
	val  = ''
	if lst is None:
		return val
	else:
		for i in lst:
			if type(i) == list:
				val += ListToString(i)+'\n'
			elif isinstance(i, float):
				f = '%.'+str(nDigits)+'f '
				val += f % i
			else:
				val += str(i)+' '
		return val[:-1]

def GetValidFilename(filename):
	filename = Blender.sys.expandpath( filename )
	filename = filename.replace( "//", "/" )
	filename = filename.replace( Blender.sys.sep, "/" )
	return "file://" + filename

def GetColumnVector(matrix, colNumber, rowCount):
	if rowCount == 4:
		return Vector(matrix[0][colNumber], matrix[1][colNumber], matrix[2][colNumber], matrix[3][colNumber])
	else:
		return Vector(matrix[0][colNumber], matrix[1][colNumber], matrix[2][colNumber])

def PrintTransforms(matrix, name):
	print "\n",name, "matrix\n", matrix
	newMat = Matrix(matrix).transpose()
	print name,"loc:   ", newMat.translationPart()
	print name,"euler: ", newMat.toEuler()
	print name,"scale: ", newMat.scalePart()
	
def MakeIDXMLConform(id):
	'''
	Make the name/id COLLADA XML/XSD conform.
	See StripString and translateMap docu for more information.
	'''
	if (len(id) > 0 and id[0] == '#'):
		return '#' + string.translate(id[1:], translateMap)
	else:
		return string.translate(id, translateMap)
		
def AdjustName(adjustedName):
	'''
	Make the name/id COLLADA XML/XSD conform.
	See StripString and translateMap docu for more information.
	'''
	if len(adjustedName) > 0 and not adjustedName[0].isalpha():
		adjustedName = "i"+adjustedName
	return MakeIDXMLConform(adjustedName)

'''
List of error IDs and common strings.
'''
# Expects 1 argument: The mesh name.
# Means that the armature modifier for a mesh couldn't be found,
# probably the parenting is wrong.   
ERROR_MESH_ARMATURE_PARENT = 1

MSG_ERROR_FATAL = "Fatal error. Exit script."

def HandleError(failureId, *args):
	'''
	Prints the error message belonging to the given Id.
	May raise an exception if it's a fatal error.
	'''
	if (failureId == ERROR_MESH_ARMATURE_PARENT):
		print("Can't read armature modifier for mesh %s.\n" \
				"Most likely your armature isn't correctly parenting the mesh.\n" \
				% args[0])
		raise MSG_ERROR_FATAL
	else:
		print("Wrong failure id: %i" % failureId)
