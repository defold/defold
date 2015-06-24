package com.defold.editor;

import java.io.File;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.ArrayList;
import java.util.Timer;
import java.util.TimerTask;
import java.util.concurrent.Callable;
import java.util.concurrent.Future;
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import javafx.application.Application;
import javafx.stage.Stage;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.defold.editor.Updater.UpdateInfo;
import com.defold.libs.Libs;


public class Start extends Application {

    private static Logger logger = LoggerFactory.getLogger(Start.class);

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
    private Timer updateTimer;
    private Updater updater;
    private static boolean createdFromMain = false;
    private final int firstUpdateDelay = 1000;
    private final int updateDelay = 60000;

    public Start() throws IOException {
        pool = new LinkedBlockingQueue<>(1);
        threadPool = new ThreadPoolExecutor(1, 1, 3000, TimeUnit.MILLISECONDS,
                new LinkedBlockingQueue<Runnable>());
        threadPool.allowCoreThreadTimeOut(true);

        if (System.getProperty("defold.resourcespath") != null && System.getProperty("defold.sha1") != null)  {
            logger.debug("automatic updates enabled");
            installUpdater();
        }
    }

    private void installUpdater() throws IOException {
        // TODO: Localhost. Move to config or equivalent
        updater = new Updater("http://d.defold.com/editor2", System.getProperty("defold.resourcespath"), System.getProperty("defold.sha1"));
        updateTimer = new Timer();
        updateTimer.schedule(newUpdateTask(), firstUpdateDelay);
    }

    private TimerTask newUpdateTask() {
        return new TimerTask() {
            @Override
            public void run() {
                try {
                    logger.debug("checking for updates");
                    UpdateInfo updateInfo = updater.check();
                    if (updateInfo != null) {
                        System.exit(17);
                    }
                    updateTimer.schedule(newUpdateTask(), updateDelay);
                } catch (IOException e) {
                    logger.debug("update failed", e);
                }
            }
        };
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

        Future<?> extractFuture = threadPool.submit(() -> {
            try {
                Libs.extractNativeLibs();
            } catch (Exception e) {
                logger.error("failed to extract native libs", e);
            }
        });

        threadPool.submit(() -> {
            try {
                pool.add(makeEditor());
                javafx.application.Platform.runLater(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            extractFuture.get();
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
                javafx.application.Platform.runLater(() -> {
                    splash.setLaunchError(message);
                    splash.setErrorShowing(true);
                });
            }
            return null;
        });
    }

    @Override
    public void stop() throws Exception {
        // NOTE: We force exit here as it seems like the shutdown process
        // is waiting for all non-daemon threads to terminate, e.g. clojure agent thread
        System.exit(0);
    }

    public static void main(String[] args) throws Exception {
        createdFromMain = true;
        Start.launch(args);
    }

}
