package com.dynamo.cr.sceneed.core.test;

import java.util.HashMap;
import java.util.Map;

import org.eclipse.swt.dnd.Transfer;

import com.dynamo.cr.sceneed.core.IClipboard;

public class TestClipboard implements IClipboard {

    private Map<Transfer, Object> transfers = new HashMap<Transfer, Object>();

    @Override
    public Object getContents(Transfer transfer) {
        return this.transfers.get(transfer);
    }

    @Override
    public void setContents(Object[] data, Transfer[] dataTypes) {
        int n = data.length;
        for (int i = 0; i < n; ++i) {
            this.transfers.put(dataTypes[i], data[i]);
        }
    }

}
