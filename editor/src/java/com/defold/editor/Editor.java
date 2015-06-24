package com.defold.editor;

public class Editor {

    public static boolean isDev() {
        return System.getProperty("defold.version") == null;
    }

}
