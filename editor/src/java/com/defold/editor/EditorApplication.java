package com.defold.editor;

import org.projectodd.shimdandy.ClojureRuntimeShim;

/**
 * Project/Main Window abstraction. One class per project by using custom class loaders
 * @author chmu
 *
 */
public class EditorApplication {
    private ClojureRuntimeShim runtime;

    public EditorApplication(ClassLoader classLoader) {
        runtime = ClojureRuntimeShim.newRuntime(classLoader, "editor");
        if (Editor.isDev()) {
            runtime.invoke("editor.debug/start-server", null);
        }
        runtime.invoke("editor.bootloader/load-boot");
    }

    public void waitForClojureNamespaces() {
        runtime.invoke("editor.bootloader/wait-until-editor-boot-loaded");
    }

    public void run(String[] args) {
        runtime.invoke("editor.bootloader/main", args);
    }
}
