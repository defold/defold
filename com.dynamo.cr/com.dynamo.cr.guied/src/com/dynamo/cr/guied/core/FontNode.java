package com.dynamo.cr.guied.core;

import java.awt.FontFormatException;
import java.awt.image.BufferedImage;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.file.Paths;

import javax.media.opengl.GL2;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.swt.graphics.Image;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.FontResourceResolver;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.guied.Activator;
import com.dynamo.cr.properties.NotEmpty;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.Property.EditorType;
import com.dynamo.cr.sceneed.core.FontRendererHandle;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.Identifiable;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.validators.Unique;
import com.dynamo.render.proto.Font;
import com.dynamo.render.proto.Font.FontMap;
import com.google.protobuf.TextFormat;

@SuppressWarnings("serial")
public class FontNode extends Node implements Identifiable {

    private static Logger logger = LoggerFactory.getLogger(FontNode.class);

    @Property
    @NotEmpty(severity = IStatus.ERROR)
    @Unique(scope = FontNode.class, base = FontsNode.class)
    private String id;

    @Property(editorType = EditorType.RESOURCE, extensions = { "font" })
    private String font;

    private transient FontRendererHandle fontRendererHandle = null;
    private transient FontRendererHandle defaultFontRendererHandle = null;

    public FontNode() {
        this.font = "";
        this.id = "";
        updateFontRendererHandle(); // Will load default font
    }

    public FontNode(String font) {
        this.font = font;
        this.id = new Path(font).removeFileExtension().lastSegment();
        updateFontRendererHandle();
    }

    public void dispose(GL2 gl) {
        super.dispose(gl);
        if (this.fontRendererHandle != null) {
            this.fontRendererHandle.clear(gl);
        }
        if (this.defaultFontRendererHandle != null) {
            this.defaultFontRendererHandle.clear(gl);
        }
    }

    @Override
    public void setModel(ISceneModel model) {
        super.setModel(model);
        updateFontRendererHandle();
    }

    private void loadFont(String fontPath, FontRendererHandle handle) throws CoreException, IOException {
        IProject project = EditorUtil.getProject();
        final IContainer contentRoot = EditorUtil.getContentRoot(project);
        IFile fontFile = contentRoot.getFile(new Path(fontPath));
        InputStream is = fontFile.getContents();
        Font.FontDesc fontDesc = null;

        // Parse font description
        try {
            Reader reader = new InputStreamReader(is);
            Font.FontDesc.Builder fontDescBuilder = Font.FontDesc.newBuilder();
            TextFormat.merge(reader, fontDescBuilder);
            fontDesc = fontDescBuilder.build();
        } finally {
            is.close();
        }

        if (fontDesc == null) {
            throw new IOException("Could not load font: " + fontPath);
        }

        // Compile to FontMap
        FontMap.Builder fontMapBuilder = FontMap.newBuilder();
        final IFile fontInputFile = contentRoot.getFile(new Path(fontDesc.getFont()));
        final String searchPath = new Path(fontDesc.getFont()).removeLastSegments(1).toString();
        BufferedImage image;
        Fontc fontc = new Fontc();

        try {
            image = fontc.compile(fontInputFile.getContents(), fontDesc, fontMapBuilder, new FontResourceResolver() {

                @Override
                public InputStream getResource(String resourceName)
                        throws FileNotFoundException {

                    String resPath = Paths.get(searchPath, resourceName).toString();
                    IFile resFile = contentRoot.getFile(new Path(resPath));

                    try {
                        return resFile.getContents();
                    } catch (CoreException e) {
                        throw new FileNotFoundException(e.getMessage());
                    }
                }
            });
        } catch (FontFormatException e) {
            throw new IOException(e.getMessage());
        }

        handle.setFont(fontMapBuilder.build(), image, fontc.getInputFormat());

    }

    private void updateFontRendererHandle() {

        if (getModel() == null)
            return;

        // Reset current handle
        this.fontRendererHandle = null;

        if (!this.font.isEmpty()) {
            try {
                FontRendererHandle tmpFontRendererHandle = new FontRendererHandle();
                loadFont(this.font, tmpFontRendererHandle);
                this.fontRendererHandle = tmpFontRendererHandle;
                this.defaultFontRendererHandle = null;
            } catch (CoreException e) {
                logger.error("Could not load font " + this.font, e);
            } catch (IOException e) {
                logger.error("Could not load font " + this.font, e);
            }
        }

        if (this.fontRendererHandle == null && this.defaultFontRendererHandle == null) {
            this.defaultFontRendererHandle = new FontRendererHandle();

            try {
                this.loadFont("/builtins/fonts/system_font.font", this.defaultFontRendererHandle);
            } catch (CoreException e) {
                logger.error("Could not load default font.", e);
            } catch (IOException e) {
                logger.error("Could not load default font.", e);
            }
        }


    }

    public FontRendererHandle getFontRendererHandle() {
        return this.fontRendererHandle;
    }

    public FontRendererHandle getDefaultFontRendererHandle() {
        return this.defaultFontRendererHandle;
    }

    @Override
    public String getId() {
        return this.id;
    }

    @Override
    public void setId(String id) {
        this.id = id;
    }

    public String getFont() {
        return this.font;
    }

    public void setFont(String font) {
        this.font = font;
        updateFontRendererHandle();
    }

    @Override
    public String toString() {
        return this.id;
    }

    @Override
    public Image getIcon() {
        return Activator.getDefault().getImageRegistry().get(Activator.FONT_IMAGE_ID);
    }
}
