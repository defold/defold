package com.dynamo.cr.sceneed.core;

import java.awt.FontFormatException;
import java.awt.image.BufferedImage;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.file.Paths;
import java.util.HashMap;
import java.util.Map;

import javax.media.opengl.GL2;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IProject;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Path;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.widgets.Display;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.bob.font.Fontc;
import com.dynamo.bob.font.Fontc.FontResourceResolver;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;
import com.dynamo.cr.editor.ui.IImageProvider;
import com.dynamo.cr.properties.Entity;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.properties.PropertyIntrospector;
import com.dynamo.cr.properties.PropertyIntrospectorModel;
import com.dynamo.cr.sceneed.core.operations.SelectOperation;
import com.dynamo.render.proto.Font;
import com.dynamo.render.proto.Font.FontMap;
import com.google.inject.Inject;
import com.google.protobuf.TextFormat;

@Entity(commandFactory = SceneUndoableCommandFactory.class)
public class SceneModel implements IAdaptable, IOperationHistoryListener, ISceneModel {

    private static Logger logger = LoggerFactory.getLogger(SceneModel.class);
    private final IModelListener listener;
    private final IOperationHistory history;
    private final IUndoContext undoContext;
    private final IContainer contentRoot;
    private final ILoaderContext loaderContext;
    private final IImageProvider imageProvider;
    private ProjectProperties projectProperties;
    private DelayedUpdateStatus delayedUpdateStatus = new DelayedUpdateStatus();

    private Node root;
    private IStructuredSelection selection;
    private int undoRedoCounter;

    // TODO These caches are partial optimizations for loading collections.
    // Images and textures took the most time to load and create.
    // It should probably be fixed by a general resource management.
    // Issue for this: https://defold.fogbugz.com/default.asp?1052
    private final Map<String, BufferedImage> imageCache = new HashMap<String, BufferedImage>();
    private final Map<String, TextureHandle> textureCache = new HashMap<String, TextureHandle>();
    private final Map<String, FontRendererHandle> fontCache = new HashMap<String, FontRendererHandle>();
    private FontRendererHandle defaultFontRendererHandle = null;

    private static PropertyIntrospector<SceneModel, SceneModel> introspector = new PropertyIntrospector<SceneModel, SceneModel>(SceneModel.class);

    // This is a hack to let tests disable the delayed status update when running on OSX (which has an active Display)
    public static void setUpdateStatusDelay(int delay) {
        DELAY = delay;
    }
    private static int DELAY = 100;

    private class DelayedUpdateStatus implements Runnable {

        long start = 0;
        boolean delay = false;
        boolean updateRequested = false;

        @Override
        public void run() {
            long now = System.currentTimeMillis();
            if (!delay || now - start > DELAY) {
                updateRequested = false;
                root.updateStatus();
                listener.stateChanged(selection, isDirty());
            } else {
                Display.getCurrent().asyncExec(this);
            }
        }

        public void update(boolean delay) {
            // Update is constantly postponed when update(true) is invoked
            this.start = System.currentTimeMillis();
            this.delay = delay;
            if (!updateRequested) {
                updateRequested = true;
                Display display = Display.getCurrent();
                if (DELAY > 0 && display != null) {
                    display.asyncExec(this);
                } else {
                    // Fallback to immediate update when there is no display (like in tests)
                    start -= DELAY + 1;
                    run();
                }
            }
        }
    }

    @Inject
    public SceneModel(IModelListener listener, IOperationHistory history, IUndoContext undoContext, IContainer contentRoot, ILoaderContext loaderContext, IImageProvider imageProvider) {
        this.listener = listener;
        this.history = history;
        this.undoContext = undoContext;
        this.contentRoot = contentRoot;
        this.loaderContext = loaderContext;
        this.imageProvider = imageProvider;

        this.selection = new StructuredSelection();
        this.undoRedoCounter = 0;
    }

    @Inject
    public void init() {
        this.history.addOperationHistoryListener(this);

    }

    public FontRendererHandle getDefaultFontRendererHandle() {

        if (this.defaultFontRendererHandle == null) {
            this.defaultFontRendererHandle = new FontRendererHandle();
            try {
                loadFont("/builtins/fonts/system_font.font", this.defaultFontRendererHandle);
            } catch (CoreException | IOException e) {
                logger.error("Could not load default font");
            }
        }

        if (this.defaultFontRendererHandle.isValid()) {
            return this.defaultFontRendererHandle;
        }
        return null;
    }


