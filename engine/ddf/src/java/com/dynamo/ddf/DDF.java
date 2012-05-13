package com.dynamo.ddf;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.Writer;

import com.dynamo.ddf.internal.DDFLoader;
import com.dynamo.ddf.internal.MessageBuilder;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public class DDF
{
    /**
     * Returns a string representation of the message.
     *
     * @param object Object to print
     * @return String representation
     */
    public final static String printToString(Object object)
    {
        Message m = MessageBuilder.build(object);
        return TextFormat.printToString(m);
    }

    /**
     * Load message from input stream
     * @param <T> Object type
     * @param input Input {@link InputStream}
     * @param ddf_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T load(InputStream input, Class<T> ddf_class) throws IOException
    {
        return DDFLoader.load(input, ddf_class);
    }

    /**
     * Load message in text format from readable
     *
     * @param <T> Object type
     * @param input Input {@link Readable}
     * @param ddf_class Object type class
     * @return New instance of type T
     * @throws IOException
     */
    public static <T> T loadTextFormat(Readable input, Class<T> ddf_class) throws IOException
    {
        return DDFLoader.loadTextFormat(input, ddf_class);
    }

    /**
     * Save message
     * @param obj Object to save
     * @param output Output stream {@link OutputStream}
     * @throws IOException
     */
    public static void save(Object obj, OutputStream output) throws IOException
    {
        Message msg = MessageBuilder.build(obj);
        msg.writeTo(output);
    }

    /**
     * Save message in text format
     * @param obj Object to save
     * @param writer Writer {@link Writer}
     * @throws IOException
     */
    public static void saveTextFormat(Object obj, Writer writer) throws IOException
    {
        writer.write(printToString(obj));
    }

}
