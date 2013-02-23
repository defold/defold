package com.dynamo.cr.guieditor.render;

import java.awt.Font;
import java.io.ByteArrayInputStream;

import com.dynamo.cr.guieditor.Activator;
import com.jogamp.opengl.util.awt.TextRenderer;

public class GuiFontResource {

    private byte[] fontData;
    private int size;
    private TextRenderer textRenderer;

    public GuiFontResource(byte[] fontData, int size) {
        this.fontData = fontData;
        this.size = size;
    }

    private void createDeferred() {
        if (fontData != null) {
            try {
                Font font = Font.createFont(Font.TRUETYPE_FONT, new ByteArrayInputStream(fontData));
                font = font.deriveFont(Font.PLAIN, size);
                textRenderer = new TextRenderer(font, true, true);
            } catch (Throwable e) {
                Activator.logException(e);
            } finally {
                fontData = null;
            }
        }
    }

    public int getSize() {
        return size;
    }

    public TextRenderer getTextRenderer() {
        if (fontData != null) {
            createDeferred();
        }
        return textRenderer;
    }
}
