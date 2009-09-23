package com.dynamo.format;

import java.io.IOException;

import com.dynamo.format.internal.FormatLoader;
import com.dynamo.format.internal.MessageBuilder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.Descriptors.Descriptor;

public class Format
{
    /**
     * Returns a string representation of the message.
     * 
     * @param object Object to print
     * @param descriptor Type descriptor
     * @return String representation
     */
    public final static String printToString(Object object, Descriptor descriptor)
    {
        Message m = MessageBuilder.build(object, descriptor);
        return TextFormat.printToString(m);
    }

    /**
     * Load message from readable
     * 
     * @param <T> Object type
     * @param input Input {@link Readable}
     * @param desciptor Protocol descriptor {@link Descriptor}
     * @param format_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T load(Readable input, Descriptor desciptor, Class<T> format_class) throws IOException
    {
        return FormatLoader.load(input, desciptor, format_class);
    }

}
