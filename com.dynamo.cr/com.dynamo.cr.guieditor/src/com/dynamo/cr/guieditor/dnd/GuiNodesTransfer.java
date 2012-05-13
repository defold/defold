package com.dynamo.cr.guieditor.dnd;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.ArrayList;
import java.util.List;

import org.eclipse.swt.dnd.ByteArrayTransfer;
import org.eclipse.swt.dnd.TransferData;

import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Builder;
import com.google.protobuf.TextFormat;

public class GuiNodesTransfer extends ByteArrayTransfer {
    private static final String TYPENAME = "dynamo.NodeDescs";
    private static final int TYPEID = registerType (TYPENAME);

    private static GuiNodesTransfer instance = new GuiNodesTransfer ();

    @Override
    protected void javaToNative(Object object, TransferData transferData) {

        @SuppressWarnings("unchecked")
        List<NodeDesc> nodes = (List<NodeDesc>) object;
        List<String> textNodes = new ArrayList<String>();

        for (NodeDesc node : nodes) {
            textNodes.add(node.toString());
        }

        try {
            ByteArrayOutputStream out = new ByteArrayOutputStream ();
            ObjectOutputStream objectOut = new ObjectOutputStream(out);
            objectOut.writeObject(textNodes);
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
            @SuppressWarnings("unchecked")
            List<String> textNodes = (List<String>) objectIn.readObject();
            List<NodeDesc> nodes = new ArrayList<NodeDesc>();
            for (String text : textNodes) {
                Builder builder = NodeDesc.newBuilder();
                TextFormat.merge(text, builder);
                nodes.add(builder.build());
            }
            return nodes;

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

    public static GuiNodesTransfer getInstance() {
        return instance;
    }

}