    public void dispose(GL2 gl) {
        if (this.root != null) {
            this.root.dispose(gl);
        }
        this.history.removeOperationHistoryListener(this);
        for (TextureHandle texture : this.textureCache.values()) {
            texture.clear(gl);
        }
        for (FontRendererHandle font : this.fontCache.values()) {
            font.clear(gl);
        }
        if (this.defaultFontRendererHandle != null) {
            this.defaultFontRendererHandle.clear(gl);
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getRoot()
     */
    @Override
    public Node getRoot() {
        return this.root;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#setRoot(com.dynamo.cr.sceneed.core.Node)
     */
    @Override
    public void setRoot(Node root) {
        if (this.root != root) {
            this.root = root;
            if (root != null) {
                root.setModel(this);
                this.root.updateStatus();
            }
            if (root != null) {
                setSelection(new StructuredSelection(this.root));
            } else {
                setSelection(new StructuredSelection());
            }
            this.listener.rootChanged(root);
            clearDirty();
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getSelection()
     */
    @Override
    public IStructuredSelection getSelection() {
        return this.selection;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#setSelection(org.eclipse.jface.viewers.IStructuredSelection)
     */
    @Override
    public void setSelection(IStructuredSelection selection) {
        this.selection = selection;
        this.listener.stateChanging(selection);
    }

    private void notifyChange(boolean delay) {
        this.listener.stateChanging(this.selection);
        this.delayedUpdateStatus.update(delay);
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#executeOperation(org.eclipse.core.commands.operations.IUndoableOperation)
     */
    @Override
    public void executeOperation(IUndoableOperation operation) {
        operation.addContext(this.undoContext);
        IStatus status = null;
        try {
            status = this.history.execute(operation, null, null);
            if (status != Status.OK_STATUS) {
                logger.error("Failed to execute operation", status.getException());
            }
        } catch (final ExecutionException e) {
            logger.error("Failed to execute operation", e);
        }

    }

    @Override
    public Object getAdapter(@SuppressWarnings("rawtypes") Class adapter) {
        if (adapter == IPropertyModel.class) {
            return new PropertyIntrospectorModel<SceneModel, SceneModel>(this, this, introspector);
        }
        return null;
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getContentRoot()
     */
    @Override
    public IContainer getContentRoot() {
        return this.contentRoot;
    }

    @Override
    public boolean isDirty() {
        return this.undoRedoCounter != 0;
    }

    @Override
    public void clearDirty() {
        if (this.undoRedoCounter != 0) {
            this.undoRedoCounter = 0;
            notifyChange(false);
        }
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }
        boolean change = false;
        int type = event.getEventType();
        boolean delay = false;
        switch (type) {
        case OperationHistoryEvent.DONE:
        case OperationHistoryEvent.REDONE:
            change = true;
            if (!(event.getOperation() instanceof SelectOperation)) {
                ++this.undoRedoCounter;
            }
            break;
        case OperationHistoryEvent.UNDONE:
            change = true;
            if (!(event.getOperation() instanceof SelectOperation)) {
                --this.undoRedoCounter;
            }
            break;
        case OperationHistoryEvent.OPERATION_CHANGED:
            change = true;
            delay = true;
            break;
        }
        if (change && this.root != null) {
            notifyChange(delay);
        }
    }

    private class ResourceDeltaVisitor implements IResourceDeltaVisitor {
        private boolean reloaded;

        public ResourceDeltaVisitor() {
            this.reloaded = false;
        }

        public boolean isReloaded() {
            return this.reloaded;
        }

        @Override
        public boolean visit(IResourceDelta delta) throws CoreException {
            IResource resource = delta.getResource();

            if (resource instanceof IFile) {
                String path = EditorUtil.makeResourcePath(contentRoot, resource);
                // Remove from cache so we guarantee reload
                if (imageCache.containsKey(path)) {
                    imageCache.remove(path);
                    // We can't remove texture here as it needs active GLU to dispose the texture
                    TextureHandle texture = textureCache.get(path);
                    if (texture != null) {
                        // Clearing image means texture will be disposed at next access
                        texture.setImage(null);
                    }

                }

                if (fontCache.containsKey(path)) {
                    FontRendererHandle font = fontCache.get(path);
                    fontCache.remove(path);
                    fontCache.put(path, null);

                    // Need to wait to clear font, needs active GL to dispose of texture
                    if (font != null) {
                        font.setDeferredClear();
                    }
                }

                if (loaderContext != null) {
                    loaderContext.removeFromCache(path);
                }
                if (handleReload(root, (IFile)resource)) {
                    this.reloaded = true;
                }
                return false;
            }
            return true;
        }

        private boolean handleReload(Node node, IFile file) {
            boolean reloaded = false;
            for (Node child : node.getChildren()) {
                if (handleReload(child, file)) {
                    reloaded = true;
                }
            }
            if (node.handleReload(file, reloaded)) {
                reloaded = true;
            }
            return reloaded;
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#handleResourceChanged(org.eclipse.core.resources.IResourceChangeEvent)
     */
    @Override
    public void handleResourceChanged(IResourceChangeEvent event) throws CoreException {
        if (this.root != null) {
            ResourceDeltaVisitor visitor = new ResourceDeltaVisitor();
            event.getDelta().accept(visitor);
            if (visitor.isReloaded()) {
                notifyChange(false);
            }
        }
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#getFile(java.lang.String)
     */
    @Override
    public IFile getFile(String path) {
        return this.contentRoot.getFile(new Path(path));
    }

    /* (non-Javadoc)
     * @see com.dynamo.cr.sceneed.core.ISceneModel#loadNode(java.lang.String)
     */
    @Override
    public Node loadNode(String path) throws IOException, CoreException {
        return this.loaderContext.loadNode(path);
    }

    @Override
    public String getExtension(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.loaderContext.getNodeTypeRegistry().getNodeTypeClass(nodeClass);
        if (nodeType != null) {
            return nodeType.getExtension();
        }
        return null;
    }

    @Override
    public String getTypeName(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.loaderContext.getNodeTypeRegistry().getNodeTypeClass(nodeClass);
        if (nodeType != null) {
            return nodeType.getResourceType().getName();
        }
        return null;
    }

    @Override
    public Image getIcon(Class<? extends Node> nodeClass) {
        String extension = getExtension(nodeClass);
        return this.imageProvider.getIconByExtension(extension);
    }

    @Override
    public INodeLoader<Node> getNodeLoader(Class<? extends Node> nodeClass) {
        INodeType nodeType = this.loaderContext.getNodeTypeRegistry().getNodeTypeClass(nodeClass);
        if (nodeType != null) {
            return nodeType.getLoader();
        }
        return null;
    }

    @Override
    public ILoaderContext getLoaderContext() {
        return this.loaderContext;
    }

    @Override
    public INodeType getNodeType(Class<? extends Node> nodeClass) {
        return this.loaderContext.getNodeTypeRegistry().getNodeTypeClass(nodeClass);
    }

    @Override
    public BufferedImage getImage(String path) {
        BufferedImage image = this.imageCache.get(path);
        if (image == null) {
            IFile file = getFile(path);
            try {
                image = SceneUtil.loadImage(file);
                if (image != null) {
                    this.imageCache.put(path, image);
                    TextureHandle texture = this.textureCache.get(path);
                    if (texture != null) {
                        texture.setImage(image);
                    }
                }
            } catch (Exception e) {
                logger.error("Failed to create image", e);
            }
        }
        return image;
    }

    @Override
    public TextureHandle getTexture(String path) {
        TextureHandle texture = this.textureCache.get(path);
        if (texture == null) {
            texture = new TextureHandle();
            BufferedImage image = getImage(path);
            texture.setImage(image);
            this.textureCache.put(path, texture);
        }
        return texture;
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
        final IFile fontInputFile = contentRoot.getFile(new Path(fontDesc.getFont()));
        final String searchPath = new Path(fontDesc.getFont()).removeLastSegments(1).toString();
        BufferedImage image;
        Fontc fontc = new Fontc();

        try {
            image = fontc.compile(fontInputFile.getContents(), fontDesc, true, new FontResourceResolver() {

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

        handle.setFont(fontc.getFontMap(), image, fontc.getInputFormat());

    }

    @Override
    public FontRendererHandle getFont(String path) {
        FontRendererHandle font = this.fontCache.get(path);
        if (font == null) {
            font = new FontRendererHandle();
            this.fontCache.put(path, font);

            try {
                loadFont(path, font);
            } catch (CoreException | IOException e) {
                logger.error("Could not load font " + path, e);
            }

        }
        return font;
    }

    @Override
    public ProjectProperties getProjectProperties() {
        return this.projectProperties;
    }
}
