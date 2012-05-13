package com.dynamo.cr.sceneed.ui;

import org.eclipse.swt.dnd.Clipboard;
import org.eclipse.swt.dnd.Transfer;
import org.eclipse.swt.widgets.Display;

import com.dynamo.cr.sceneed.core.IClipboard;

public class SceneClipboard implements IClipboard {

    @Override
    public Object getContents(Transfer transfer) {
        Clipboard clipboard = new Clipboard(Display.getCurrent());
        return clipboard.getContents(transfer);
    }

    @Override
    public void setContents(Object[] data, Transfer[] dataTypes) {
        Clipboard clipboard = new Clipboard(Display.getCurrent());
        clipboard.setContents(data, dataTypes);
    }

}
