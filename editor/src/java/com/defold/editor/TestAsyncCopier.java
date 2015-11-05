package com.defold.editor;

import java.awt.image.BufferedImage;
import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

import javax.media.opengl.GL2;
import javax.media.opengl.GLAutoDrawable;
import javax.media.opengl.GLCapabilities;
import javax.media.opengl.GLContext;
import javax.media.opengl.GLDrawableFactory;
import javax.media.opengl.GLEventListener;
import javax.media.opengl.GLOffscreenAutoDrawable;
import javax.media.opengl.GLProfile;

import com.jogamp.opengl.util.awt.Screenshot;

import javafx.animation.AnimationTimer;
import javafx.application.Application;
import javafx.embed.swing.SwingFXUtils;
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
        //drawable.setAutoSwapBufferMode(false);
        context = drawable.getContext();
        context.makeCurrent();

        GL2 gl = (GL2) context.getGL();
        threadPool = Executors.newFixedThreadPool(1);
        copier = new AsyncCopier(gl, threadPool, imageView, width, height);
        context.release();

        drawable.addGLEventListener(new GLEventListener() {

            @Override
            public void reshape(GLAutoDrawable drawable, int x, int y, int width, int height) {
                gl.glViewport(x, y, width, height);
                System.out.println("reshape");

            }

            @Override
            public void init(GLAutoDrawable drawable) {
                // TODO Auto-generated method stub

            }

            @Override
            public void dispose(GLAutoDrawable drawable) {
                // TODO Auto-generated method stub

            }

            @Override
            public void display(GLAutoDrawable drawable) {
                //System.out.println("display");
                GL2 gl = (GL2) context.getGL();
                draw(gl);


            }
        });
        //drawable.display();
        //drawable.an
        //FPSAnimator animator = new FPSAnimator(drawable, 60);
        //animator.start();

        gl.glViewport(0, 0, width, height);

        /*Thread t = new Thread() {
            @Override
            public void run() {
                while (true) {
                    try {
                        Thread.sleep(16);
                    } catch (InterruptedException e) {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                    }
                    GL2 gl = (GL2) context.getGL();
                    //gl.getContext().makeCurrent();
                    draw(gl);
                }
            }
        };
        t.start();*/


        //gl.getContext().makeCurrent();

        red = 0.0f;
        redDelta = 0.01f;
        new AnimationTimer() {

            @Override
            public void handle(long now) {
                GL2 gl = (GL2) context.getGL();
                draw(gl);
                //drawable.display();

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


        StackPane root = new StackPane();
        root.getChildren().add(imageView);
        primaryStage.setScene(new Scene(root, width, height));
        primaryStage.show();

        setup();
    }

    private void draw(GL2 gl) {
        if (true) {
            copier.beginFrame2(gl);
        }

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

        gl.glMatrixMode(GL2.GL_PROJECTION);
        gl.glLoadIdentity();
        gl.glOrtho(-1, 1, -1, 1, -1, 1);

        gl.glMatrixMode(GL2.GL_MODELVIEW);
        gl.glLoadIdentity();


        //gl.glCullFace(GL2.GL_FRONT_AND_BACK);

        gl.glBegin(GL2.GL_QUADS);
        float z = 0.0f;
        float w = 1.0f;
        float alpha = 1.0f;
        gl.glColor4f(1,0,0,alpha);
        gl.glVertex3f(-w, -w, z);

        gl.glColor4f(0,1,0,alpha);
        gl.glVertex3f(-w, w, z);

        gl.glColor4f(0,0,1,alpha);
        gl.glVertex3f(w, w, z);

        gl.glColor4f(0,0,0,alpha);
        gl.glVertex3f(w, -w*0.5f, z);

        gl.glEnd();
/*
        gl.glBegin(GL2.GL_POLYGON);
        gl.glVertex3d(0.0, 0.0, 0.0);
        gl.glVertex3d(0.5, 0.0, 0.0);
        gl.glVertex3d(0.5, 0.5, 0.0);
        gl.glVertex3d(0.0, 0.5, 0.0);
        gl.glEnd();
*/

        if (true) {
            copier.endFrame(gl);
        } else {
            BufferedImage img = Screenshot.readToBufferedImage(0, 0, width, height, true);
            imageView.setImage(SwingFXUtils.toFXImage(img, null));
        }

        /*
        try {
            ImageIO.write(img,"png",new File("/tmp/im.png"));
        } catch (IOException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }*/

        //drawable.swapBuffers();

    }
}
