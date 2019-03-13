package com.defold.editor;

import ch.qos.logback.classic.encoder.PatternLayoutEncoder;
import ch.qos.logback.core.rolling.RollingFileAppender;
import ch.qos.logback.core.rolling.TimeBasedRollingPolicy;
import ch.qos.logback.core.util.FileSize;
import com.defold.libs.ResourceUnpacker;
import javafx.application.Application;
import javafx.application.Platform;
import javafx.stage.Stage;
import org.apache.commons.io.FileUtils;
import org.apache.commons.io.filefilter.FalseFileFilter;
import org.apache.commons.io.filefilter.WildcardFileFilter;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.bridge.SLF4JBridgeHandler;

import java.awt.image.BufferedImage;
import java.io.File;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.util.Collection;
import java.util.List;

public class Start extends Application {

    private static Logger logger = LoggerFactory.getLogger(Start.class);

    private void kickLoading(Splash splash) {
        // Do this work in a different thread or it will stop the splash screen from showing/animating.
        Thread kickThread = new Thread(() -> {
                try {
                    // A terrible hack as an attempt to avoid a deadlock when loading native libraries.
                    // Prism might be loading native libraries at this point, although we kick this loading after the splash has been shown.
                    // The current hypothesis is that the splash is "onShown" before the loading has finished and rendering can start.
                    // Ocular inspection shows the splash as grey for a few frames (1-3?) before filled in with graphics. That grey-time also seems to differ between runs.
                    // This is an attempt to make the deadlock less likely to happen and hopefully avoid it altogether. No guarantees.
                    Thread.sleep(200);
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
                        final String message = t.getMessage();
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

            new BufferedImage(1, 1, BufferedImage.TYPE_3BYTE_BGR);

            // Clean up old packages as they consume a lot of hard drive space.
            // NOTE! This is a temp hack to give some hard drive space back to users.
            // The proper fix would be an upgrade feature where users can upgrade and downgrade as desired.
            prunePackages();

            final Splash splash = new Splash();
            splash.shownProperty().addListener((observable, oldValue, newValue) -> {
                if (newValue) {
                    try {
                        kickLoading(splash);
                    } catch (Throwable t) {
                        t.printStackTrace();
                        logger.error("failed to kick loading", t);
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
    public void stop() {
        Shutdown.runShutdownActions();

        // NOTE: We force exit here as it seems like the shutdown process
        // is waiting for all non-daemon threads to terminate, e.g. clojure agent thread
        // TODO: See if we can shut down these threads gracefully and avoid this.
        System.exit(0);
    }

    public static void main(String[] args) {
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
