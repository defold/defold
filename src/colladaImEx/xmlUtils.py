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

from datetime import *
from cutils import *
from xml.dom.minidom import *

debprn = False #--- print debug "print 'deb: ..."

#---XML Utils---

# Returns the first child of the specified type in node
def FindElementByTagName(parentNode, type):
	child = parentNode.firstChild
	while child != None:
		if child.localName == type:
			return child
		child = child.nextSibling
##	  childs = parentNode.getElementsByTagName(type)
##	  if len(childs) > 0:
##		  return childs[0]
	return None

def FindElementsByTagName(parentNode, type):
	result = []
	child = parentNode.firstChild
	while child != None:
		if child.localName == type:
			result.append(child)
		child = child.nextSibling
	return result

def ReadAttribute(node,attributeName):
	if node != None and attributeName != None:
		attribute = node.getAttribute(attributeName)
		return attribute
	return None

def ReadContents(node):
	if node != None:
		child = node.firstChild
		if child != None and child.nodeType == child.TEXT_NODE:
			return child.nodeValue
	return None

def ReadDateTime(node):
	if node == None:
		return None
	return GetDateTime(ReadContents(node))

def RemoveWhiteSpace(parent):
	for child in list(parent.childNodes):
		if child.nodeType==child.TEXT_NODE and child.data.strip()=='':
			parent.removeChild(child)
		else:
			RemoveWhiteSpace(child)

def RemoveWhiteSpaceNode(parent):
	for child in list(parent.childNodes):
		if child.nodeType == child.TEXT_NODE and child.data.strip()=='':
			parent.removeChild(child)
	return parent

def RemoveComments(parent):
	for child in list(parent.childNodes):
		if child.__class__.__name__ == "Comment":
			parent.removeChild(child)
	return parent

##def RemoveWhiteSpace(node):
##	  removeList = []
##	  for child in node.childNodes:
##		  if child.nodeType == child.TEXT_NODE and not child.data.strip():
##			  removeList.append(child)
##		  elif child.hasChildNodes():
##			  RemoveWhiteSpace(child)
##
##	  for node in removeList:
##		  node.parentNode.removeChild(node)

def GetDateTime(xmlvalue):
	vals = xmlvalue.split('T')
	datestr = vals[0]
	timestr =  vals[1]
	date = datestr.split('-')
	time = timestr.split(':')
	time[2]=time[2].rstrip('Z')
	return datetime(int(date[0]), int(date[1]), int(date[2]),int(time[0]), int(time[1]), int(float(time[2])))

def ToDateTime(val):
	return '%s-%s-%sT%s:%s:%sZ'%(val.year,str(val.month).zfill(2),str(val.day).zfill(2), str(val.hour).zfill(2), str(val.minute).zfill(2),str(val.second).zfill(2))

def GetStringArrayFromNodes(xmlNodes):
	vals = []
	if xmlNodes == None:
		return vals
	for xmlNode in xmlNodes:
		stringvals = ReadContents(xmlNode).split( )
		for string in stringvals:
			vals.append(string)
	return vals

def GetListFromNodes(xmlNodes, cast=None):
	result = []
	if xmlNodes is None:
		return result

	for xmlNode in xmlNodes:
		val = ReadContents(xmlNode).split( )
		if cast == float:
			val = ToFloatList(val)
		elif cast == int:
			val = ToIntList(val)
		elif cast == bool:
			val = ToBoolList(val)
		result.append(val)
	return result


def ToXml(xmlNode, indent='\t', newl='\n'):
	return '<?xml version="1.0" encoding="utf-8"?>\n%s'%(__ToXml(xmlNode, indent,newl))

def __ToXml(xmlNode, indent='\t',newl='\n',totalIndent=''):
	childs = xmlNode.childNodes
	if len(childs) > 0:
		attrs = ''
		attributes = xmlNode.attributes
		if attributes != None:
			for attr in attributes.keys():
				val = attributes[attr].nodeValue
				attrs += ' %s="%s"'%(attr,val)
		result = '%s<%s%s>'%(totalIndent,xmlNode.localName,attrs)
		tempnewl = newl
		tempTotIndent = totalIndent
		for child in childs:
			if child.nodeType == child.TEXT_NODE:
				tempnewl = ''
				tempTotIndent = ''

			result += '%s%s'%(tempnewl,__ToXml(child, indent, newl, totalIndent+indent))
		result += '%s%s</%s>'%(tempnewl,tempTotIndent,xmlNode.localName)
		return result
	else:
		if xmlNode.nodeType == xmlNode.TEXT_NODE:
			return xmlNode.toxml().replace('\n','\n'+totalIndent[:-1])
		else:
			return totalIndent+xmlNode.toxml()

def AppendChilds(xmlNode, syntax, lst):
	if lst is None or syntax is None or xmlNode is None:
		return

	for i in lst:
		el = Element(syntax)
		text = Text()
		text.data = ListToString(i)
		el.appendChild(text)
		xmlNode.appendChild(el)

	return xmlNode


