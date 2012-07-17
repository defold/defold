package com.dynamo.cr.ddfeditor;

import java.io.ByteArrayInputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.lang.reflect.Method;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.IOperationApprover;
import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.IOperationHistoryListener;
import org.eclipse.core.commands.operations.IUndoableOperation;
import org.eclipse.core.commands.operations.OperationHistoryEvent;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IContainer;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.swt.SWT;
import org.eclipse.swt.custom.SashForm;
import org.eclipse.swt.graphics.Image;
import org.eclipse.swt.layout.FillLayout;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.forms.widgets.Form;
import org.eclipse.ui.forms.widgets.FormToolkit;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.eclipse.ui.statushandlers.StatusManager;

import com.dynamo.cr.editor.core.EditorCorePlugin;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IResourceType;
import com.dynamo.cr.editor.core.IResourceTypeRegistry;
import com.dynamo.cr.protobind.MessageNode;
import com.google.protobuf.GeneratedMessage;
import com.google.protobuf.GeneratedMessage.Builder;
import com.google.protobuf.TextFormat;

public abstract class DdfEditor extends EditorPart implements IOperationHistoryListener, IResourceChangeListener {

    private MessageNode message;
    private UndoContext undoContext;
    private UndoActionHandler undoAction;
    private RedoActionHandler redoAction;
    private int cleanUndoStackDepth = 0;
    private IResourceType resourceType;
    private ProtoTreeEditor protoTreeEditor;
    private IContainer contentRoot;
    private Form form;
    private IOperationHistory history;
    private boolean inSave = false;

    public DdfEditor(String extension) {
        IResourceTypeRegistry regist = EditorCorePlugin.getDefault().getResourceTypeRegistry();
        this.resourceType = regist.getResourceTypeFromExtension(extension);
        if (this.resourceType == null)
            throw new RuntimeException("Missing resource type for: " + extension);
    }

    @Override
    public void dispose() {
        super.dispose();
        if (history != null) {
            history.removeOperationHistoryListener(this);
        }
        ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
    }

    public IContainer getContentRoot() {
        return contentRoot;
    }

    public MessageNode getMessage() {
        return message;
    }

    public void executeOperation(IUndoableOperation operation) {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        operation.addContext(undoContext);
        try {
            history.execute(operation, null, null);
        } catch (ExecutionException e) {
            Activator.logException(e);
        }
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        this.inSave = true;
        try {
            IFileEditorInput input = (IFileEditorInput) getEditorInput();

            String messageString = TextFormat.printToString(message.build());
            ByteArrayInputStream stream = new ByteArrayInputStream(messageString.getBytes());

            try {
                input.getFile().setContents(stream, false, true, monitor);
                IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
                cleanUndoStackDepth = history.getUndoHistory(undoContext).length;
                firePropertyChange(PROP_DIRTY);
            } catch (CoreException e) {
                Status status = new Status(IStatus.ERROR, Activator.PLUGIN_ID, e.getMessage(), e);
                StatusManager.getManager().handle(status, StatusManager.SHOW);
            }
        }
       finally {
           this.inSave = false;
       }
    }

