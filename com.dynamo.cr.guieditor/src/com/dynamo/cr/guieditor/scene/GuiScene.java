package com.dynamo.cr.guieditor.scene;

import java.awt.FontFormatException;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.media.opengl.GLException;
import javax.vecmath.Vector4d;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.ui.services.IDisposable;
import org.eclipse.ui.views.properties.IPropertySource;

import com.dynamo.cr.guieditor.Activator;
import com.dynamo.cr.guieditor.DrawContext;
import com.dynamo.cr.guieditor.IGuiEditor;
import com.dynamo.cr.guieditor.render.GuiFontResource;
import com.dynamo.cr.guieditor.render.GuiTextureResource;
import com.dynamo.cr.guieditor.render.IGuiRenderer;
import com.dynamo.cr.properties.IPropertyObjectWorld;
import com.dynamo.cr.properties.Property;
import com.dynamo.cr.properties.PropertyIntrospectorSource;
import com.dynamo.gui.proto.Gui.NodeDesc;
import com.dynamo.gui.proto.Gui.NodeDesc.BlendMode;
import com.dynamo.gui.proto.Gui.NodeDesc.Type;
import com.dynamo.gui.proto.Gui.SceneDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.FontDesc;
import com.dynamo.gui.proto.Gui.SceneDesc.TextureDesc;
import com.dynamo.render.proto.Font;
import com.google.protobuf.TextFormat;

public class GuiScene implements IPropertyObjectWorld, IAdaptable, IResourceChangeListener, IDisposable {

    @Property(commandFactory = UndoableCommandFactory.class, isResource = true)
    private String script;

    private SceneDesc sceneDesc;
    private ArrayList<GuiNode> nodes;
    // Use to preserve node order, eg when delete/undo
    private Map<GuiNode, Integer> nodeToIndex = new HashMap<GuiNode, Integer>();

    private List<EditorTextureDesc> textures;
    // Use to preserve node order, eg when delete/undo
    private Map<EditorTextureDesc, Integer> textureToIndex = new HashMap<EditorTextureDesc, Integer>();

    private List<EditorFontDesc> fonts;
    // Use to preserve node order, eg when delete/undo
    private Map<EditorFontDesc, Integer> fontToIndex = new HashMap<EditorFontDesc, Integer>();

    private ArrayList<IGuiSceneListener> listeners = new ArrayList<IGuiSceneListener>();
    private IGuiEditor editor;

    private PropertyIntrospectorSource<GuiScene, GuiScene> propertySource;

    // NOTE: This can contain removed items. It used for reloading. Better be safe than sorry. :-)
    private Set<IResource> referredResources = new HashSet<IResource>();

    private RenderResourceCollection renderResourceCollection = new RenderResourceCollection();

    private boolean isDisposed;

    public GuiScene(IGuiEditor editor, SceneDesc sceneDesc) {
        this.editor = editor;
        this.sceneDesc = sceneDesc;
        this.script = sceneDesc.getScript();

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

        ResourcesPlugin.getWorkspace().addResourceChangeListener(this);
    }

    @Override
    protected void finalize() throws Throwable {
        super.finalize();
        if (!isDisposed) {
            System.err.println("ERROR: GuiScene not explicitly disposed.");
        }
    }

    @Override
    public void dispose() {
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        isDisposed = true;
    }

