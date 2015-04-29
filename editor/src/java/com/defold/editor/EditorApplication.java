package com.defold.editor;

import javafx.application.Platform;

import org.projectodd.shimdandy.ClojureRuntimeShim;

public class EditorApplication {
    public static void run(String[] args, ClassLoader classLoader) {
        ClojureRuntimeShim runtime = ClojureRuntimeShim.newRuntime(classLoader, "editor");
        runtime.invoke("editor.boot/main");
        Platform.runLater(new Runnable() {
            public void run() {
                runtime.invoke("editor.debug/start-server", null);
            }
        });        
    }
}
