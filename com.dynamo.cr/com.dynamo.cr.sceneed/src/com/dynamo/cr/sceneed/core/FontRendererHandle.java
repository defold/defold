package com.dynamo.cr.sceneed.core;

import java.awt.image.BufferedImage;
import java.util.HashMap;

import javax.media.opengl.GL2;
import javax.media.opengl.GLProfile;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.InputFontFormat;
import com.dynamo.render.proto.Font.FontMap;
import com.jogamp.opengl.util.texture.Texture;
import com.jogamp.opengl.util.texture.awt.AWTTextureIO;

public class FontRendererHandle {

    private static Logger logger = LoggerFactory.getLogger(FontRendererHandle.class);

    private FontMap fontMap = null;
    private BufferedImage image = null;
    private Fontc.InputFontFormat inputFormat = InputFontFormat.FORMAT_TRUETYPE;
    private Texture texture = null;
    private boolean reloadTexture = false;
    private boolean awaitClear = false;

    private HashMap<Integer, FontMap.Glyph> glyphLookup;

    private void generateTexture(GL2 gl) {
        if (this.texture == null) {
            this.texture = AWTTextureIO.newTexture(GLProfile.getGL2GL3(), this.image, true);
        } else {
            this.texture.updateImage(gl, AWTTextureIO.newTextureData(GLProfile.getGL2GL3(), this.image, true));
        }
    }

    public void clear(GL2 gl) {
        this.image = null;
        if (this.texture != null) {
            this.texture.destroy(gl);
            this.texture = null;
        }
        this.awaitClear = false;
    }

    public void setFont(FontMap fontMap, BufferedImage image, Fontc.InputFontFormat inputFormat) {
        this.fontMap = fontMap;
        this.image = image;
        this.inputFormat = inputFormat;
        this.reloadTexture = true;

        // build glyph lookup table
        this.glyphLookup = new HashMap<Integer, FontMap.Glyph>();
        for (int i = 0; i < this.fontMap.getGlyphsCount(); i++) {
            FontMap.Glyph g = this.fontMap.getGlyphs(i);
            this.glyphLookup.put(g.getCharacter(), g);
        }
    }

    public Texture getTexture(GL2 gl) {

        try {

            if (this.reloadTexture) {
                this.reloadTexture = false;

                if (this.texture != null) {
                    this.texture.destroy(gl);
                }
                generateTexture(gl);
            }

        } catch (Throwable e) {
            logger.error("Could not create a font texture.", e);
        }

        return this.texture;
    }

    public FontMap.Glyph getGlyph(int character) {
        FontMap.Glyph g = this.glyphLookup.get(character);
        if (g == null) {
            g = this.glyphLookup.get(126); // Fallback to ~
        }
        return g;
    }

    public void setDeferredClear() {
        this.awaitClear = true;
    }

    public boolean getDeferredClear() {
        return this.awaitClear;
    }

    public boolean isValid() {
        return (this.image != null && this.fontMap != null);
    }

    public FontMap getFontMap() {
        return fontMap;
    }

    public Fontc.InputFontFormat getInputFormat() {
        return inputFormat;
    }
}