    @Override
    public void doSaveAs() {
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {

        ResourcesPlugin.getWorkspace().addResourceChangeListener(this);
        setSite(site);
        setInput(input);
        setPartName(input.getName());

        IFileEditorInput i = (IFileEditorInput) input;
        IFile file = i.getFile();

        this.contentRoot = EditorUtil.findContentRoot(file);

        try {
            Reader reader = new InputStreamReader(file.getContents());

            try {
                Method m = this.resourceType.getMessageClass().getDeclaredMethod("newBuilder");
                @SuppressWarnings("rawtypes")
                GeneratedMessage.Builder builder = (Builder) m.invoke(null);

                try {
                    TextFormat.merge(reader, builder);

                    MessageNode message = new MessageNode(builder.build());
                    this.message = message;
                } finally {
                    reader.close();
                }
            } catch (Throwable e) {
                throw new PartInitException(e.getMessage(), e);
            }

        } catch (CoreException e) {
            throw new PartInitException(e.getMessage(), e);
        }

        this.undoContext = new UndoContext();
        history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        history.setLimit(this.undoContext, 100);
        history.addOperationHistoryListener(this);

        @SuppressWarnings("unused")
        IOperationApprover approver = new LinearUndoViolationUserApprover(this.undoContext, this);

        this.undoAction = new UndoActionHandler(this.getEditorSite(), this.undoContext);
        this.redoAction = new RedoActionHandler(this.getEditorSite(), this.undoContext);
    }

    @Override
    public boolean isDirty() {
        IOperationHistory history = PlatformUI.getWorkbench().getOperationSupport().getOperationHistory();
        return history.getUndoHistory(undoContext).length != cleanUndoStackDepth;
    }

    @Override
    public boolean isSaveAsAllowed() {
        return false;
    }

    public void createPreviewControl(Composite parent) {

    }

    public void updatePreview() {
    }

    @Override
    public void createPartControl(Composite parent) {
        FormToolkit toolkit = new FormToolkit(parent.getDisplay());
        this.form = toolkit.createForm(parent);

        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        Image image = Activator.getDefault().getImage(input.getName());
        form.setImage(image);
        form.setText(this.resourceType.getName());
        toolkit.decorateFormHeading(form);
        form.getBody().setLayout(new FillLayout());

        SashForm splitter = new SashForm(form.getBody(), SWT.BORDER | SWT.VERTICAL);

        protoTreeEditor = new ProtoTreeEditor(splitter, toolkit, this.contentRoot, this.undoContext);
        createPreviewControl(splitter);

        protoTreeEditor.setInput(message, resourceType);
        updatePreview();
    }

    @Override
    public void setFocus() {
        this.form.getBody().setFocus();
    }

    public void updateActions() {
        IActionBars action_bars = getEditorSite().getActionBars();
        action_bars.updateActionBars();

        action_bars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoAction);
        action_bars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoAction);
    }

    @Override
    public void historyNotification(OperationHistoryEvent event) {
        if (!event.getOperation().hasContext(this.undoContext)) {
            // Only handle operations related to this editor
            return;
        }

        final int type = event.getEventType();
        Display display = Display.getDefault();
        display.asyncExec(new Runnable() {
            @Override
            public void run() {
                firePropertyChange(PROP_DIRTY);
                if (type == OperationHistoryEvent.DONE || type == OperationHistoryEvent.UNDONE || type == OperationHistoryEvent.REDONE) {
                    updatePreview();
                }
            }
        });
    }

    @Override
    public void resourceChanged(IResourceChangeEvent event) {
        if (this.inSave)
            return;

        final IFileEditorInput input = (IFileEditorInput) getEditorInput();
        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    if ((delta.getKind() & IResourceDelta.REMOVED) == IResourceDelta.REMOVED) {
                        IResource resource = delta.getResource();
                        if (resource.equals(input.getFile())) {
                            getSite().getShell().getDisplay().asyncExec(new Runnable() {
                                @Override
                                public void run() {
                                    getSite().getPage().closeEditor(DdfEditor.this, false);
                                }
                            });
                            return false;
                        }
                    } else if ((delta.getKind() & IResourceDelta.CHANGED) == IResourceDelta.CHANGED
                            && (delta.getFlags() & IResourceDelta.CONTENT) == IResourceDelta.CONTENT) {
                        IResource resource = delta.getResource();
                        if (resource.equals(input.getFile())) {
                            try {
                                Reader reader = new InputStreamReader(input.getFile().getContents());

                                try {
                                    Method m = resourceType.getMessageClass().getDeclaredMethod("newBuilder");
                                    @SuppressWarnings("rawtypes")
                                    GeneratedMessage.Builder builder = (Builder) m.invoke(null);

                                    // NOTE: Budget solution to clear undo history. Couldn't find a better solution...
                                    history.setLimit(undoContext, 0);
                                    history.setLimit(undoContext, 100);

                                    cleanUndoStackDepth = 0;

                                    try {
                                        TextFormat.merge(reader, builder);

                                        MessageNode msg = new MessageNode(builder.build());
                                        message = msg;
                                        getSite().getShell().getDisplay().asyncExec(new Runnable() {
                                            @Override
                                            public void run() {
                                                protoTreeEditor.setInput(message, resourceType);
                                            }
                                        });
                                    } finally {
                                        reader.close();
                                    }
                                } catch (Throwable e) {
                                    throw new PartInitException(e.getMessage(), e);
                                }

                            } catch (CoreException e) {
                                throw new PartInitException(e.getMessage(), e);
                            }
                        }
                    }
                    return true;
                }
            });
        } catch (CoreException e) {
            Activator.logException(e);
        }
    }
}
