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
import java.util.concurrent.FutureTask;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.ThreadPoolExecutor;
import java.util.concurrent.TimeUnit;

import javafx.fxml.FXMLLoader;
import javafx.scene.Parent;
import javafx.scene.Scene;
import javafx.scene.control.ButtonBase;
import javafx.stage.Modality;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.defold.editor.Updater.PendingUpdate;
import com.defold.libs.ResourceUnpacker;

import javafx.application.Application;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.stage.Stage;


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
        updateTimer.schedule(newCheckForUpdateTask(), firstUpdateDelay);
    }

    private Boolean showRestartDialog() throws IOException {
        Parent root = FXMLLoader.load(Thread.currentThread().getContextClassLoader().getResource("update-alert.fxml"));
        Stage stage = new Stage();
        Scene scene = new Scene(root);
        ButtonBase ok = (ButtonBase) root.lookup("#ok");
        ButtonBase cancel = (ButtonBase) root.lookup("#cancel");
        final Boolean[] result = {false};

        ok.setOnAction(event -> {
            result[0] = true;
            stage.close();
        });

        cancel.setOnAction(event -> {
            result[0] = false;
            stage.close();
        });

        stage.setTitle("Update Available");
        stage.initModality(Modality.APPLICATION_MODAL);
        stage.setScene(scene);
        stage.showAndWait();

        return result[0];
    }

    private TimerTask newCheckForUpdateTask() {
        return new TimerTask() {
            @Override
            public void run() {
                try {
                    logger.debug("checking for updates");
                    PendingUpdate pendingUpdate = updater.check();
                    if (pendingUpdate != null) {
                        javafx.application.Platform.runLater(() -> {
                            try {
                                if (showRestartDialog()) {
                                    updateTimer.schedule(newInstallUpdateTask(pendingUpdate), 0);
                                } else {
                                    updateTimer.schedule(newCheckForUpdateTask(), updateDelay);
                                }
                            } catch (IOException e) {
                                logger.error("unable to open update alert dialog");
                            }
                        });
                    } else {
                        updateTimer.schedule(newCheckForUpdateTask(), updateDelay);
                    }
                } catch (IOException e) {
                    logger.debug("update check failed", e);
                }
            }
        };
    }

    private TimerTask newInstallUpdateTask(PendingUpdate pendingUpdate) {
        return new TimerTask() {
            @Override
            public void run() {
                try {
                    pendingUpdate.install();
                    logger.info("update installed - restarting");
                    System.exit(17);
                } catch (IOException e) {
                    logger.debug("update installation failed", e);
                    updateTimer.schedule(newCheckForUpdateTask(), updateDelay);
                }
            }
        };
    }

    private ClassLoader makeClassLoader() {
        ArrayList<URL> urls = extractURLs(System.getProperty("java.class.path"));
        // The "boot class-loader", i.e. for java.*, sun.*, etc
        ClassLoader parent = ClassLoader.getSystemClassLoader();
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

    private void kickLoading(Splash splash) {
        threadPool.submit(() -> {
            try {
                // A terrible hack as an attempt to avoid a deadlock when loading native libraries
                // Prism might be loading native libraries at this point, although we kick this loading after the splash has been shown.
                // The current hypothesis is that the splash is "onShown" before the loading has finished and rendering can start.
                // Occular inspection shows the splash as grey for a few frames (1-3?) before filled in with graphics. That grey-time also seems to differ between runs.
                // This is an attempt to make the deadlock less likely to happen and hopefully avoid it altogether. No guarantees.
                Thread.sleep(200);
                ResourceUnpacker.unpackResources();
                ClassLoader parent = ClassLoader.getSystemClassLoader();
                Class<?> glprofile = parent.loadClass("com.jogamp.opengl.GLProfile");
                Method init = glprofile.getMethod("initSingleton");
                init.invoke(null);
            } catch (Throwable t) {
                logger.error("failed to extract native libs", t);
            }
            try {
                pool.add(makeEditor());
                javafx.application.Platform.runLater(new Runnable() {
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
                javafx.application.Platform.runLater(() -> {
                    splash.setLaunchError(message);
                    splash.setErrorShowing(true);
                });
            }
            return null;
        });
    }

    @Override
    public void start(Stage primaryStage) throws Exception {
        final Splash splash = new Splash();
        splash.shownProperty().addListener(new ChangeListener<Boolean>() {
            @Override
            public void changed(ObservableValue<? extends Boolean> observable,
                    Boolean oldValue, Boolean newValue) {
                if (newValue.booleanValue()) {
                    kickLoading(splash);
                }
            }
        });
        splash.show();
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
