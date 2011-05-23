package com.dynamo.cr.guieditor.scene;

import java.awt.FontFormatException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

import javax.media.opengl.GLException;

import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.property.IPropertyObjectWorld;
import com.dynamo.cr.guieditor.property.Property;
import com.dynamo.cr.guieditor.property.PropertyIntrospectorSource;
import com.dynamo.cr.guieditor.render.GuiRenderer;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.dynamo.render.proto.Font;
import com.google.protobuf.TextFormat;

public class GuiScene implements IPropertyObjectWorld, IAdaptable {

    @Property(commandFactory = UndoableCommandFactory.class)
    private int referenceWidth;

    @Property(commandFactory = UndoableCommandFactory.class)
    private int referenceHeight;

    private SceneDesc sceneDesc;
    private ArrayList<GuiNode> nodes;
    private ArrayList<EditorTextureDesc> textures;
    private ArrayList<EditorFontDesc> fonts;
    private ArrayList<IGuiSceneListener> listeners = new ArrayList<IGuiSceneListener>();
    private IGuiEditor editor;

    private PropertyIntrospectorSource<GuiScene, GuiScene> propertySource;

    public GuiScene(IGuiEditor editor, SceneDesc sceneDesc) {
        this.editor = editor;
        this.sceneDesc = sceneDesc;
        this.referenceWidth = sceneDesc.getReferenceWidth();
        this.referenceHeight = sceneDesc.getReferenceHeight();

        nodes = new ArrayList<GuiNode>();
        for (NodeDesc nodeDesc : sceneDesc.getNodesList()) {
            if (nodeDesc.getType() == Type.TYPE_BOX) {
                BoxGuiNode node = new BoxGuiNode(this, nodeDesc);
                this.nodes.add(node);
            }
            else if (nodeDesc.getType() == Type.TYPE_TEXT) {
                TextGuiNode node = new TextGuiNode(this, nodeDesc);
                this.nodes.add(node);
            }
        }

        textures = new ArrayList<EditorTextureDesc>();
        for (TextureDesc texture : sceneDesc.getTexturesList()) {
            textures.add(new EditorTextureDesc(this, texture));
        }

        fonts = new ArrayList<EditorFontDesc>();
        for (FontDesc font : sceneDesc.getFontsList()) {
            fonts.add(new EditorFontDesc(this, font));
        }
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<GuiScene, GuiScene>(this, this);
            }
            return this.propertySource;
        }
        return null;
    }

    private void loadTTF(String alias, Font.FontDesc fontDesc, IContainer contentRoot, GuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        String ttfFileName = fontDesc.getFont();
        IFile ttfFile = contentRoot.getFile(new Path(ttfFileName));
        renderer.loadFont(alias, ttfFile, fontDesc.getSize());
    }

    private void loadFont(EditorFontDesc guiFont, IContainer contentRoot, GuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        String fontFileName = guiFont.getFont();
        IFile fontFile = contentRoot.getFile(new Path(fontFileName));

        Font.FontDesc.Builder fontDescBuilder = Font.FontDesc.newBuilder();
        InputStream is = fontFile.getContents();
        try {
            Reader reader = new InputStreamReader(is);
            TextFormat.merge(reader, fontDescBuilder);
            Font.FontDesc fontDesc = fontDescBuilder.build();
            loadTTF(guiFont.getName(), fontDesc, contentRoot, renderer);

        } finally {
            is.close();
        }
    }

    private void loadTexture(EditorTextureDesc textureDesc, IContainer contentRoot,
            GuiRenderer renderer) throws GLException, IOException, CoreException {
        IFile textureFile = contentRoot.getFile(new Path(textureDesc.getTexture()));
        renderer.loadTexture(textureDesc.getName(), textureFile);
    }

    public void loadRenderResources(IProgressMonitor monitor, IContainer contentRoot, GuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        monitor.beginTask("Loading resources...", fonts.size() + textures.size());
        for (EditorFontDesc guiFont : fonts) {
            monitor.subTask(String.format("Loading %s", guiFont.getFont()));
            try {
                loadFont(guiFont, contentRoot, renderer);
            }
            catch(Throwable e) {
                // TODO: WE MUST HANDLE THIS...
                e.printStackTrace();
            }
            monitor.worked(1);
        }

        for (EditorTextureDesc textureDesc : textures) {
            monitor.subTask(String.format("Loading %s", textureDesc.getTexture()));
            try {
                loadTexture(textureDesc, contentRoot, renderer);
            }
            catch(Throwable e) {
                // TODO: WE MUST HANDLE THIS...
                e.printStackTrace();
            }
            monitor.worked(1);
        }
    }

    public void addPropertyChangeListener(IGuiSceneListener listener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener);
        }
    }

    public void removePropertyChangeListener(IGuiSceneListener listener) {
        listeners.remove(listener);
    }

    public void draw(DrawContext drawContext) {
        for (GuiNode node : nodes) {
            node.draw(drawContext);
        }
    }

    public int getReferenceWidth() {
        return referenceWidth;
    }

    public void setReferenceWidth(int referenceWidth) {
        this.referenceWidth = referenceWidth;
    }

    public int getReferenceHeight() {
        return referenceHeight;
    }

    public void setReferenceHeight(int referenceHeight) {
        this.referenceHeight = referenceHeight;
    }

    public void drawSelect(DrawContext drawContext) {
        int nextName = 0;
        GuiRenderer renderer = drawContext.getRenderer();
        for (GuiNode node : nodes) {
            renderer.setName(nextName++);
            node.drawSelect(drawContext);
            renderer.clearName();
        }
    }

    public GuiNode getNode(int index) {
        return nodes.get(index);
    }

    public void propertyChanged(GuiNode node, String property) {
        for (IGuiSceneListener listener : listeners) {
            listener.propertyChanged(node, property);
        }
    }

    public IGuiEditor getEditor() {
        return editor;
    }

    public SceneDesc buildSceneDesc() {
        SceneDesc.Builder builder = SceneDesc.newBuilder().mergeFrom(sceneDesc);
        builder.clearNodes();
        for (GuiNode node : nodes) {
            NodeDesc nodeDesc = node.buildNodeDesc();
            builder.addNodes(nodeDesc);
        }

        builder.clearTextures();
        for (EditorTextureDesc texture : textures) {
            builder.addTextures(texture.buildDesc());
        }

        builder.clearFonts();
        for (EditorFontDesc font : fonts) {
            builder.addFonts(font.buildDesc());
        }

        return builder.build();
    }

    public void addNode(GuiNode node) {
        nodes.add(node);
    }

    public void removeNode(GuiNode node) {
        boolean found = nodes.remove(node);
        assert found;
        for (IGuiSceneListener listener : listeners) {
            listener.nodeRemoved(node);
        }

    }

    public List<EditorFontDesc> getFonts() {
        return Collections.unmodifiableList(this.fonts);
    }

    public List<EditorTextureDesc> getTextures() {
        return Collections.unmodifiableList(this.textures);
    }

    public int getNodeCount() {
        return nodes.size();
    }

    public GuiNode[] getNodes() {
        return nodes.toArray(new GuiNode[nodes.size()]);
    }

    public String getUniqueId() {
        int i = 1;
        while (true) {
            String id = String.format("unnamed.%d", i++);
            boolean found = false;
            for (GuiNode node : nodes) {
                if (id.equals(node.getId())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return id;
            }
        }
    }

    public String getUniqueTextureName(String name) {
        int i = 0;
        while (true) {
            String newName = String.format("%s.%d", name, i);
            if (i == 0)
                newName = name;

            boolean found = false;
            for (EditorTextureDesc texture : textures) {
                if (newName.equals(texture.getName())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return newName;
            }
            ++i;
        }
    }

    public String getUniqueFontName(String name) {
        int i = 0;
        while (true) {
            String newName = String.format("%s.%d", name, i);
            if (i == 0)
                newName = name;

            boolean found = false;
            for (EditorFontDesc font : fonts) {
                if (newName.equals(font.getName())) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                return newName;
            }
            ++i;
        }
    }

    public void addTexture(String name, String texture) {
        textures.add(new EditorTextureDesc(this, TextureDesc.newBuilder().setName(name).setTexture(texture).build()));
    }

    public void addFont(String name, String font) {
        fonts.add(new EditorFontDesc(this, FontDesc.newBuilder().setName(name).setFont(font).build()));
    }

    public void removeTexture(String name) {
        for (EditorTextureDesc textureDesc : textures) {
            if (textureDesc.getName().equals(name)) {
                textures.remove(textureDesc);
                return;
            }
        }
        throw new RuntimeException("Texture " + name + " not found");
    }

    public void removeFont(String name) {
        for (EditorFontDesc fontDesc : fonts) {
            if (fontDesc.getName().equals(name)) {
                fonts.remove(fontDesc);
                return;
            }
        }
        throw new RuntimeException("Font " + name + " not found");
    }

    public EditorTextureDesc getTextureFromPath(String relativePath) {
        for (EditorTextureDesc textureDesc : textures) {
            if (textureDesc.getTexture().equals(relativePath)) {
                return textureDesc;
            }
        }
        return null;
    }

    public EditorFontDesc getFontFromPath(String relativePath) {
        for (EditorFontDesc fontDesc : fonts) {
            if (fontDesc.getFont().equals(relativePath)) {
                return fontDesc;
            }
        }
        return null;
    }

}
