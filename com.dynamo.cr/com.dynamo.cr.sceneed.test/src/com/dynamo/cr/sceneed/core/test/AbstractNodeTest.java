package com.dynamo.cr.sceneed.core.test;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.awt.image.BufferedImage;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.net.URL;
import java.util.Enumeration;
import java.util.HashMap;
import java.util.Map;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.DefaultOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoContext;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IPath;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.core.runtime.Path;
import org.eclipse.jface.viewers.IStructuredSelection;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.swt.widgets.Display;
import org.junit.Before;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.osgi.framework.Bundle;
import org.osgi.framework.FrameworkUtil;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.properties.IPropertyModel;
import com.dynamo.cr.sceneed.core.ILoaderContext;
import com.dynamo.cr.sceneed.core.INodeLoader;
import com.dynamo.cr.sceneed.core.INodeTypeRegistry;
import com.dynamo.cr.sceneed.core.ISceneModel;
import com.dynamo.cr.sceneed.core.ISceneView;
import com.dynamo.cr.sceneed.core.ISceneView.IPresenterContext;
import com.dynamo.cr.sceneed.core.Node;
import com.dynamo.cr.sceneed.core.SceneUtil;
import com.google.protobuf.Message;
import com.google.protobuf.TextFormat;

public abstract class AbstractNodeTest {

    private IContainer contentRoot;
    private ISceneModel model;
    private ILoaderContext loaderContext;
    private IPresenterContext presenterContext;
    private IOperationHistory history;
    private IUndoContext undoContext;
    private INodeTypeRegistry nodeTypeRegistry;
    private IResourceTypeRegistry resourceTypeRegistry;
    private Node root;

    private Map<String, IFile> fileStore;

    private int selectionCount;
    private int executionCount;

    @Before
    public void setup() throws CoreException, IOException {
        // Avoid hang when running unit-test on Mac OSX
        // Related to SWT and threads?
        if (System.getProperty("os.name").toLowerCase().indexOf("mac") != -1) {
            Display.getDefault();
        }

        this.contentRoot = mock(IContainer.class);

        this.model = mock(ISceneModel.class);
        when(this.model.getContentRoot()).thenReturn(this.contentRoot);
        doAnswer(new Answer<Object>() {
            @Override
            public Object answer(InvocationOnMock invocation) throws Throwable {
                root = (Node)invocation.getArguments()[0];
                root.setModel(model);
                root.updateStatus();
                return null;
            }
        }).when(this.model).setRoot(any(Node.class));
        doAnswer(new Answer<BufferedImage>() {
            @Override
            public BufferedImage answer(InvocationOnMock invocation)
                    throws Throwable {
                return SceneUtil.loadImage(getFile((String) invocation
                        .getArguments()[0]));
            }
        }).when(this.model).getImage(any(String.class));

        this.nodeTypeRegistry = mock(INodeTypeRegistry.class);

        this.loaderContext = mock(ILoaderContext.class);
        when(this.loaderContext.getNodeTypeRegistry()).thenReturn(this.nodeTypeRegistry);

        this.resourceTypeRegistry = EditorCorePlugin.getDefault();

        this.presenterContext = mock(ISceneView.IPresenterContext.class);

        this.history = new DefaultOperationHistory();
        this.history.addOperationHistoryListener(new IOperationHistoryListener() {
            @Override
            public void historyNotification(OperationHistoryEvent event) {
                switch (event.getEventType()) {
                case OperationHistoryEvent.DONE:
                case OperationHistoryEvent.UNDONE:
                case OperationHistoryEvent.REDONE:
                    root.updateStatus();
                    break;
                }
            }
        });
        this.undoContext = new UndoContext();

        setupFileStore();

        this.selectionCount = 0;
        this.executionCount = 0;
    }

