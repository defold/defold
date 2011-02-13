package com.dynamo.ddf;

public class FieldDescriptor 
{
	public FieldDescriptor(String name, int number, int type, int label, Descriptor message_desc, EnumDescriptor enum_desc)
	{
		m_Name = name;
		m_Number = number;
		m_Type = type;
		m_Label = label;
		m_MessageDescriptor = message_desc;
		m_EnumDescriptor = enum_desc;
	}
	
	public String 	      m_Name;
	public int    	      m_Number;
	public int    	      m_Type;
	public int		      m_Label;
	public Descriptor  	  m_MessageDescriptor;
	public EnumDescriptor m_EnumDescriptor;
}
