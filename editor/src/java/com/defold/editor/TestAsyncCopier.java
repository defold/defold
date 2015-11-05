package com.defold.editor;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import javax.media.opengl.GL2;
import javax.media.opengl.GLCapabilities;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.GLOffscreenAutoDrawable;
import javax.media.opengl.GLProfile;

import javafx.animation.AnimationTimer;
import javafx.application.Application;
import javafx.event.EventHandler;
import javafx.scene.Scene;
import javafx.scene.image.ImageView;
import javafx.scene.layout.StackPane;
import javafx.stage.Stage;
import javafx.stage.WindowEvent;

public class TestAsyncCopier extends Application {

    private GLOffscreenAutoDrawable drawable;
    private GLContext context;
    int width = 1024;
    int height = 1024;
    private float red;
    private float redDelta;
    private int frames = 0;
    private ImageView imageView;
    private AsyncCopier copier;
    private ExecutorService threadPool;

    public void setup() {
        GLProfile profile = GLProfile.getDefault();
        GLDrawableFactory factory = GLDrawableFactory.getFactory(profile);
        GLCapabilities cap = new GLCapabilities(profile);
        cap.setOnscreen(false);
        cap.setPBuffer(true);
        // TODO: true or false
        cap.setDoubleBuffered(false);

        drawable = factory.createOffscreenAutoDrawable(null, cap, null, width, height, null);
        context = drawable.getContext();
        context.makeCurrent();

        GL2 gl = (GL2) context.getGL();
        threadPool = Executors.newFixedThreadPool(1);
        copier = new AsyncCopier(gl, threadPool, imageView, width, height);
        context.release();

        red = 0.0f;
        redDelta = 0.01f;
        new AnimationTimer() {

            @Override
            public void handle(long now) {
                GL2 gl = (GL2) context.getGL();

                copier.beginFrame2(gl);

                red += redDelta;
                if (red > 1.0f) {
                    redDelta *= -1;
                    red = 1.0f;
                } else if (red < 0.0f) {
                    red = 0.0f;
                    redDelta *= -1;
                }

                if (frames == 50) {
                    try {
                        copier.dumpSamples("misc/timeseries.csv", 0);
                        System.out.println("timeseries written");
                    } catch (IOException e) {
                        e.printStackTrace();
                    }
                }
                frames++;


                gl.glClearColor(red, 0, 0, 1);
                gl.glClear(GL2.GL_COLOR_BUFFER_BIT | GL2.GL_DEPTH_BUFFER_BIT);


                copier.endFrame(gl);
            }
        }.start();;

    }

    public void dispose() {
        GL2 gl = (GL2) context.getGL();
        copier.dispose(gl);
    }

    public static void main(String[] args) {
        launch(args);
    }

    @Override
    public void start(Stage primaryStage) throws Exception {
        primaryStage.setTitle("Hello World!");
        imageView = new ImageView();
        primaryStage.setOnCloseRequest(new EventHandler<WindowEvent>() {

            @Override
            public void handle(WindowEvent event) {
                threadPool.shutdown();
                dispose();
            }
        });



        int w = 100;
        int h = 100;

        BufferedImage bufImage = new BufferedImage(w, h, BufferedImage.TYPE_INT_ARGB);
        for (int i = 0; i < w; i++) {
            for (int j = 0; j < h; j++) {
                bufImage.setRGB(i, j, 0xff00ffff);
            }

        }
        //WritableImage image = new WritableImage(w, h);
        /*if (false) {
            imageView.setImage(SwingFXUtils.toFXImage(bufImage, null));
        } else {
            imageView.setImage(SwingFXUtils.toFXImage(glImage, null));
        }*/
        //imageView.setImage(image);

/*        Button btn = new Button();
        btn.setText("Say 'Hello World'");
        btn.setOnAction(new EventHandler<ActionEvent>() {

            @Override
            public void handle(ActionEvent event) {
                System.out.println("Hello World!");
            }
        });*/

        StackPane root = new StackPane();
        root.getChildren().add(imageView);
        primaryStage.setScene(new Scene(root, 512, 512));
        primaryStage.show();

        setup();
    }
}
