package com.dynamo.cr.guieditor.util;

import java.awt.geom.Rectangle2D;

import javax.media.opengl.GL2;

public class DrawUtil {
    public static void fillRectangle(GL2 gl, double x, double y, double w, double h) {
        gl.glBegin(GL2.GL_QUADS);
        gl.glVertex2d(x, y);
        gl.glVertex2d(x + w, y);
        gl.glVertex2d(x + w, y + h);
        gl.glVertex2d(x, y + h);
        gl.glEnd();
    }

    public static void drawRectangle(GL2 gl, double x, double y, double w, double h) {
        gl.glBegin(GL2.GL_LINE_LOOP);
        gl.glVertex2d(x, y);
        gl.glVertex2d(x + w, y);
        gl.glVertex2d(x + w, y + h);
        gl.glVertex2d(x, y + h);
        gl.glEnd();
    }

    public static void drawRectangle(GL2 gl, Rectangle2D.Double rectangle) {
        drawRectangle(gl, rectangle.x, rectangle.y, rectangle.width, rectangle.height);
    }

    public static void fillRectangle(GL2 gl, Rectangle2D.Double rectangle) {
        fillRectangle(gl, rectangle.x, rectangle.y, rectangle.width, rectangle.height);
    }

}
