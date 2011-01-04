package com.dynamo.cr.contenteditor;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.DND;
import org.eclipse.swt.dnd.TransferData;

import com.dynamo.ddf.DDF;
import com.dynamo.ddf.annotations.ProtoClassName;

public class DDFTransfer extends ByteArrayTransfer
{
    private static final String DDFTYPENAME = "ddf_transfer";

    private static final int DDFTYPEID = registerType(DDFTYPENAME);

    private static DDFTransfer _instance = new DDFTransfer();

    public static DDFTransfer getInstance()
    {
        return _instance;
    }

    @Override
    protected void javaToNative(Object object, TransferData transferData)
    {
        ProtoClassName proto_class_name = object.getClass().getAnnotation(ProtoClassName.class);
        if (proto_class_name == null || !isSupportedType(transferData))
            DND.error(DND.ERROR_INVALID_DATA);

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(out);
        try
        {
            dos.writeUTF(object.getClass().getName());
            dos.flush();
            dos.close();
            DDF.save(object, out);
            super.javaToNative(out.toByteArray(), transferData);
        } catch (IOException e)
        {
            e.printStackTrace();
        }
    }
    @Override
    protected Object nativeToJava(TransferData transferData)
    {
        byte[] buffer = (byte[]) super.nativeToJava(transferData);
        if (buffer == null)
            return null;
        ByteArrayInputStream in = new ByteArrayInputStream(buffer);
        DataInputStream dis = new DataInputStream(in);

        try
        {
            String name = dis.readUTF();
            Class<?> cls = Class.forName(name);
            return DDF.load(in, cls);
        } catch (Throwable e)
        {
            e.printStackTrace();
        }
        return null;
    }

    @Override
    protected String[] getTypeNames()
    {
        return new String[] { DDFTYPENAME };
    }

    @Override
    protected int[] getTypeIds()
    {
        return new int[] { DDFTYPEID };
    }
}
