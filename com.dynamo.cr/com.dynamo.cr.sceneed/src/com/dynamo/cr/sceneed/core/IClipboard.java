package com.dynamo.cr.sceneed.core;

import org.eclipse.swt.dnd.Transfer;

public interface IClipboard {
    Object getContents(Transfer transfer);
    void setContents(Object[] data, Transfer[] dataTypes);
}
