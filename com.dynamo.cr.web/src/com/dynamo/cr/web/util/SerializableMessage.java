package com.dynamo.cr.web.util;

import java.io.IOException;
import java.io.Serializable;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;

import com.google.protobuf.Message;

/**
 * Help class to make google protocol buffers serializable
 * @author chmu
 *
 * @param <T>
 */
public class SerializableMessage<T extends Message> implements Serializable {
    private static final long serialVersionUID = 1L;

    public T message;

    public SerializableMessage(T message) {
        this.message = message;
    }

    public static <T extends Message> List<SerializableMessage<T>> fromList(List<T> lst) {
        ArrayList<SerializableMessage<T>> ret = new ArrayList<SerializableMessage<T>>();
        for (T msg : lst) {
            ret.add(new SerializableMessage<T>(msg));
        }
        return ret;
    }

    private void writeObject(java.io.ObjectOutputStream out) throws IOException {
        String name = message.getClass().getName();
        out.writeInt(name.length());
        out.write(name.getBytes());
        message.writeTo(out);
    }

    @SuppressWarnings("unchecked")
    private void readObject(java.io.ObjectInputStream in) throws IOException, ClassNotFoundException, SecurityException, NoSuchMethodException, IllegalArgumentException, IllegalAccessException, InvocationTargetException {
        int len = in.readInt();
        byte[] nameBuffer = new byte[len];
        in.read(nameBuffer, 0, len);
        String className= new String(nameBuffer);
        Class<Message> messageClass = (Class<Message>) Class.forName(className);
        Method newBuilder = messageClass.getMethod("newBuilder");
        Message.Builder builder = (Message.Builder) newBuilder.invoke(messageClass);
        builder.mergeFrom(in);
        message = (T) builder.build();
    }

    @Override
    public String toString() {
        return message.toString();
    }
}