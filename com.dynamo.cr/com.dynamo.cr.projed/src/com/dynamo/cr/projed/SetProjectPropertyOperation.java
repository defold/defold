package com.dynamo.cr.projed;

import org.eclipse.core.commands.ExecutionException;
import org.eclipse.core.commands.operations.AbstractOperation;
import org.eclipse.core.runtime.IAdaptable;
import org.eclipse.core.runtime.IProgressMonitor;
import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;

import com.dynamo.cr.editor.core.ProjectProperties;

public class SetProjectPropertyOperation extends AbstractOperation {

    private ProjectProperties projectProperties;
    private String category;
    private String key;
    private String newValue;
    private String oldValue;
    private IProjectEditorView view;
    private KeyMeta keyMeta;

    public SetProjectPropertyOperation(String label, ProjectProperties projectProperties, KeyMeta keyMeta, String newValue, String oldValue, IProjectEditorView view) {
        super(label);
        this.projectProperties = projectProperties;
        this.keyMeta = keyMeta;
        this.category = keyMeta.getCategory().getName();
        this.key = keyMeta.getName();
        this.newValue = newValue;
        this.oldValue = oldValue;
        this.view = view;
    }

    @Override
    public IStatus execute(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        projectProperties.putStringValue(category, key, newValue);
        view.setValue(keyMeta, newValue);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus redo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        projectProperties.putStringValue(category, key, newValue);
        view.setValue(keyMeta, newValue);
        return Status.OK_STATUS;
    }

    @Override
    public IStatus undo(IProgressMonitor monitor, IAdaptable info)
            throws ExecutionException {
        projectProperties.putStringValue(category, key, oldValue);
        view.setValue(keyMeta, oldValue);
        return Status.OK_STATUS;
    }

}