    private void setupFileStore() throws CoreException {
        this.fileStore = new HashMap<String, IFile>();

        Bundle bundle = FrameworkUtil.getBundle(getClass());
        Enumeration<URL> entries = bundle.findEntries("/test", "*", true);
        if (entries != null) {
            while (entries.hasMoreElements()) {
                final URL url = entries.nextElement();
                IPath path = new Path(url.getPath()).removeFirstSegments(1);
                IFile file = mock(IFile.class);
                when(file.exists()).thenReturn(true);
                when(file.getContents()).thenAnswer(new Answer<InputStream>() {
                    @Override
                    public InputStream answer(InvocationOnMock invocation) throws Throwable {
                        return url.openStream();
                    }
                });
                // Fake a "project relative" absolute path, i.e. foo.bar => /foo.bar
                String p = "/" + path.toString();
                this.fileStore.put(p, file);
            }
        }

        final IFile missingFile = mock(IFile.class);
        when(missingFile.exists()).thenReturn(false);
        when(missingFile.getContents()).thenReturn(new ByteArrayInputStream(new byte[0]));

        Answer<IFile> fileAnswer = new Answer<IFile>() {
            @Override
            public IFile answer(InvocationOnMock invocation) throws Throwable {
                Object arg = invocation.getArguments()[0];
                String path = arg.toString();
                IFile file = fileStore.get(path);
                if (file != null) {
                    return file;
                }
                return missingFile;
            }
        };
        when(getModel().getFile(anyString())).thenAnswer(fileAnswer);
        when(getContentRoot().getFile(any(IPath.class))).thenAnswer(fileAnswer);
    }

    // Accessors

    protected IContainer getContentRoot() {
        return this.contentRoot;
    }

    protected ISceneModel getModel() {
        return this.model;
    }

    protected INodeTypeRegistry getNodeTypeRegistry() {
        return this.nodeTypeRegistry;
    }

    protected void setResourceTypeRegistry(IResourceTypeRegistry resourceTypeRegistry) {
        this.resourceTypeRegistry = resourceTypeRegistry;
    }

    protected ILoaderContext getLoaderContext() {
        return this.loaderContext;
    }

    protected ISceneView.IPresenterContext getPresenterContext() {
        return this.presenterContext;
    }

    // Helpers

    protected void execute(IUndoableOperation operation) throws ExecutionException {
        operation.addContext(this.undoContext);
        this.history.execute(operation, null, null);
    }

    protected void undo() throws ExecutionException {
        this.history.undo(this.undoContext, null, null);
    }

    protected void redo() throws ExecutionException {
        this.history.redo(this.undoContext, null, null);
    }

    protected void verifySelection() {
        ++this.selectionCount;
        verify(this.presenterContext, times(this.selectionCount)).setSelection(any(IStructuredSelection.class));
    }

    protected void verifyExecution() {
        ++this.executionCount;
        verify(this.presenterContext, times(this.executionCount)).executeOperation(any(IUndoableOperation.class));
    }

