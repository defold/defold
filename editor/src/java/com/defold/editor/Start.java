package com.defold.editor;

import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.net.MalformedURLException;
import java.net.URL;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

import org.apache.commons.io.FileUtils;
import org.apache.commons.io.filefilter.FalseFileFilter;
import org.apache.commons.io.filefilter.WildcardFileFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import ch.qos.logback.classic.encoder.PatternLayoutEncoder;
import ch.qos.logback.core.rolling.*;
import ch.qos.logback.core.util.FileSize;

import com.defold.libs.ResourceUnpacker;

import javafx.application.Application;
import javafx.application.Platform;
import javafx.beans.value.ChangeListener;
import javafx.beans.value.ObservableValue;
import javafx.stage.Stage;
import org.slf4j.bridge.SLF4JBridgeHandler;


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

    private static boolean createdFromMain = false;

    private void kickLoading(Splash splash) throws Exception {
        // Do this work in a different thread or it will stop the splash screen from showing/animating.
        Thread kickThread = new Thread(() -> {
                try {
                    ResourceUnpacker.unpackResources();
                    ClassLoader parent = ClassLoader.getSystemClassLoader();
                    Class<?> glprofile = parent.loadClass("com.jogamp.opengl.GLProfile");
                    Method init = glprofile.getMethod("initSingleton");
                    init.invoke(null);
                    final EditorApplication app = new EditorApplication(Thread.currentThread().getContextClassLoader());
                    app.waitForClojureNamespaces();
                    try {
                        Platform.runLater(() -> {
                                try {
                                    List<String> params = getParameters().getRaw();
                                    app.run(params.toArray(new String[params.size()]));
                                    splash.close();
                                } catch (Throwable t) {
                                    t.printStackTrace();
                                    logger.error("failed to open editor", t);
                                }
                            });
                    } catch (Throwable t) {
                        t.printStackTrace();
                        String message = (t instanceof InvocationTargetException) ? t.getCause().getMessage() : t.getMessage();
                        logger.error(message, t);
                        Platform.runLater(() -> {
                                splash.setLaunchError(message);
                                splash.setErrorShowing(true);
                            });
                    }
                } catch (Throwable t) {
                    t.printStackTrace();
                    logger.error("failed to extract native libs", t);
                }
            });
        kickThread.start();
    }

    private void prunePackages() {
        String sha1 = System.getProperty("defold.editor.sha1");
        String resourcesPath = System.getProperty("defold.resourcespath");
        if (sha1 != null && resourcesPath != null) {
            try {
                File dir = new File(resourcesPath, "packages");
                if (dir.exists() && dir.isDirectory()) {
                    Collection<File> files = FileUtils.listFiles(dir, new WildcardFileFilter("defold-*.jar"), FalseFileFilter.FALSE);
                    for (File f : files) {
                        if (!f.getName().contains(sha1)) {
                            f.delete();
                        }
                    }
                }
            } catch (Throwable t) {
                t.printStackTrace();
                logger.error("could not prune packages", t);
            }
        }
    }

    @Override
    public void start(Stage primaryStage) throws Exception {
        try {
            /*
              Note
              Don't remove

              Background
              Before the mysterious line below Command-H on OSX would open a generic Java about dialog instead of hiding the application.
              The hypothosis is that awt must be initialized before JavaFX and in particular on the main thread as we're pooling stuff using
              a threadpool.
              Something even more mysterious is that if the construction of the buffered image is moved to "static void main(.." we get a null pointer in
              clojure.java.io/resource..
            */

            BufferedImage tmp = new BufferedImage(1, 1, BufferedImage.TYPE_3BYTE_BGR);

            // Clean up old packages as they consume a lot of hard drive space.
            // NOTE! This is a temp hack to give some hard drive space back to users.
            // The proper fix would be an upgrade feature where users can upgrade and downgrade as desired.
            prunePackages();

            final Splash splash = new Splash();
            splash.shownProperty().addListener(new ChangeListener<Boolean>() {
                    @Override
                    public void changed(ObservableValue<? extends Boolean> observable,
                                        Boolean oldValue, Boolean newValue) {
                        if (newValue.booleanValue()) {
                            try {
                                kickLoading(splash);
                            } catch (Throwable t) {
                                t.printStackTrace();
                                logger.error("failed to kick loading", t);
                            }
                        }
                    }
                });
            splash.show();
        } catch (Throwable t) {
            t.printStackTrace();
            logger.error("failed start", t);
            throw t;
        }
    }

    @Override
    public void stop() throws Exception {
        Shutdown.runShutdownActions();

        // NOTE: We force exit here as it seems like the shutdown process
        // is waiting for all non-daemon threads to terminate, e.g. clojure agent thread
        // TODO: See if we can shut down these threads gracefully and avoid this.
        System.exit(0);
    }

    public static void main(String[] args) {
        createdFromMain = true;
        initializeLogging();
        Start.launch(args);
    }

    private static void initializeLogging() {
        Path logDirectory = Editor.getLogDirectory();
        ch.qos.logback.classic.Logger root = (ch.qos.logback.classic.Logger) LoggerFactory.getLogger(Logger.ROOT_LOGGER_NAME);
        SLF4JBridgeHandler.removeHandlersForRootLogger();
        SLF4JBridgeHandler.install();

        RollingFileAppender appender = new RollingFileAppender();
        appender.setName("FILE");
        appender.setAppend(true);
        appender.setPrudent(true);
        appender.setContext(root.getLoggerContext());

        TimeBasedRollingPolicy rollingPolicy = new TimeBasedRollingPolicy();
        rollingPolicy.setMaxHistory(30);
        rollingPolicy.setFileNamePattern(logDirectory.resolve("editor2.%d{yyyy-MM-dd}.log").toString());
        rollingPolicy.setTotalSizeCap(FileSize.valueOf("1GB"));
        rollingPolicy.setContext(root.getLoggerContext());
        rollingPolicy.setParent(appender);
        appender.setRollingPolicy(rollingPolicy);
        rollingPolicy.start();

        PatternLayoutEncoder encoder = new PatternLayoutEncoder();
        encoder.setPattern("%d{yyyy-MM-dd HH:mm:ss.SSS} %-4relative [%thread] %-5level %logger{35} - %msg%n");
        encoder.setContext(root.getLoggerContext());
        encoder.start();

        appender.setEncoder(encoder);
        appender.start();

        root.addAppender(appender);
    }

}
