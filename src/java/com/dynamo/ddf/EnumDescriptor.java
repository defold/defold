package com.dynamo.ddf;

public class EnumDescriptor 
{
	public EnumDescriptor(String name, EnumValueDescriptor[] enum_values)
	{
		m_Name = name;
		m_EnumValues = enum_values;
	}
	
	public String 			     m_Name;
	public EnumValueDescriptor[] m_EnumValues;
}
