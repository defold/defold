package com.dynamo.cr.projed;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.text.ParseException;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.runtime.CoreException;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.NullProgressMonitor;
import org.eclipse.jface.dialogs.MessageDialog;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.ui.IActionBars;
import org.eclipse.ui.IEditorInput;
import org.eclipse.ui.IEditorSite;
import org.eclipse.ui.IFileEditorInput;
import org.eclipse.ui.PartInitException;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.actions.ActionFactory;
import org.eclipse.ui.operations.RedoActionHandler;
import org.eclipse.ui.operations.UndoActionHandler;
import org.eclipse.ui.part.EditorPart;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class ProjectEditor extends EditorPart implements IProjectEditor {

    protected static Logger logger = LoggerFactory.getLogger(ProjectEditor.class);
    private ProjectEditorView view;
    private ProjectEditorPresenter presenter;
    private UndoActionHandler undoAction;
    private RedoActionHandler redoAction;

    public ProjectEditor() {
    }

    @Override
    public void dispose() {
        super.dispose();
        if (this.presenter != null) {
            presenter.dispose();
        }
    }

    @Override
    public void doSave(IProgressMonitor monitor) {
        IFileEditorInput input = (IFileEditorInput) getEditorInput();
        try {
            ByteArrayOutputStream os = new ByteArrayOutputStream();
            presenter.save(os);
            os.close();
            InputStream is = new ByteArrayInputStream(os.toByteArray());
            input.getFile().setContents(is, true, false, new NullProgressMonitor());
            presenter.clearDirty();
            updateDirty();
        } catch (CoreException e) {
            MessageDialog.openError(getSite().getShell(), "Error saving project file", e.getMessage());
            logger.error("Error saving project file", e);
        } catch (IOException e) {
            MessageDialog.openError(getSite().getShell(), "Error saving project file", e.getMessage());
            logger.error("Error saving project file", e);
        }
    }

    @Override
    public void doSaveAs() {
    }

    @Override
    public void updateDirty() {
        firePropertyChange(PROP_DIRTY);
    }

    @Override
    public void init(IEditorSite site, IEditorInput input)
            throws PartInitException {
        setSite(site);
        setInput(input);
        setPartName(input.getName());

        presenter = new ProjectEditorPresenter(this, PlatformUI.getWorkbench().getOperationSupport().getOperationHistory());
        IFileEditorInput fileInput = (IFileEditorInput) input;
        InputStream is = null;
        try {
            is = fileInput.getFile().getContents();
            presenter.load(is);
        } catch (CoreException e) {
            throw new RuntimeException(e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        } catch (ParseException e) {
            throw new RuntimeException(e);
        } finally {
            IOUtils.closeQuietly(is);
        }

        this.undoAction = new UndoActionHandler(this.getEditorSite(), presenter.getUndoContext());
        this.redoAction = new RedoActionHandler(this.getEditorSite(), presenter.getUndoContext());
    }

    public void updateActions() {
        IActionBars actionBars = getEditorSite().getActionBars();
        actionBars.updateActionBars();
        actionBars.setGlobalActionHandler(ActionFactory.UNDO.getId(), undoAction);
        actionBars.setGlobalActionHandler(ActionFactory.REDO.getId(), redoAction);
    }

    @Override
    public boolean isDirty() {
        return presenter.isDirty();
    }

    @Override
    public boolean isSaveAsAllowed() {
        return false;
    }

    @Override
    public void createPartControl(Composite parent) {
        view = new ProjectEditorView(presenter);
        view.createPartControl(parent, Meta.getDefaultMeta());
        presenter.setView(view);
        presenter.refreshAll();
    }

    @Override
    public void setFocus() {
        view.setFocus();
        updateActions();
    }
}
