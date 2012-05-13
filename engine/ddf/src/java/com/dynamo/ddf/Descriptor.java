package com.dynamo.ddf;

public class Descriptor 
{
	public Descriptor(String name, FieldDescriptor[] fields)
	{
		m_Name = name;
		m_Fields = fields;
	}
    public String      	  	 m_Name;
    public FieldDescriptor[] m_Fields;    
}
