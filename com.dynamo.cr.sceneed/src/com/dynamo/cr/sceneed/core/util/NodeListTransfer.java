package com.dynamo.cr.sceneed.core.util;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;

import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.TransferData;

public class NodeListTransfer extends ByteArrayTransfer {
    private static final String TYPENAME = "com.dynamo.cr.sceneed.NodeList";
    private static final int TYPEID = registerType (TYPENAME);

    private static NodeListTransfer instance = new NodeListTransfer ();

    @Override
    protected void javaToNative(Object object, TransferData transferData) {

        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream ();
            ObjectOutputStream objectOut = new ObjectOutputStream(out);
            objectOut.writeObject(object);
            objectOut.close();
            super.javaToNative(out.toByteArray(), transferData);

        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected Object nativeToJava(TransferData transferData) {
        byte [] buffer = (byte []) super.nativeToJava (transferData);
        if (buffer == null) return null;
        try {
            ByteArrayInputStream in = new ByteArrayInputStream (buffer);
            ObjectInputStream objectIn = new ObjectInputStream(in);
            return objectIn.readObject();

        } catch (Throwable e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    protected int[] getTypeIds() {
        return new int [] { TYPEID };
    }

    @Override
    protected String[] getTypeNames() {
        return new String [] { TYPENAME };
    }

    public static NodeListTransfer getInstance() {
        return instance;
    }

}
