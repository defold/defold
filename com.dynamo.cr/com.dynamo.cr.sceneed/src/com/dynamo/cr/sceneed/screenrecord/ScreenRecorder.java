package com.dynamo.cr.sceneed.screenrecord;

import java.awt.AWTException;
import java.awt.Dimension;
import java.awt.Graphics;
import java.awt.MouseInfo;
import java.awt.Point;
import java.awt.Robot;
import java.awt.Toolkit;
import java.awt.image.BufferedImage;
import java.awt.image.RenderedImage;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.concurrent.ArrayBlockingQueue;

import javax.imageio.IIOImage;
import javax.imageio.ImageIO;
import javax.imageio.ImageWriteParam;
import javax.imageio.ImageWriter;
import javax.imageio.stream.FileImageOutputStream;

import org.eclipse.jface.dialogs.MessageDialog;

/**
 * Screen recorder utility
 * Use something like this to encode invidivual images to a video:
 *
 * # ffmpeg -f image2 -r 30 -i frame%04d.jpg -vcodec libx264 /tmp/c/v.mp4
 *
 * @author chmu
 *
 */

public class ScreenRecorder  {

    private String path;
    private boolean recording;
    private int nextFrame;
    private ArrayBlockingQueue<Entry> queue = new ArrayBlockingQueue<ScreenRecorder.Entry>(16);
    private Writer writer;
    private BufferedImage cursor;
    private Robot robot;

    static class Entry {
        public Entry(BufferedImage image, String fileName) {
            this.image = image;
            this.fileName = fileName;
        }
        public RenderedImage image;
        public String fileName;

    }

    class Writer extends Thread {

        private ImageWriter imageWriter;
        private ImageWriteParam iwp;

        public Writer() {
            imageWriter = (ImageWriter)ImageIO.getImageWritersByFormatName("jpeg").next();
            iwp = imageWriter.getDefaultWriteParam();
            iwp.setCompressionMode(ImageWriteParam.MODE_EXPLICIT);
            iwp.setCompressionQuality(0.9f);

        }

        @Override
        public void run() {
            while (recording) {
                try {
                    Entry e = queue.poll();
                    if (e != null) {
                        writeImage(e);
                    } else {
                        Thread.sleep(1);
                    }
                } catch (IOException e) {
                    e.printStackTrace();
                } catch (InterruptedException e) {
                    break;
                }
            }
            imageWriter.dispose();
        }

        private void writeImage(Entry e) throws FileNotFoundException,
                IOException {
            File file = new File(e.fileName);
            FileImageOutputStream output = new FileImageOutputStream(file);
            imageWriter.setOutput(output);
            IIOImage image = new IIOImage(e.image, null, null);
            imageWriter.write(null, image, iwp);
            output.close();
        }
    }

    public ScreenRecorder(String path) {
        this.path = path;
        File f = new File(path);
        if (!f.exists()) {
            f.mkdir();
        }

        try {
            robot = new Robot();
            cursor = ImageIO.read(getClass().getResourceAsStream("cursor.png"));
        } catch (IOException e) {
            throw new RuntimeException(e);
        } catch (AWTException e) {
            throw new RuntimeException(e);
        }

        writer = new Writer();
        writer.start();
    }

    public void start() {
        recording = true;
    }

    public void stop() {
        recording = false;

        for (int i = 0; i < 5; ++i) {
            try {
                writer.interrupt();
                writer.join(1000);
                break;
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
        }
    }

    public void captureScreen() {
        if (recording) {

            // Only 30 fps
            if (nextFrame % 2 == 0) {
                // NOTE: Division by two for consecutive frame numbers
                String fileName = String.format("%s/frame%04d.jpg", this.path, nextFrame / 2);
                Dimension screenSize = Toolkit.getDefaultToolkit().getScreenSize();
                try {
                    Point location = MouseInfo.getPointerInfo().getLocation();
                    BufferedImage screenCapture = robot.createScreenCapture(new java.awt.Rectangle(0, 0, screenSize.width, screenSize.height));
                    Graphics g = screenCapture.getGraphics();
                    g.drawImage(cursor, location.x - 4, location.y - 6, null);
                    g.dispose();

                    queue.put(new Entry(screenCapture, fileName));

                } catch (Exception e) {
                    e.printStackTrace();
                    MessageDialog.openError(null, "Failed to record", e.getMessage());
                    recording = false;
                }
            }

            nextFrame++;
        }
    }

}
