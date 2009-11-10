package com.dynamo.ddf.internal;

import java.lang.reflect.Method;

import com.dynamo.ddf.annotations.ProtoClassName;
import com.google.protobuf.Descriptors.Descriptor;

public class DDFUtil
{
    @SuppressWarnings("unchecked")
    public static <T> Descriptor getDescriptor(Class<T> ddf_class)
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
