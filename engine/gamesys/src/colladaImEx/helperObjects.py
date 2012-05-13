# --------------------------------------------------------------------------
# Illusoft Collada 1.4 plugin for Blender
# --------------------------------------------------------------------------
# ***** BEGIN GPL LICENSE BLOCK *****
#
# Copyright (C) 2006: Illusoft - colladablender@illusoft.com
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

import Blender
import collada
from Blender.Mathutils import *

debprn = 0 #False #--- print debug "print 'deb: ..."

class Armature(object):
	# static vars
	# A list of all created armatures
	_armatures = dict()

	def __init__(self, armatureBObject, daeNode):
		self.armatureBObject = armatureBObject
		self.blenderArmature = Blender.Armature.New()
		self.armatureBObject.link(self.blenderArmature)
		print self.armatureBObject
		self.boneInfos = dict()
		self.rootBoneInfos = dict()
		# The real blender name of this armature
		self.realName = None
		self.deaNode = daeNode

	def GetBlenderObject(self):
		return self.armatureBObject

	def GetBlenderArmature(self):
		return self.blenderArmature

	def HasBone(self, boneName):
		return boneName in self.boneInfos

	def AddNewBone(self, boneName, parentBoneName, daeNode):
		# Create a new Editbone.
		editBone = Blender.Armature.Editbone()
		# Add the bone to the armature
		self.blenderArmature.bones[boneName] = editBone
		# Get the boneInfo for the parent of this bone. (if it exists)
		parentBoneInfo = None
		if not parentBoneName is None and parentBoneName in self.boneInfos:
			parentBoneInfo = self.boneInfos[parentBoneName]

		# Create a new boneInfo object
		boneInfo = BoneInfo(boneName, parentBoneInfo, self, daeNode)
		# Store the boneInfo object in the boneInfos collection of this armature.
		self.boneInfos[boneName] = boneInfo

		# If this bone has a parent, set it.
		if not parentBoneName is None and parentBoneName in self.boneInfos:
			parentBoneInfo = self.boneInfos[parentBoneName]
			parentBoneInfo.childs[boneName] = boneInfo
			editBone.parent = self.GetBone(parentBoneName)
			##boneInfo.SetConnected()
		else:
			self.rootBoneInfos[boneName] = boneInfo

		return boneInfo

	def MakeEditable(self,makeEditable):
		if makeEditable:
			self.GetBlenderArmature().makeEditable()
		else:
			self.GetBlenderArmature().update()

	def GetBone(self, boneName):
		if boneName is None or not (boneName in self.blenderArmature.bones.keys()):
			return None
		else:
			return self.blenderArmature.bones[boneName]

	# Get the location of the armature (VECTOR)
	def GetLocation(self):
		return Vector(self.armatureBObject.loc).resize4D()

	def GetTransformation(self):
		return self.armatureBObject.matrix

	def GetBoneInfo(self, boneName):
		if boneName is None:
			return None
		else:
			return self.boneInfos[boneName]

	def GetBoneInfoFromJoint(self, jointName):
		for boneInfo in self.boneInfos:
			if boneInfo.jointName == jointName:
				return boneInfo
		return None

	def GetJointList(self):
		result = dict()
		for boneInfo in self.boneInfos.values():
			result[boneInfo.GetJointName()] = boneInfo
		return result

	#---CLASSMETHODS

	# Factory method
	def CreateArmature(cls,objectName,armatureName, realArmatureName, daeNode):
		armatureBObject = armature_obj = Blender.Object.New ('Armature', objectName)
		armatureBObject.name = str(realArmatureName)
		armature = Armature(armatureBObject, daeNode)
		armature.name = armatureName
		cls._armatures[armatureName] = armature

		return armature
	CreateArmature = classmethod(CreateArmature)

	def GetArmature(cls, armatureName):
		return cls._armatures.setdefault(armatureName)
	GetArmature = classmethod(GetArmature)

	def FindArmatureWithJoint(cls, jointName):
		for armature in cls._armatures.values():
			jointList = armature.GetJointList()
			if jointName in jointList:
				return armature
		return None
	FindArmatureWithJoint = classmethod(FindArmatureWithJoint)

class BoneInfo(object):

	def __init__(self, boneName, parentBoneInfo, armature, daeNode):
		if debprn: print 'deb:class BoneInfo_INITIALIZE............' #--------
		if debprn: print 'deb:       boneName=', boneName #--------
		if debprn: print 'deb: parentBoneInfo=', #--------
		if parentBoneInfo: print parentBoneInfo.name #, parentBoneInfo #--------
		else: print parentBoneInfo #--------
		#if debprn: print 'deb: armature=', #--------
		#if armature: print armature.name #, armature #--------
		#else:	print armature, #--------

		self.name = boneName
		self.parent = parentBoneInfo
		self.armature = armature
		self.childs = dict()
		self.daeNode = daeNode

		self.headTransformMatrix = None
		self.tailTransformMatrix = None

		self.localTransformMatrix = Matrix()
		self.worldTransformMatrix = Matrix()

	def GetBone(self):
		return self.armature.GetBone(self.name)

	def SetTail(self, tailLocVector):
		if len(tailLocVector) == 4:
			tailLocVector.resize3D()
		self.GetBone().tail = tailLocVector

	def GetTail(self):
		return self.GetBone().tail

	def SetHead(self, headLocVector):
		if len(headLocVector) == 4:
			headLocVector.resize3D()
		self.GetBone().head = headLocVector

	def GetHead(self):
		return self.GetBone().head

	def SetConnected(self):
		self.GetBone().options = Blender.Armature.CONNECTED

	def IsEnd(self):
		return len(self.childs) == 0

	def IsRoot(self):
		return self.parent is None

	def GetTailName(self):
		return self.daeNode.name

	def GetJointName(self):
		return self.name
