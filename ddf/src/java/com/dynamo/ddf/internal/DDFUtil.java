package com.dynamo.ddf.internal;

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;

import com.dynamo.ddf.Descriptor;
import com.dynamo.ddf.EnumDescriptor;
import com.dynamo.ddf.EnumValueDescriptor;
import com.dynamo.ddf.FieldDescriptor;
import com.dynamo.ddf.annotations.ProtoClassName;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.DescriptorProtos.EnumDescriptorProto;
import com.google.protobuf.DescriptorProtos.FieldDescriptorProto.Label;
import com.google.protobuf.DescriptorProtos.FieldDescriptorProto.Type;
import com.google.protobuf.DescriptorProtos.FileDescriptorProto;
import com.google.protobuf.DescriptorProtos.FileDescriptorProto.Builder;
import com.google.protobuf.Descriptors;
import com.google.protobuf.Descriptors.DescriptorValidationException;

public class DDFUtil
{

	static void buildEnumProto(EnumDescriptor ddf_descriptor, Map<String, DescriptorProtos.EnumDescriptorProto> protos)
	{
		if (protos.containsKey(ddf_descriptor.m_Name))
			return;

    	DescriptorProtos.EnumDescriptorProto.Builder builder = DescriptorProtos.EnumDescriptorProto.newBuilder();
    	builder.setName(ddf_descriptor.m_Name);
    	protos.put(ddf_descriptor.m_Name, null);

    	EnumValueDescriptor[] fields = ddf_descriptor.m_EnumValues;
    	for (EnumValueDescriptor fd : fields)
    	{
    		DescriptorProtos.EnumValueDescriptorProto.Builder enum_bld = DescriptorProtos.EnumValueDescriptorProto.newBuilder();
    		enum_bld.setName(fd.m_Name);
    		enum_bld.setNumber(fd.m_Number);

    		DescriptorProtos.EnumValueDescriptorProto value_proto = enum_bld.build();
    		builder.addValue(value_proto);
		}
    	protos.put(ddf_descriptor.m_Name, builder.build());
	}

	static void buildDescriptorProto(Descriptor ddf_descriptor,
			Map<String, DescriptorProtos.DescriptorProto> protos,
			Map<String, DescriptorProtos.EnumDescriptorProto> enum_protos)
	{
		if (protos.containsKey(ddf_descriptor.m_Name))
			return;

    	DescriptorProtos.DescriptorProto.Builder builder = DescriptorProtos.DescriptorProto.newBuilder();
    	builder.setName(ddf_descriptor.m_Name);
    	protos.put(ddf_descriptor.m_Name, null);

    	FieldDescriptor[] fields = ddf_descriptor.m_Fields;
    	for (FieldDescriptor fd : fields)
    	{
    		DescriptorProtos.FieldDescriptorProto.Builder fld_bld = DescriptorProtos.FieldDescriptorProto.newBuilder();
    		fld_bld.setName(fd.m_Name).setNumber(fd.m_Number).setLabel(Label.valueOf(fd.m_Label)).setType(Type.valueOf(fd.m_Type));

    		if (fd.m_Type == Type.TYPE_MESSAGE.getNumber())
    		{
    			fld_bld.setTypeName(fd.m_MessageDescriptor.m_Name);
    			buildDescriptorProto(fd.m_MessageDescriptor, protos, enum_protos);
    		}
    		else if (fd.m_Type == Type.TYPE_ENUM.getNumber())
    		{
    			fld_bld.setTypeName(fd.m_EnumDescriptor.m_Name);
    			buildEnumProto(fd.m_EnumDescriptor, enum_protos);
    		}

    		DescriptorProtos.FieldDescriptorProto fld_proto = fld_bld.build();
    		builder.addField(fld_proto);
		}
    	protos.put(ddf_descriptor.m_Name, builder.build());
	}

	static Descriptors.Descriptor buildDescriptor(Descriptor ddf_descriptor) throws DescriptorValidationException
	{
    	Map<String, DescriptorProtos.DescriptorProto> protos = new HashMap<String, DescriptorProtos.DescriptorProto>();
    	Map<String, DescriptorProtos.EnumDescriptorProto> enum_protos = new HashMap<String, DescriptorProtos.EnumDescriptorProto>();

    	buildDescriptorProto(ddf_descriptor, protos, enum_protos);

        Builder fileDescP = DescriptorProtos.FileDescriptorProto.newBuilder();
    	for (DescriptorProtos.DescriptorProto dp : protos.values())
    	{
			fileDescP.addMessageType(dp);
		}

    	for (EnumDescriptorProto ep : enum_protos.values())
    	{
			fileDescP.addEnumType(ep);
		}

    	FileDescriptorProto file_desc = fileDescP.build();

        Descriptors.FileDescriptor[] fileDescs = new Descriptors.FileDescriptor[0];
        Descriptors.FileDescriptor dynamicDescriptor = Descriptors.FileDescriptor.buildFrom(file_desc, fileDescs);

        return dynamicDescriptor.findMessageTypeByName(ddf_descriptor.m_Name);
	}

    public static <T> Descriptors.Descriptor getDescriptor(Class<T> ddf_class)
    {
        try
        {
        	Field desc_field = ddf_class.getDeclaredField("DESCRIPTOR");
        	Descriptor ddf_descriptor = (Descriptor) desc_field.get(null);
        	return buildDescriptor(ddf_descriptor);
        }
        catch (NoSuchFieldException e)
        {
            throw new RuntimeException(String.format("Field DESCRIPTOR not found in class %s", ddf_class.getCanonicalName()), e);
        }
        catch (Throwable e)
        {
            throw new RuntimeException("Unable to locate protobuf Descriptor class", e);
        }
    }

    /*
     * Old implementation.
     * Not used anymore due to various path-problems with googles implementation
     * Embedded descriptors have to have the exakt same path, ie foo/foo.proto vs foo.proto
     * Huge practical issue when importing from another project
     */
	@SuppressWarnings("unchecked")
    public static <T> Descriptor getDescriptorProto(Class<T> ddf_class)
    {
        try
        {
            Class<T> descriptor_class = (Class<T>) Class.forName(ddf_class.getAnnotation(ProtoClassName.class).name());
            Method x = descriptor_class.getMethod("getDescriptor");
            return (Descriptor) x.invoke(null);
        } catch (Throwable e)
        {
            throw new RuntimeException("Unable to locate protobuf Descriptor class", e);
        }
    }
}
