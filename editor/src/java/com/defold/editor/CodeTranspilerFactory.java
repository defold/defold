package com.defold.editor;

import java.io.IOException;
import java.io.Reader;

public interface CodeTranspilerFactory {
    CodeTranspiler makeCodeTranspiler(Reader reader, String projPath) throws IOException;
}
