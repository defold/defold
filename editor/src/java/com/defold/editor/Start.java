package com.defold.editor;

import java.io.File;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingQueue;

import javafx.application.Application;
import javafx.stage.Stage;

public class Start extends Application {

    static ArrayList<URL> extractURLs(String classPath) {
        ArrayList<URL> urls = new ArrayList<>();
        for (String s : classPath.split(File.pathSeparator)) {
            String suffix = "";
            if (!s.endsWith(".jar")) {
                suffix = "/";
            }
            URL url;
            try {
                url = new URL(String.format("file:%s%s", s, suffix));
            } catch (MalformedURLException e) {
                throw new RuntimeException(e);
            }
            urls.add(url);
        }
        return urls;
    }

    private LinkedBlockingQueue<Object> pool;
    private ExecutorService threadPool;
    private static boolean createdFromMain = false;

    public Start() {
        pool = new LinkedBlockingQueue<>(4);
        threadPool = Executors.newFixedThreadPool(4);
    }

    private Object makeEditor() throws Exception {
        ArrayList<URL> urls = extractURLs(System.getProperty("java.class.path"));
        // The "boot class-loader", i.e. for java.*, sun.*, etc
        ClassLoader parent = ClassLoader.getSystemClassLoader().getParent();
        // Per instance class-loader
        ClassLoader classLoader = new URLClassLoader(urls.toArray(new URL[urls.size()]), parent);

        // NOTE: Is context classloader required?
        // Thread.currentThread().setContextClassLoader(classLoader);

        Class<?> editorApplicationClass = classLoader.loadClass("com.defold.editor.EditorApplication");
        Object editorApplication = editorApplicationClass.getConstructor(new Class[] { Object.class, ClassLoader.class }).newInstance(this, classLoader);
        return editorApplication;
    }

    private void poolEditor() {
        FutureTask<Object> future = new FutureTask<>(new Callable<Object>() {

            @Override
            public Object call() throws Exception {
                // Arbitrary sleep in order to reduce the CPU load while loading the project
                Thread.sleep(3000);
                Object editorApplication = makeEditor();
                pool.add(editorApplication);
                return null;
            }

        });
        threadPool.submit(future);
    }

    public void openEditor(String[] args) throws Exception {
        if (!createdFromMain) {
            throw new RuntimeException(String.format("Multiple %s classes. ClassLoader errors?", this.getClass().getName()));
        }
        poolEditor();
        Object editorApplication = pool.take();
        Method run = editorApplication.getClass().getMethod("run", new Class[]{ String[].class });
        run.invoke(editorApplication, new Object[] { args });
    }

    @Override
    public void start(Stage primaryStage) throws Exception {
        // NOTE: We pool initial editor in main thread. Deadlock when pooling with poolEditor for initial
        pool.add(makeEditor());
        openEditor(new String[0]);
    }

    public static void main(String[] args) throws Exception {
        createdFromMain = true;
        Start.launch(args);
    }

}