    public IContainer getContentRoot() {
        return editor.getContentRoot();
    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertySource.class) {
            if (this.propertySource == null) {
                this.propertySource = new PropertyIntrospectorSource<GuiScene, GuiScene>(this, this, getContentRoot());
            }
            return this.propertySource;
        }
        return null;
    }

    private GuiFontResource loadTTF(String alias, Font.FontDesc fontDesc, IContainer contentRoot, IGuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        String ttfFileName = fontDesc.getFont();
        IFile ttfFile = contentRoot.getFile(new Path(ttfFileName));

        ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
        IOUtils.copy(ttfFile.getContents(), output);

        return new GuiFontResource(output.toByteArray(), fontDesc.getSize());
    }

    private void loadFont(EditorFontDesc guiFont, IContainer contentRoot, IGuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        String fontFileName = guiFont.getFont();
        IFile fontFile = contentRoot.getFile(new Path(fontFileName));
        long lastModified = fontFile.getModificationStamp();
        if (lastModified == guiFont.getLastModified())
            return;
        guiFont.setLastModified(lastModified);

        referredResources.add(fontFile);

        Font.FontDesc.Builder fontDescBuilder = Font.FontDesc.newBuilder();
        InputStream is = fontFile.getContents();
        try {
            Reader reader = new InputStreamReader(is);
            TextFormat.merge(reader, fontDescBuilder);
            Font.FontDesc fontDesc = fontDescBuilder.build();
            GuiFontResource fontResource = loadTTF(guiFont.getName(), fontDesc, contentRoot, renderer);
            guiFont.setFontResource(fontResource);

        } finally {
            is.close();
        }
    }

    private void loadTexture(EditorTextureDesc textureDesc, IContainer contentRoot,
            IGuiRenderer renderer) throws GLException, IOException, CoreException {
        IFile textureFile = contentRoot.getFile(new Path(textureDesc.getTexture()));

        if (!textureFile.exists()) {
            textureDesc.setTextureResource(null);
            return;
        }

        long lastModified = textureFile.getModificationStamp();
        if (lastModified == textureDesc.getLastModified())
            return;
        textureDesc.setLastModified(lastModified);

        referredResources.add(textureFile);

        ByteArrayOutputStream output = new ByteArrayOutputStream(1024 * 128);
        IOUtils.copy(textureFile.getContents(), output);

        GuiTextureResource textureResource = new GuiTextureResource(output.toByteArray(), textureFile.getFileExtension());
        textureDesc.setTextureResource(textureResource);
    }

    public RenderResourceCollection getRenderResourceCollection() {
        renderResourceCollection = new RenderResourceCollection();
        for (EditorFontDesc guiFont : fonts) {
            renderResourceCollection.addFontResource(guiFont.getName(), guiFont.getFontResource());
        }

        for (EditorTextureDesc textureDesc : textures) {
            renderResourceCollection.addTextureResource(textureDesc.getName(), textureDesc.getTextureResource());
        }

        return renderResourceCollection;
    }

    public void loadRenderResources(IProgressMonitor monitor, IContainer contentRoot, IGuiRenderer renderer) throws CoreException, IOException, FontFormatException {
        monitor.beginTask("Loading resources...", fonts.size() + textures.size());
        for (EditorFontDesc guiFont : fonts) {
            monitor.subTask(String.format("Loading %s", guiFont.getFont()));
            try {
                loadFont(guiFont, contentRoot, renderer);
            }
            catch(Throwable e) {
                Activator.logException(e);
            }
            monitor.worked(1);
        }

        for (EditorTextureDesc textureDesc : textures) {
            monitor.subTask(String.format("Loading %s", textureDesc.getTexture()));
            try {
                loadTexture(textureDesc, contentRoot, renderer);
            }
            catch(Throwable e) {
                Activator.logException(e);
            }
            monitor.worked(1);
        }
    }

    public void addGuiSceneListener(IGuiSceneListener listener) {
        if (!listeners.contains(listener)) {
            listeners.add(listener);
        }
    }

    public void removeGuiSceneListener(IGuiSceneListener listener) {
        listeners.remove(listener);
    }

    public void draw(DrawContext drawContext) {
        for (GuiNode node : nodes) {
            node.draw(drawContext);
        }
    }

    public void drawPivot(DrawContext drawContext) {
        IGuiRenderer renderer = drawContext.getRenderer();
        final double s = 3;
        double s2 = 4;

        for (GuiNode node : nodes) {
            if (drawContext.isSelected(node)) {
                Vector4d position = node.getPosition();
                double x = position.getX();
                double y = position.getY();
                renderer.drawQuad(x - s2, y - s2, x + s2, y + s2, 0.3, 0.3, 0.3, 1, BlendMode.BLEND_MODE_ALPHA, null);
                renderer.drawQuad(x - s, y - s, x + s, y + s, 99/255.0, 116/255.0, 220/255.0, 1, BlendMode.BLEND_MODE_ALPHA, null);
            }
        }
    }

    public String getScript() {
        return script;
    }

    public void setScript(String script) {
        this.script = script;
    }

    public void drawSelect(DrawContext drawContext) {
        int nextName = 0;
        IGuiRenderer renderer = drawContext.getRenderer();
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

        builder.setScript(script);

        return builder.build();
    }

    public void removeNodes(List<GuiNode> nodesToRemove) {
        removeGeneric(nodes, nodeToIndex, nodesToRemove);
    }

    private static <T> void addGeneric(List<T> collection, final Map<T, Integer> toIndex, List<T> toAdd) {
        ArrayList<T> tmp = new ArrayList<T>(toAdd);
        Collections.sort(tmp, new Comparator<T>() {
            @Override
            public int compare(T o1, T o2) {
                Integer index1 = toIndex.get(o1);
                Integer index2 = toIndex.get(o2);
                int i1 = 0, i2 = 0;
                if (index1 != null)
                    i1 = index1;

                if (index2 != null)
                    i2 = index2;

                return i1 - i2;
            }
        });

        for (T o : tmp) {
            Integer index = toIndex.get(o);
            if (index != null && index < collection.size()) {
                // Try to preserve node order.
                collection.add(index, o);

            } else {
                collection.add(o);
            }
        }
    }

    private static <T> void removeGeneric(final List<T> collection, final Map<T, Integer> toIndex, List<T> toRemove) {
        ArrayList<T> tmp = new ArrayList<T>(toRemove);
        Collections.sort(tmp, new Comparator<T>() {
            @Override
            public int compare(T o1, T o2) {
                int i1 = collection.indexOf(o1);
                int i2 = collection.indexOf(o2);
                return i2 - i1;
            }
        });

        for (T o : tmp) {

            int index = collection.indexOf(o);
            assert index != -1;
            toIndex.put(o, index);
            collection.remove(index);
        }
    }

    public void addNodes(List<GuiNode> nodesToAdd) {
        addGeneric(nodes, nodeToIndex, nodesToAdd);
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
            for (EditorTextureDesc textureDesc : textures) {
                if (textureDesc.getName().equals(newName)) {
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
            for (EditorFontDesc fontDesc : fonts) {
                if (fontDesc.getName().equals(newName)) {
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

    public EditorTextureDesc addTexture(String name, String texture) {
        EditorTextureDesc textureDesc = new EditorTextureDesc(this, TextureDesc.newBuilder().setName(name).setTexture(texture).build());
        textures.add(textureDesc);
        try {
            loadRenderResources(new NullProgressMonitor(), editor.getContentRoot(), editor.getRenderer());
        } catch (Throwable e) {
            Activator.logException(e);
        }
        return textureDesc;
    }

    public EditorFontDesc addFont(String name, String font) {
        EditorFontDesc fontDesc = new EditorFontDesc(this, FontDesc.newBuilder().setName(name).setFont(font).build());
        fonts.add(fontDesc);
        try {
            loadRenderResources(new NullProgressMonitor(), editor.getContentRoot(), editor.getRenderer());
        } catch (Throwable e) {
            Activator.logException(e);
        }
        return fontDesc;
    }

    public void addFonts(List<EditorFontDesc> fontsToAdd) {
        addGeneric(fonts, fontToIndex, fontsToAdd);
    }

    public void removeFonts(List<EditorFontDesc> fontsToRemove) {
        removeGeneric(fonts, fontToIndex, fontsToRemove);
    }

    public void addTextures(List<EditorTextureDesc> texturesToAdd) {
        addGeneric(textures, textureToIndex, texturesToAdd);
    }

    public void removeTextures(List<EditorTextureDesc> texturesToRemove) {
        removeGeneric(textures, textureToIndex, texturesToRemove);
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

    public List<GuiNode> bringForward(List<GuiNode> nodesForward) {
        ArrayList<GuiNode> sortedNodesForward = new ArrayList<GuiNode>(nodesForward);
        Collections.sort(sortedNodesForward, new Comparator<GuiNode>() {
            @Override
            public int compare(GuiNode o1, GuiNode o2) {
                int i1 = nodes.indexOf(o1);
                int i2 = nodes.indexOf(o2);
                return i2 - i1;
            }
        });

        ArrayList<GuiNode> movedNodes = new ArrayList<GuiNode>(nodesForward.size());
        int roof = nodes.size() - 1;
        for (GuiNode node : sortedNodesForward) {
            int i = nodes.indexOf(node);
            if (i != roof) {
                nodes.remove(i);
                nodes.add(i + 1, node);
                movedNodes.add(node);
            }
            else {
                --roof;
            }
        }

        return movedNodes;
    }

    public ArrayList<GuiNode> sendBackward(List<GuiNode> nodesBackward) {
        ArrayList<GuiNode> sortedNodesBackward = new ArrayList<GuiNode>(nodesBackward);
        Collections.sort(sortedNodesBackward, new Comparator<GuiNode>() {
            @Override
            public int compare(GuiNode o1, GuiNode o2) {
                int i1 = nodes.indexOf(o1);
                int i2 = nodes.indexOf(o2);
                return i1 - i2;
            }
        });

        ArrayList<GuiNode> movedNodes = new ArrayList<GuiNode>(nodesBackward.size());
        int floor = 0;
        for (GuiNode node : sortedNodesBackward) {
            int i = nodes.indexOf(node);
            if (i != floor) {
                nodes.remove(i);
                nodes.add(i - 1, node);
                movedNodes.add(node);
            } else {
                ++floor;
            }
        }

        return movedNodes;
    }

    public int getNodeIndex(GuiNode node) {
        return nodes.indexOf(node);
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        final boolean doReload[] = new boolean[] { false };
        try {
            IResourceDelta delta = event.getDelta();
            if (delta == null)
                return;
            delta.accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    // Only reload if change is added, removed or content-change
                    if ((delta.getKind() & (IResourceDelta.ADDED | IResourceDelta.REMOVED)) != 0
                            || ((delta.getKind() & IResourceDelta.CHANGED) == IResourceDelta.CHANGED
                                    && (delta.getFlags() & IResourceDelta.CONTENT) == IResourceDelta.CONTENT)) {
                        IResource resource = delta.getResource();
                        if (resource != null) {
                            if (referredResources.contains(resource)) {
                                doReload[0] = true;
                                return false;
                            }
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            Activator.logException(e);
        }

        try {
            if (doReload[0]) {
                loadRenderResources(new NullProgressMonitor(), editor.getContentRoot(), editor.getRenderer());

                for (IGuiSceneListener listener : listeners) {
                    listener.resourcesChanged();
                }
            }
        } catch (Throwable e) {
            Activator.logException(e);
        }
    }
}
