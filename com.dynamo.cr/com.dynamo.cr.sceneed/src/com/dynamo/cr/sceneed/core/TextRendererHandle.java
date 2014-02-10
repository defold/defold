package com.dynamo.cr.sceneed.core;

import java.awt.Font;
import java.io.ByteArrayInputStream;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.jogamp.opengl.util.awt.TextRenderer;

public class TextRendererHandle {

    private static Logger logger = LoggerFactory.getLogger(TextRendererHandle.class);

    private byte[] fontData;
    private int size;
    private TextRenderer textRenderer;
    private boolean reloadRenderer = false;

    public void setFont(byte[] fontData, int size) {
        this.fontData = fontData;
        this.size = size;
        this.reloadRenderer = true;
    }

    public TextRenderer getTextRenderer() {
        if (this.reloadRenderer) {
            this.reloadRenderer = false;
            try {
                Font font = Font.createFont(Font.TRUETYPE_FONT, new ByteArrayInputStream(this.fontData));
                font = font.deriveFont(Font.PLAIN, this.size);
                if (this.textRenderer != null) {
                    this.textRenderer.dispose();
                }
                this.textRenderer = new TextRenderer(font, true, true);
            } catch (Throwable e) {
                logger.error("Could not create a font renderer.", e);
            }
        }
        return this.textRenderer;
    }

    public int getSize() {
        return this.size;
    }
}
