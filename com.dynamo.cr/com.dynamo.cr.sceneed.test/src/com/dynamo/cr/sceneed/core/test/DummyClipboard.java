package com.dynamo.cr.sceneed.core.test;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.ObjectInputStream;
import java.io.ObjectOutputStream;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.swt.dnd.Transfer;

import com.dynamo.cr.sceneed.core.IClipboard;

public class DummyClipboard implements IClipboard {

    private Map<Transfer, byte[]> transfers = new HashMap<Transfer, byte[]>();

    @Override
    public Object getContents(Transfer transfer) {
        Object object = null;
        ObjectInputStream in = null;
        try {
            in = new ObjectInputStream(new ByteArrayInputStream(this.transfers.get(transfer)));
            object = in.readObject();
        } catch (Exception e) {
            throw new RuntimeException(e);
        } finally {
            if (in != null) {
                try {
                    in.close();
                } catch (IOException e) {
                    throw new RuntimeException(e);
                }
            }
        }
        return object;
    }

    @Override
    public void setContents(Object[] data, Transfer[] dataTypes) {
        int n = data.length;
        for (int i = 0; i < n; ++i) {
            ByteArrayOutputStream outBytes = new ByteArrayOutputStream();
            ObjectOutputStream out = null;
            try {
                out = new ObjectOutputStream(outBytes);
                out.writeObject(data[i]);
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                if (out != null) {
                    try {
                        out.close();
                    } catch (IOException e) {
                        throw new RuntimeException(e);
                    }
                }
            }
            this.transfers.put(dataTypes[i], outBytes.toByteArray());
        }
    }

}
