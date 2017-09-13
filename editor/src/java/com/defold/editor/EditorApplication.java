package com.defold.editor;

import java.lang.reflect.Method;

import org.projectodd.shimdandy.ClojureRuntimeShim;

/**
 * Project/Main Window abstraction. One class per project by using custom class loaders
 * @author chmu
 *
 */
public class EditorApplication {
    private ClojureRuntimeShim runtime;
    // Object as we use custom class loaders and A.class != A.class when loaded from different class loaders
    private static Object startInstance;

    public EditorApplication(Object startInstance, ClassLoader classLoader) {
        EditorApplication.startInstance = startInstance;
        runtime = ClojureRuntimeShim.newRuntime(classLoader, "editor");
        if (Editor.isDev()) {
            runtime.invoke("editor.debug/start-server", null);
        }
        runtime.require("editor.boot");
    }

    public static void openEditor(String[] args) throws Exception {
        // See comment next to startInstance why we use reflection here
        Method openEditor = startInstance.getClass().getMethod("openEditor", String[].class);
        openEditor.invoke(startInstance, new Object[] { args });
    }

    public void run(String[] args) {
        runtime.invoke("editor.boot/main", args);
    }
}