    protected Object getNodeProperty(Node node, Object id) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.getPropertyValue(id);
    }

    protected void setNodeProperty(Node node, Object id, Object value) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        execute(propertyModel.setPropertyValue(id, value));
    }

    protected boolean isNodePropertyOverridden(Node node, Object id) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.isPropertyOverridden(id);
    }

    protected void resetNodeProperty(Node node, Object id) throws ExecutionException {
        @SuppressWarnings("unchecked")
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        propertyModel.resetPropertyValue(id);
    }

    @SuppressWarnings("unchecked")
    protected IStatus getNodePropertyStatus(Node node, Object id) {
        IPropertyModel<? extends Node, ISceneModel> propertyModel = (IPropertyModel<? extends Node, ISceneModel>)node.getAdapter(IPropertyModel.class);
        return propertyModel.getPropertyStatus(id);
    }

    protected void assertNodePropertyStatus(Node node, Object id, int severity, String message) {
        IStatus status = getNodePropertyStatus(node, id);
        assertStatus(status, severity, message);
    }

    protected void assertNodeStatus(Node node, int severity, String message) {
        assertStatus(node.getStatus(), severity, message);
    }

    protected void assertStatus(IStatus status, int severity, String message) {
        boolean found = containsStatus(status, severity, message);
        if (!found) {
            String error;
            if (message != null) {
                error = String.format("Status with severity %d and message '%s' not found in %s", severity, message, status.toString());
            } else {
                error = String.format("Status with severity %d not found in %s", severity, status.toString());
            }
            assertTrue(error, false);
        }
    }

    private boolean containsStatus(IStatus status, int severity, String message) {
        if (status.isMultiStatus()) {
            for (IStatus child : status.getChildren()) {
                if (containsStatus(child, severity, message)) {
                    return true;
                }
            }
            return false;
        } else {
            return status.getSeverity() == severity && (message == null || message.equals(status.getMessage()));
        }
    }

    /**
     * Retrieve a file from the file system, virtual (see registerFile) or read from disk (/test under current test-bundle)
     * @param path Path of file, e.g. /foo.bar
     * @return
     */
    protected IFile getFile(String path) {
        return this.fileStore.get(path);
    }

    /**
     * Register a virtual file to the file system
     * @param path Path of file, e.g. /foo.bar
     * @param contents Contents of the file
     * @throws CoreException
     */
    protected void registerFile(String path, final String contents) throws CoreException {
        IFile file = mock(IFile.class);
        when(file.exists()).thenReturn(true);
        when(file.getContents()).thenAnswer(new Answer<InputStream>() {
            @Override
            public InputStream answer(InvocationOnMock invocation) throws Throwable {
                return new ByteArrayInputStream(contents.getBytes());
            }
        });
        this.fileStore.put(path, file);
    }

    /**
     * Simulate a copy of the source file to the destination file
     * @param source Path of source file, e.g. /foo.bar
     * @param destination Path of destination file, e.g. /foo2.bar
     * @return A reference of the destination file
     * @throws CoreException
     */
    protected IFile copyFile(String source, String destination) throws CoreException {
        IFile destinationFile = getFile(destination);
        final IFile sourceFile = getFile(source);
        when(destinationFile.getContents()).thenAnswer(new Answer<InputStream>() {
            @Override
            public InputStream answer(InvocationOnMock invocation) throws Throwable {
                return sourceFile.getContents();
            }
        });
        return destinationFile;
    }

    protected <T extends Node> T registerAndLoadRoot(Class<T> nodeClass, String extension, INodeLoader<T> loader) throws IOException, CoreException {
        when(getModel().getExtension(nodeClass)).thenReturn(extension);
        IResourceType resourceType = this.resourceTypeRegistry.getResourceTypeFromExtension(extension);
        byte[] ddf = resourceType.getTemplateData();
        T node = loader.load(getLoaderContext(), new ByteArrayInputStream(ddf));
        this.model.setRoot(node);
        when(this.presenterContext.getSelection()).thenReturn(new StructuredSelection(node));
        return node;
    }

    protected void registerNodeType(Class<? extends Node> nodeClass, String extension) throws IOException, CoreException {
        when(getModel().getExtension(nodeClass)).thenReturn(extension);
    }

    protected void registerLoadedNode(String path, Node node) throws IOException, CoreException {
        when(getModel().loadNode(path)).thenReturn(node);
    }

    protected <T extends Node> void saveLoadCompare(INodeLoader<T> loader, Message.Builder builder, String path) throws Exception {
        IFile file = getFile(path);
        Reader reader = new InputStreamReader(file.getContents());
        TextFormat.merge(reader, builder);
        reader.close();
        Message directMessage = builder.build();

        T node = loader.load(this.loaderContext, file.getContents());
        Message createdMessage = loader.buildMessage(this.loaderContext, node, new NullProgressMonitor());
        assertEquals(directMessage.toString(), createdMessage.toString());
    }

}
