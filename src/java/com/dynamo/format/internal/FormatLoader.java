package com.dynamo.format.internal;

import java.io.IOException;
import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.List;

import com.dynamo.format.annotations.ComponentType;
import com.google.protobuf.ByteString;
import com.google.protobuf.DynamicMessage;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.Descriptors.Descriptor;
import com.google.protobuf.Descriptors.FieldDescriptor;

public class FormatLoader
{

    private static void merge(Object obj, Message from) throws Exception
    {
        List<FieldDescriptor> message_fields = from.getDescriptorForType().getFields();

        for (FieldDescriptor message_f : message_fields)
        {
            Field f = obj.getClass().getDeclaredField("m_" + message_f.getName());
            f.setAccessible(true);
            Object field_value = from.getField(message_f);
            Object value;
            if (field_value instanceof List<?>)
            {
                value = createList(obj, message_f, (List<?>) field_value, f
                        .getAnnotation(ComponentType.class).type());
            }
            else if (field_value instanceof Message)
            {
                value = f.getType().newInstance();
                FormatLoader.merge(value, (Message) field_value);
            }
            else if (field_value instanceof ByteString)
            {
                value = ((ByteString) field_value).toByteArray();
            }
            else
            {
                value = field_value;
            }
            f.set(obj, value);
        }
    }

    private static Object createList(Object obj, FieldDescriptor field_desc,
            List<?> from_list, Class<?> elem_class) throws Exception
    {

        ArrayList<Object> list = new ArrayList<Object>();
        for (Object field_o : from_list)
        {
            if (field_o instanceof Message)
            {
                Object o = elem_class.newInstance();
                FormatLoader.merge(o, (Message) field_o);
                list.add(o);
            }
            else if (field_o instanceof List<?>)
            {
                // TODO:
                throw new RuntimeException("TODO?");
            }
            else
            {
                list.add(field_o);
            }
        }
        return list;
    }

    public static <T> T load(Readable input, Descriptor desciptor, Class<T> format_class)
            throws IOException
    {
        DynamicMessage.Builder b = DynamicMessage.newBuilder(desciptor);
        TextFormat.merge(input, b);
        
        try
        {
            T ret = format_class.newInstance();
            FormatLoader.merge(ret, b.build());

            return ret;
        } catch (Throwable e)
        {
            // TODO: Checked exception?
            throw new RuntimeException(e);
        }
    }
}
