package com.dynamo.ddf.internal;

import java.lang.reflect.Field;
import java.util.List;

import com.google.protobuf.ByteString;
import com.google.protobuf.DescriptorProtos;
import com.google.protobuf.Descriptors;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;
import com.google.protobuf.Descriptors.FieldDescriptor.JavaType;

public class MessageBuilder
{

    private final static Object build2(Object object, FieldDescriptor field_descriptor)
            throws Throwable
    {
        assert (!field_descriptor.isRepeated());

        JavaType java_type = field_descriptor.getJavaType();
        switch (java_type)
        {
        case BOOLEAN:
        case DOUBLE:
        case FLOAT:
        case INT:
        case LONG:
        case STRING:
            return object;
        case ENUM:
        	return field_descriptor.getEnumType().findValueByNumber((Integer) object);

        case BYTE_STRING:
            return ByteString.copyFrom((byte[]) object);

        case MESSAGE:
            return buildMessage(object, field_descriptor.getMessageType());

        default:
            throw new RuntimeException("Internal error");
        }
    }

    private static Message buildMessage(Object object, Descriptor message_descriptor)
            throws Throwable
    {
        List<FieldDescriptor> fields = message_descriptor.getFields();
        DynamicMessage.Builder builder = DynamicMessage.newBuilder(message_descriptor);

        for (FieldDescriptor fd : fields)
        {
            Field f = object.getClass().getField("m_" + fd.getName());
            f.setAccessible(true);

            if (fd.isRepeated())
            {
                buildList(f.get(object), builder, fd);
            }
            else
            {
                builder.setField(fd, build2(f.get(object), fd));
            }
        }

        return builder.build();
    }

    private static void buildList(Object object, DynamicMessage.Builder builder,
            FieldDescriptor field_descriptor) throws Throwable
    {
        List<?> lst = (List<?>) object;
        for (Object o : lst)
        {
            builder.addRepeatedField(field_descriptor, build2(o, field_descriptor));
        }
    }

    public final static Message build(Object object)
    {
        Descriptor message_descriptor = DDFUtil.getDescriptor(object.getClass());

        try
        {
            return buildMessage(object, message_descriptor);
        } catch (Throwable e)
        {
            // TODO: Checked exception?
            throw new RuntimeException(e);
        }
    }
}
