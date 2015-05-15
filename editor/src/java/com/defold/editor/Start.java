package com.defold.editor;

import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import javafx.application.Application;
import javafx.application.Platform;
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
    private ThreadPoolExecutor threadPool;
    private static boolean createdFromMain = false;

    public Start() {
        pool = new LinkedBlockingQueue<>(1);
        threadPool = new ThreadPoolExecutor(1, 1, 3000, TimeUnit.MILLISECONDS, new LinkedBlockingQueue<Runnable>());
        threadPool.allowCoreThreadTimeOut(true);
    }

    private ClassLoader makeClassLoader() {
        ArrayList<URL> urls = extractURLs(System.getProperty("java.class.path"));
        // The "boot class-loader", i.e. for java.*, sun.*, etc
        ClassLoader parent = ClassLoader.getSystemClassLoader().getParent();
        // Per instance class-loader
        ClassLoader classLoader = new URLClassLoader(urls.toArray(new URL[urls.size()]), parent);
        return classLoader;
    }

    private Object makeEditor() throws Exception {
        ClassLoader classLoader = makeClassLoader();

        // NOTE: Is context classloader required?
        // Thread.currentThread().setContextClassLoader(classLoader);

        Class<?> editorApplicationClass = classLoader.loadClass("com.defold.editor.EditorApplication");
        Object editorApplication = editorApplicationClass.getConstructor(new Class[] { Object.class, ClassLoader.class }).newInstance(this, classLoader);
        return editorApplication;
    }

    private void poolEditor(long delay) {
        FutureTask<Object> future = new FutureTask<>(new Callable<Object>() {

            @Override
            public Object call() throws Exception {
                // Arbitrary sleep in order to reduce the CPU load while loading the project
                Thread.sleep(delay);
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
        poolEditor(3000);
        Object editorApplication = pool.take();
        Method run = editorApplication.getClass().getMethod("run", new Class[]{ String[].class });
        run.invoke(editorApplication, new Object[] { args });
    }

    @Override
    public void start(Stage primaryStage) throws Exception {
        Splash splash = new Splash();

        FutureTask<Object> future = new FutureTask<>(new Callable<Object>() {

            @Override
            public Object call() throws Exception {

                try {
                    pool.add(makeEditor());
                    Platform.runLater(new Runnable() {
                        @Override
                        public void run() {
                            try {
                                openEditor(new String[0]);
                                splash.close();
                            } catch (Throwable t) {
                                t.printStackTrace();
                            }
                        }
                    });
                } catch (Throwable t) {
                    t.printStackTrace();
                    String message = (t instanceof InvocationTargetException) ? t.getCause().getMessage() : t.getMessage();
                    Platform.runLater(() -> {
                            splash.setLaunchError(message);
                            splash.setErrorShowing(true);
                        });
                }
                return null;
            }

        });
        threadPool.submit(future);
    }

    public static void main(String[] args) throws Exception {
        createdFromMain = true;
        Start.launch(args);
    }

}
