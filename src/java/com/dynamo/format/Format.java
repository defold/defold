package com.dynamo.format;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;

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
     * Load message from input stream
     * @param <T> Object type
     * @param input Input {@link InputStream}
     * @param desciptor Protocol descriptor {@link Descriptor}
     * @param format_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T load(InputStream input, Descriptor desciptor, Class<T> format_class) throws IOException
    {
        return FormatLoader.load(input, desciptor, format_class);
    }

    /**
     * Load message in text format from readable
     * 
     * @param <T> Object type
     * @param input Input {@link Readable}
     * @param desciptor Protocol descriptor {@link Descriptor}
     * @param format_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T loadTextFormat(Readable input, Descriptor desciptor, Class<T> format_class) throws IOException
    {
        return FormatLoader.loadTextFormat(input, desciptor, format_class);
    }

    /**
     * Save message
     * @param obj Object to save
     * @param descriptor Message descriptor {@link Descriptor}
     * @param output Output stream {@link OutputStream}
     * @throws IOException
     */
    public static void save(Object obj, Descriptor descriptor, OutputStream output) throws IOException
    {
        Message msg = MessageBuilder.build(obj, descriptor);
        msg.writeTo(output);
    }

    /**
     * Save message in text format
     * @param obj Object to save
     * @param descriptor Message descriptor {@link Descriptor}
     * @param writer Writer {@link Writer}
     * @throws IOException
     */
    public static void saveTextFormat(Object obj, Descriptor descriptor, Writer writer) throws IOException
    {
        writer.write(printToString(obj, descriptor));
    }

}
