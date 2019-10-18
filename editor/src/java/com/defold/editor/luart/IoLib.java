package com.defold.editor.luart;

import org.luaj.vm2.lib.jse.JseIoLib;

import java.io.IOException;

public class IoLib extends JseIoLib {

    /**
     * made public to avoid reflection from clojure
     */
    @Override
    public File openFile(String s, boolean b, boolean b1, boolean b2, boolean b3) throws IOException {
        return super.openFile(s, b, b1, b2, b3);
    }
}