##		if not self.parent is None:
##			return self.parent.name
##		else:
##			return self.armature.name

class AnimationInfo(object):
	_animations = dict()

	def __init__(self, nodeId):
		self.nodeId = nodeId
		self.times = dict()

	def GetTypes(self, daeNode):
		types = []
		if len(self.times) > 0:
			for target in self.times.values()[0]:
				ta = self.GetType(daeNode, target)
				if ta[0] == collada.DaeSyntax.TRANSLATE and not Blender.Object.Pose.LOC in types:
					types.append(Blender.Object.Pose.LOC)
				elif ta[0] == collada.DaeSyntax.ROTATE and not Blender.Object.Pose.ROT in types:
					types.append(Blender.Object.Pose.ROT)
				#TODO: check if scale correct implemented
				elif ta[0] == collada.DaeSyntax.SCALE and not Blender.Object.Pose.SCALE in types:
					types.append(Blender.Object.Pose.SCALE)
		return types

	def GetType(self, daeNode, target):
		ta = target.split('.', 1)
		for t in daeNode.transforms:
			if t[2] == ta[0]:
				return [t[0], ta]


	def CreateAnimations(cls, animationsLibrary, fps, axiss):
		for daeAnimation in animationsLibrary.daeLibrary.items:
			for channel in daeAnimation.channels:
				# Get the id of the node
				targetArray = channel.target.split("/", 1)
				nodeId = targetArray[0]
				targetId = targetArray[1]
				# Get the animationInfo object for this node (or create a new one)
				animation = cls._animations.setdefault(nodeId, AnimationInfo(nodeId))
				#if debprn: print 'deb:helperObj.py:class AnimationInfo CreateAnimations() dir(animation)',  dir(animation) #----------

				# loop trough all samplers
				sampler = None
				if debprn: print 'deb:helperObj.py:class AnimationInfo CreateAnimations() \ndeb: channel.source= ',  channel.source #----------
				for s in daeAnimation.samplers:
					#if debprn: print 'deb: sampler.id        = ', s.id #----------
					#if debprn: print 'deb: channel.source[1:]= ', channel.source[1:] #----------
#org					if s.id == channel.source[1:]:
					if s.id == channel.source:
						sampler = s

				# Get the values for all the inputs
				if not sampler is None:
					input = sampler.GetInput("INPUT")
					inputSource = daeAnimation.GetSource(input.source)
					if inputSource.techniqueCommon.accessor.HasParam("TIME") and len(inputSource.techniqueCommon.accessor.params) == 1:
						if debprn: print 'deb: DDDDD getting target' #----------
						output = sampler.GetInput("OUTPUT")
						outputSource = daeAnimation.GetSource(output.source)
						outputAccessor = outputSource.techniqueCommon.accessor
						accessorCount = outputAccessor.count
						accessorStride = outputAccessor.stride
						interpolations = sampler.GetInput("INTERPOLATION")
						interpolationsSource = daeAnimation.GetSource(interpolations.source)
						if 0: #because probably interpolationsAccessor is identical to outputAccessor
							interpolationsAccessor = interpolationsSource.techniqueCommon.accessor
							accessorCount = interpolationsAccessor.count
							accessorStride = interpolationsAccessor.stride

						if debprn: print 'deb: outputSource.source.data: ',  outputSource.source.data #----------
						#if debprn: print 'deb: dir(outputAccessor.params): ',  dir(outputAccessor.params) #----------
						#if debprn: print 'deb: dir(outputAccessor.params[0]): ',  str(outputAccessor.params[0]) #----------
						times = [x*fps for x in inputSource.source.data]
						if debprn: print 'deb: times=', times #---------
						for timeIndex in range(len(times)):
							time = animation.times.setdefault(times[timeIndex], dict())
							target = time.setdefault(targetId, dict())
							#interp = time.setdefault(targetId, dict())
							#if debprn: print 'deb: accessorStride=', accessorStride #---------
							value = []
							for j in range(accessorStride):
								#if debprn: print 'deb: timeIndex,j,data=',timeIndex, j, outputSource.source.data[timeIndex*accessorStride + j] #---------
								#if debprn: print 'deb: outputAccessor.params[j]=',outputAccessor.params[j] #---------
								#target[outputAccessor.params[j]] = outputSource.source.data[timeIndex*accessorStride + j]
								value.append(outputSource.source.data[timeIndex*accessorStride + j])
							if debprn: print 'deb: value=', value #---------
							target[outputAccessor.params[0]] = value
							#interp[outputAccessor.params[j]] = interpolationsSource.source.data[timeIndex*accessorStride + j]
							if debprn: print 'deb: time=', time #---------
							if debprn: print 'deb: target=', target #---------
					#if debprn: print 'deb:helperObj.py: X X X X X X X X X class AnimationInfo CreateAnimations() animation=',  animation #----------

	CreateAnimations = classmethod(CreateAnimations)



	def GetAnimationInfo(cls, nodeId):
		for animation in cls._animations.values():
			if animation.nodeId == nodeId:
				return animation
		return None
	GetAnimationInfo = classmethod(GetAnimationInfo)
