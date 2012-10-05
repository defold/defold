package com.dynamo.cr.editor.ui;

import org.eclipse.core.commands.operations.IOperationHistory;
import org.eclipse.core.commands.operations.UndoContext;
import org.eclipse.core.resources.IFile;
import org.eclipse.core.resources.IResource;
import org.eclipse.core.resources.IResourceChangeEvent;
import org.eclipse.core.resources.IResourceChangeListener;
import org.eclipse.core.resources.IResourceDelta;
import org.eclipse.core.resources.IResourceDeltaVisitor;
import org.eclipse.core.resources.ResourcesPlugin;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Display;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.IPartListener;
import org.eclipse.ui.IPartService;
import org.eclipse.ui.IWindowListener;
import org.eclipse.ui.IWorkbenchPart;
import org.eclipse.ui.IWorkbenchWindow;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.LinearUndoViolationUserApprover;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.ProjectProperties;

public abstract class AbstractDefoldEditor extends EditorPart {

    // Named abstractLogger due to abstract class
    protected static Logger abstractLogger = LoggerFactory.getLogger(AbstractDefoldEditor.class);
    private long lastModificationStamp;
    private ActivationListener activationListener;
    protected UndoContext undoContext;
    protected IOperationHistory history;
    protected LinearUndoViolationUserApprover approver;
    protected UndoActionHandler undoHandler;
    protected RedoActionHandler redoHandler;
    protected boolean inSave = false;
    private ResourceChangedListener resourceChangeListener;
    private final static int UNDO_LIMIT = 100;

