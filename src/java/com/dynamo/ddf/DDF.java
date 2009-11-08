package com.dynamo.ddf;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;

import com.dynamo.ddf.internal.DDFLoader;
import com.dynamo.ddf.internal.MessageBuilder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;
import com.google.protobuf.Descriptors.Descriptor;

public class DDF
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
     * @param ddf_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T load(InputStream input, Descriptor desciptor, Class<T> ddf_class) throws IOException
    {
        return DDFLoader.load(input, desciptor, ddf_class);
    }

    /**
     * Load message in text format from readable
     *
     * @param <T> Object type
     * @param input Input {@link Readable}
     * @param desciptor Protocol descriptor {@link Descriptor}
     * @param ddf_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T loadTextFormat(Readable input, Descriptor desciptor, Class<T> ddf_class) throws IOException
    {
        return DDFLoader.loadTextFormat(input, desciptor, ddf_class);
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