    @Override
    public void dispose() {
        super.dispose();

        if (this.history != null)
        {
            this.history.dispose(this.undoContext, true, true, true);
            // Better be safe than sorry. We set the limit to zero as well :-)
            this.history.setLimit(this.undoContext, 0);

            if (this.approver != null)
            {
                this.history.removeOperationApprover(this.approver);
            }
        }

        if (this.activationListener != null)
            this.activationListener.dispose();

        if (this.resourceChangeListener != null)
            this.resourceChangeListener.dispose();
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {

        setSite(site);
        setInput(input);
        setPartName(input.getName());

        if (input instanceof IFileEditorInput) {
            IFileEditorInput fileInput = (IFileEditorInput)input;
            this.lastModificationStamp = fileInput.getFile().getModificationStamp();
        }

        this.activationListener = new ActivationListener(getSite().getWorkbenchWindow().getPartService());
        this.resourceChangeListener = new ResourceChangedListener();

        this.undoContext = new UndoContext();
        this.history = PlatformUI.getWorkbench().getOperationSupport()
                .getOperationHistory();
        this.history.setLimit(this.undoContext, UNDO_LIMIT);

        approver = new LinearUndoViolationUserApprover(this.undoContext, this);
        this.history.addOperationApprover(approver);

        undoHandler = new UndoActionHandler(this.getEditorSite(), undoContext);
        redoHandler = new RedoActionHandler(this.getEditorSite(), undoContext);
    }

    public void updateActions() {
        IActionBars actionBars = this.getEditorSite().getActionBars();
        actionBars.updateActionBars();

        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), this.undoHandler);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), this.redoHandler);
    }

    private void checkFileState() {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        IFile file = input.getFile();

        // We must retrieve this before refreshLocal below.
        boolean isSynchronized = file.isSynchronized(IFile.DEPTH_ZERO);

        try {
            // Refresh if changed externally
            file.refreshLocal(IFile.DEPTH_ZERO, new NullProgressMonitor());
        } catch (CoreException e) {
            abstractLogger.error("Error occurred while refreshing files", e);
            // We continue here. Not much todo if we an error here.
        }

        if (!file.exists()) {
            Display display= getSite().getShell().getDisplay();
            display.asyncExec(new Runnable() {
                @Override
                public void run() {
                    getSite().getPage().closeEditor(AbstractDefoldEditor.this, false);
                }
            });

        }
        else if (file.getModificationStamp() != lastModificationStamp || !isSynchronized) {
            // The file can be changed by an editor within eclipse or externally

            // Get new modification time stamp
            lastModificationStamp = file.getModificationStamp();

            boolean reload;

            if (isDirty()) {
                String msg = String.format("The file '%s' has been changed on the file system. Do you want to replace the editor contents with these changes?", file.getName());
                reload = MessageDialog.openQuestion(getSite().getShell(), "File Changed", msg);
            } else {
                // Always reload if not dirty
                reload = true;
            }

            if (reload) {
                // Clear undo stack
                this.history.setLimit(this.undoContext, 0);
                this.history.setLimit(this.undoContext, UNDO_LIMIT);

                doReload(file);
            }
        }
    }


    protected void handleReloadResourceChanged(IResourceChangeEvent event) {
        IFileEditorInput fileEditorInput = (IFileEditorInput) getEditorInput();
        final IFile file = fileEditorInput.getFile();
        final IFile gameProject = EditorUtil.findGameProjectFile(file.getParent());

        try {
            event.getDelta().accept(new IResourceDeltaVisitor() {

                @Override
                public boolean visit(IResourceDelta delta) throws CoreException {
                    IResource resource = delta.getResource();
                    if (file.equals(resource)) {

                        checkFileState();

                        return false;
                    } else if (resource.equals(gameProject)) {
                        try {
                            ProjectProperties projectProperties = new ProjectProperties();
                            projectProperties.load(gameProject.getContents());
                            handleProjectPropertiesChanged(projectProperties);
                        } catch (Exception e) {
                            // Ignore errors, they should be displayed by the game-project editor
                        }
                        return false;
                    } else {
                        return true;
                    }
                }
            });
        } catch (CoreException e) {
            abstractLogger.error("Error occurred while refreshing files", e);
        }

    }

    protected abstract void doReload(IFile file);
    protected abstract void handleResourceChanged(IResourceChangeEvent event);

    protected void handleProjectPropertiesChanged(ProjectProperties properties) {

    }

    class ResourceChangedListener implements IResourceChangeListener {

        public ResourceChangedListener() {
            ResourcesPlugin.getWorkspace().addResourceChangeListener(this);
        }

        public void dispose() {
            ResourcesPlugin.getWorkspace().removeResourceChangeListener(this);
        }

        @Override
        public void resourceChanged(final IResourceChangeEvent event) {
            if (inSave) {
                IFileEditorInput fileEditorInput = (IFileEditorInput) getEditorInput();
                IFile file = fileEditorInput.getFile();
                lastModificationStamp = file.getModificationStamp();
            } else {
                Display display= getSite().getShell().getDisplay();
                display.asyncExec(new Runnable() {
                    @Override
                    public void run() {
                        handleReloadResourceChanged(event);
                        handleResourceChanged(event);
                    }
                });
            }
        }
    }

    class ActivationListener implements IPartListener, IWindowListener {

        private final IPartService partService;

        public ActivationListener(IPartService partService) {
            this.partService = partService;
            this.partService.addPartListener(this);
            PlatformUI.getWorkbench().addWindowListener(this);
        }

        public void dispose() {
            this.partService.removePartListener(this);
            PlatformUI.getWorkbench().removeWindowListener(this);
        }

        @Override
        public void windowActivated(IWorkbenchWindow window) {
            if (AbstractDefoldEditor.this == this.partService.getActivePart()) {
                checkFileState();
            }
        }

        @Override
        public void windowDeactivated(IWorkbenchWindow window) {}

        @Override
        public void windowClosed(IWorkbenchWindow window) {}

        @Override
        public void windowOpened(IWorkbenchWindow window) {}

        @Override
        public void partActivated(IWorkbenchPart part) {
            checkFileState();
        }

        @Override
        public void partBroughtToTop(IWorkbenchPart part) {}

        @Override
        public void partClosed(IWorkbenchPart part) {}

        @Override
        public void partDeactivated(IWorkbenchPart part) {}

        @Override
        public void partOpened(IWorkbenchPart part) {}
    }

}
